#ifdef ESP_PLATFORM

#include "ESP32Board.h"
#include <Arduino.h>
#include <cstring>
#include <helpers/HttpOtaDisplayState.h>
#include <helpers/RepeaterTcpOtaEmit.h>

volatile int g_meshcore_http_ota_display_active = 0;
volatile uint8_t g_meshcore_http_ota_display_pct = 0xFF;
char g_meshcore_http_ota_display_line[28] = {0};

#if (defined(ADMIN_PASSWORD) || defined(MULTI_TRANSPORT_COMPANION)) && !defined(DISABLE_WIFI_OTA)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <SPIFFS.h>

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_system.h>

static volatile bool s_http_ota_reboot_pending = false;
static bool s_http_ota_busy = false;

namespace {

struct HttpOtaBusyGuard {
  bool* flag;
  explicit HttpOtaBusyGuard(bool* f) : flag(f) { *flag = true; }
  ~HttpOtaBusyGuard() { *flag = false; }
};

static bool httpOtaExtractHost(const char* url, char* host, size_t cap) {
  const char* u = url;
  if (strncmp(u, "https://", 8) == 0)
    u += 8;
  else if (strncmp(u, "http://", 7) == 0)
    u += 7;
  else
    return false;
  size_t i = 0;
  while (u[i] && u[i] != '/' && u[i] != '?' && u[i] != '#' && u[i] != ':') {
    if (i + 1 >= cap) return false;
    host[i] = u[i];
    i++;
  }
  host[i] = '\0';
  return i > 0;
}

static bool httpOtaUrlLooksMergedBin(const char* url) {
  if (!url) return false;
  const char* marker = strstr(url, "-merged.bin");
  if (marker) return true;
  marker = strstr(url, "_merged.bin");
  return marker != nullptr;
}

/** Skip UTF-8 BOM and ASCII whitespace some proxies prepend before the raw .bin body. */
static size_t httpOtaLeadingJunkLen(const uint8_t* data, size_t len) {
  size_t i = 0;
  if (len >= 3 && data[0] == 0xEFu && data[1] == 0xBBu && data[2] == 0xBFu) i = 3;
  while (i < len && (data[i] == 0x20u || data[i] == 0x09u || data[i] == 0x0Au || data[i] == 0x0Du)) i++;
  return i;
}

/**
 * Read up to `cap` bytes until we see a valid ESP32 app image start (0xE9) after leading junk.
 * Updates *remaining_inout when Content-Length is known (body bytes consumed).
 * @return 1 if firmware prefix found; 0 if this mirror should be skipped (try next URL).
 */
static int httpOtaPeekEsp32ImagePrefix(WiFiClient* stream, HTTPClient& https, int* remaining_inout,
                                       uint8_t* prefix, size_t cap, size_t* out_len, size_t* out_skip) {
  size_t prefix_len = 0;
  unsigned long tp0 = millis();
  int& rem = *remaining_inout;

  while (prefix_len < cap && (millis() - tp0) < 30000UL) {
    if (!https.connected() && stream->available() == 0 && prefix_len > 0) break;

    size_t av = stream->available();
    if (!av) {
      if (rem == 0) break;
      if (rem < 0 && !https.connected()) break;
      delay(2);
      yield();
      continue;
    }

    size_t chunk = cap - prefix_len;
    if (chunk > av) chunk = av;
    int n = stream->readBytes(prefix + prefix_len, chunk);
    if (n <= 0) break;
    prefix_len += (size_t)n;
    if (rem > 0) rem -= n;

    size_t skip = httpOtaLeadingJunkLen(prefix, prefix_len);
    if (skip >= prefix_len) continue;

    uint8_t b = prefix[skip];
    if (b == (uint8_t)0xE9) {
      *out_len = prefix_len;
      *out_skip = skip;
      return 1;
    }
    if (b == 0x1Fu && skip + 1 < prefix_len && prefix[skip + 1] == 0x8Bu) {
      meshcoreRepeaterTcpOtaEmitLine("OTA: skip mirror (gzip)");
      return 0;
    }
    if (prefix_len - skip >= 4u) {
      if (!memcmp(prefix + skip, "<htm", 4) || !memcmp(prefix + skip, "<!DO", 4)) {
        meshcoreRepeaterTcpOtaEmitLine("OTA: skip mirror (html)");
        return 0;
      }
    }
    /* First non-junk byte is not image magic: fail once we have enough context. */
    if (prefix_len >= 24 || prefix_len - skip >= 2u) {
      char line[80];
      snprintf(line, sizeof(line), "OTA: diag first %02x %02x %02x %02x", prefix[skip],
               (skip + 1 < prefix_len) ? prefix[skip + 1] : 0u, (skip + 2 < prefix_len) ? prefix[skip + 2] : 0u,
               (skip + 3 < prefix_len) ? prefix[skip + 3] : 0u);
      meshcoreRepeaterTcpOtaEmitLine(line);
      meshcoreRepeaterTcpOtaEmitLine("OTA: skip mirror (not ESP bin)");
      return 0;
    }
  }

  if (prefix_len > 0) {
    char line[80];
    snprintf(line, sizeof(line), "OTA: diag first %02x %02x %02x %02x", prefix[0],
             prefix_len > 1 ? prefix[1] : 0u, prefix_len > 2 ? prefix[2] : 0u, prefix_len > 3 ? prefix[3] : 0u);
    meshcoreRepeaterTcpOtaEmitLine(line);
  }
  meshcoreRepeaterTcpOtaEmitLine("OTA: skip mirror (peek timeout)");
  return 0;
}

static bool httpOtaResponseLooksLikeFirmwareBody(HTTPClient& http) {
  if (!http.hasHeader("Content-Type")) return true;
  String ct = http.header("Content-Type");
  ct.toLowerCase();
  if (ct.indexOf("text/html") >= 0) return false;
  if (ct.indexOf("application/json") >= 0) return false;
  return true;
}

static void httpOtaEmitHostLookup(const char* url_label, const char* url) {
  char host[64];
  if (!httpOtaExtractHost(url, host, sizeof(host))) return;
  IPAddress ip;
  char line[112];
  if (WiFi.hostByName(host, ip)) {
    snprintf(line, sizeof(line), "OTA: diag %s %s -> %s", url_label, host, ip.toString().c_str());
  } else {
    snprintf(line, sizeof(line), "OTA: diag %s %s -> DNS fail", url_label, host);
  }
  meshcoreRepeaterTcpOtaEmitLine(line);
}

}  // namespace

static void httpOtaDisplayReset() {
  g_meshcore_http_ota_display_active = 0;
  g_meshcore_http_ota_display_pct = 0xFF;
  g_meshcore_http_ota_display_line[0] = '\0';
}

static void httpOtaDisplaySet(uint8_t pct, const char *line) {
  g_meshcore_http_ota_display_active = 1;
  g_meshcore_http_ota_display_pct = pct;
  if (line) {
    strncpy(g_meshcore_http_ota_display_line, line, sizeof(g_meshcore_http_ota_display_line) - 1);
    g_meshcore_http_ota_display_line[sizeof(g_meshcore_http_ota_display_line) - 1] = '\0';
  }
}

static unsigned long s_http_ota_last_emit_ms;
static uint8_t s_http_ota_last_emit_pct = 0xFF;

static void httpOtaEmitProgressThrottled(int clen, size_t total_written, const char *fallback_line) {
  uint8_t pct = 0xFF;
  if (clen > 0) {
    pct = (uint8_t)((total_written * 100ULL) / (size_t)clen);
    if (pct > 100) pct = 100;
  }
  httpOtaDisplaySet(pct, fallback_line);

  unsigned long now = millis();
  bool pct_jump = (clen > 0 && s_http_ota_last_emit_pct != 0xFF &&
                   (pct >= s_http_ota_last_emit_pct + 5 || pct == 100));
  if (now - s_http_ota_last_emit_ms < 450 && !pct_jump && s_http_ota_last_emit_ms != 0) return;
  s_http_ota_last_emit_ms = now;
  s_http_ota_last_emit_pct = pct;

  char line[96];
  if (clen > 0) {
    snprintf(line, sizeof(line), "OTA: downloading %u%%", (unsigned)pct);
  } else {
    snprintf(line, sizeof(line), "OTA: downloading (%u KB)", (unsigned)(total_written / 1024));
  }
  meshcoreRepeaterTcpOtaEmitLine(line);
}

/** App-only OTA: allow GitHub raw URLs and meshcomod flasher/repeater firmware-download proxy paths. */
static bool meshcoreHttpOtaUrlAllowed(const char* u) {
#if defined(OTA_URL_ALLOW_HTTP)
  if (strncmp(u, "http://127.0.0.1/", 17) == 0) return true;
  if (strncmp(u, "http://localhost/", 17) == 0) return true;
#endif
  if (strncmp(u, "https://raw.githubusercontent.com/", 34) == 0) return true;
  if (strncmp(u, "https://github.com/", 19) == 0 && strstr(u + 8, "/raw/") != nullptr) return true;
  if (strncmp(u, "https://flasher.meshcomod.com/firmware-download/", 48) == 0) return true;
  if (strncmp(u, "http://flasher.meshcomod.com/firmware-download/", 47) == 0) return true;
  if (strncmp(u, "https://repeater.meshcomod.com/firmware-download/", 50) == 0) return true;
  if (strncmp(u, "http://repeater.meshcomod.com/firmware-download/", 49) == 0) return true;
  return false;
}

/**
 * `https://github.com/owner/repo/raw/ref/path` -> `https://raw.githubusercontent.com/owner/repo/ref/path`
 * GitHub returns 302 for the former; ESP32 HTTPClient defaults to not following redirects, which breaks OTA.
 */
static bool meshcoreGithubRawToRawUsercontent(const char* url, char* out, size_t cap) {
  static const char gh[] = "https://github.com/";
  const size_t gh_len = sizeof(gh) - 1;
  if (strncmp(url, gh, gh_len) != 0) return false;
  const char* cursor = url + gh_len;
  const char* rawtok = strstr(cursor, "/raw/");
  if (!rawtok) return false;
  size_t owner_repo_len = (size_t)(rawtok - cursor);
  if (owner_repo_len < 3 || owner_repo_len > 240) return false;
  const char* past_raw = rawtok + 5;
  if (!past_raw[0]) return false;
  const char* path_after_ref = strchr(past_raw, '/');
  if (!path_after_ref || path_after_ref == past_raw) return false;
  size_t ref_len = (size_t)(path_after_ref - past_raw);
  if (ref_len == 0 || ref_len > 200) return false;
  const char* file_path = path_after_ref + 1;
  if (!file_path[0]) return false;
  int n = snprintf(out, cap, "https://raw.githubusercontent.com/%.*s/%.*s/%s", (int)owner_repo_len, cursor,
                   (int)ref_len, past_raw, file_path);
  return n > 0 && (size_t)n < cap;
}

/**
 * `https://raw.githubusercontent.com/owner/repo/ref/path` -> `https://cdn.jsdelivr.net/gh/owner/repo@ref/path`
 * Some WiFi paths block/refuse raw.githubusercontent.com while jsDelivr remains reachable.
 */
static bool meshcoreRawGithubToJsdelivr(const char* url, char* out, size_t cap) {
  static const char rawgh[] = "https://raw.githubusercontent.com/";
  const size_t rawgh_len = sizeof(rawgh) - 1;
  if (strncmp(url, rawgh, rawgh_len) != 0) return false;
  const char* p = url + rawgh_len;

  const char* slash1 = strchr(p, '/');     // owner/
  if (!slash1 || slash1 == p) return false;
  const char* repo = slash1 + 1;
  const char* slash2 = strchr(repo, '/');  // repo/
  if (!slash2 || slash2 == repo) return false;
  const char* ref = slash2 + 1;
  const char* slash3 = strchr(ref, '/');   // ref/
  if (!slash3 || slash3 == ref) return false;
  const char* path = slash3 + 1;
  if (!path[0]) return false;

  int n = snprintf(out, cap, "https://cdn.jsdelivr.net/gh/%.*s/%.*s@%.*s/%s", (int)(slash1 - p), p,
                   (int)(slash2 - repo), repo, (int)(slash3 - ref), ref, path);
  return n > 0 && (size_t)n < cap;
}

/**
 * `https://raw.githubusercontent.com/owner/repo/main/path` ->
 * `https://repeater.meshcomod.com/firmware-download/path` (or flasher.meshcomod.com).
 * The meshcomod nginx proxy maps /firmware-download/* to ALLFATHER-BV/meshcomod main/* on GitHub raw.
 */
static bool meshcoreRawGithubToMeshcomodProxy(const char* url, bool repeater_host, bool use_https, char* out,
                                              size_t cap) {
  static const char rawgh[] = "https://raw.githubusercontent.com/";
  const size_t rawgh_len = sizeof(rawgh) - 1;
  if (strncmp(url, rawgh, rawgh_len) != 0) return false;
  const char* p = url + rawgh_len;

  static const char prefix[] = "ALLFATHER-BV/meshcomod/main/";
  const size_t prefix_len = sizeof(prefix) - 1;
  if (strncmp(p, prefix, prefix_len) != 0) return false;
  const char* rel = p + prefix_len;
  if (!rel[0]) return false;

  const char* scheme = use_https ? "https" : "http";
  const char* host = repeater_host ? "repeater.meshcomod.com" : "flasher.meshcomod.com";
  int n = snprintf(out, cap, "%s://%s/firmware-download/%s", scheme, host, rel);
  return n > 0 && (size_t)n < cap;
}

void ESP32Board::emitHttpOtaNetDiagnosticLines() {
  if (WiFi.status() != WL_CONNECTED) {
    meshcoreRepeaterTcpOtaEmitLine("OTA: diag wifi=disconnected");
    return;
  }
  char line[120];
  snprintf(line, sizeof(line), "OTA: diag heap=%u ip=%s", (unsigned)esp_get_free_heap_size(),
           WiFi.localIP().toString().c_str());
  meshcoreRepeaterTcpOtaEmitLine(line);
  snprintf(line, sizeof(line), "OTA: diag gw=%s dns=%s rssi=%d", WiFi.gatewayIP().toString().c_str(),
           WiFi.dnsIP().toString().c_str(), (int)WiFi.RSSI());
  meshcoreRepeaterTcpOtaEmitLine(line);
  static const char* const kHosts[] = {"raw.githubusercontent.com", "cdn.jsdelivr.net", "repeater.meshcomod.com",
                                         "flasher.meshcomod.com", "github.com"};
  for (size_t i = 0; i < sizeof(kHosts) / sizeof(kHosts[0]); i++) {
    IPAddress ip;
    if (WiFi.hostByName(kHosts[i], ip)) {
      snprintf(line, sizeof(line), "OTA: diag %s -> %s", kHosts[i], ip.toString().c_str());
    } else {
      snprintf(line, sizeof(line), "OTA: diag %s -> DNS fail", kHosts[i]);
    }
    meshcoreRepeaterTcpOtaEmitLine(line);
  }
}

bool ESP32Board::startOTAUpdate(const char* id, char reply[]) {
  inhibit_sleep = true;   // prevent sleep during OTA
  WiFi.softAP("MeshCore-OTA", NULL);

  sprintf(reply, "Started: http://%s/update", WiFi.softAPIP().toString().c_str());
  MESH_DEBUG_PRINTLN("startOTAUpdate: %s", reply);

  static char id_buf[60];
  sprintf(id_buf, "%s (%s)", id, getManufacturerName());
  static char home_buf[90];
  sprintf(home_buf, "<H2>Hi! I am a MeshCore Repeater. ID: %s</H2>", id);

  AsyncWebServer* server = new AsyncWebServer(80);

  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", home_buf);
  });
  server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/packet_log", "text/plain");
  });

  AsyncElegantOTA.setID(id_buf);
  AsyncElegantOTA.begin(server);    // Start ElegantOTA
  server->begin();

  return true;
}

bool ESP32Board::startHttpOtaFromUrl(const char* url, char* reply) {
  if (!url || !reply) return false;
  s_http_ota_last_emit_ms = 0;
  s_http_ota_last_emit_pct = 0xFF;

  /* MESHCM / serial lines often end with '\n' or '\r\n'; HTTPClient treats that as part of the URL and
     many servers respond with HTTP 400. */
  static char url_trim[512];
  size_t n = 0;
  while (url[n] && n < sizeof(url_trim) - 1) {
    url_trim[n] = url[n];
    n++;
  }
  url_trim[n] = '\0';
  if (url[n] != '\0') {
    strcpy(reply, "ERR: URL too long");
    return true;
  }
  while (n > 0 && (url_trim[n - 1] == '\n' || url_trim[n - 1] == '\r' || url_trim[n - 1] == ' ' ||
                   url_trim[n - 1] == '\t')) {
    url_trim[--n] = '\0';
  }
  if (n == 0) {
    strcpy(reply, "ERR: missing URL");
    return true;
  }

  if (!meshcoreHttpOtaUrlAllowed(url_trim)) {
    strcpy(reply, "ERR: URL not allowed");
    return true;
  }
  if (httpOtaUrlLooksMergedBin(url_trim)) {
    strcpy(reply, "ERR: use non-merged .bin for ota url");
    meshcoreRepeaterTcpOtaEmitLine("OTA: ERR merged bin not allowed for OTA");
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    strcpy(reply, "ERR: WiFi not connected");
    return true;
  }
  if (s_http_ota_busy) {
    strcpy(reply, "ERR: OTA already running");
    return true;
  }
  HttpOtaBusyGuard ota_busy_guard(&s_http_ota_busy);

  inhibit_sleep = true;

  httpOtaDisplaySet(0xFF, "OTA: connecting");
  meshcoreRepeaterTcpOtaEmitLine("OTA: connecting");

  static char ota_url_buf[512];
  static char ota_url_alt_buf[512];
  static char ota_url_proxy_rep_https[512];
  static char ota_url_proxy_fls_https[512];
  static char ota_url_proxy_rep_http[512];
  static char ota_url_proxy_fls_http[512];
  const char* fetch_url = url_trim;
  const char* alt_fetch_url = nullptr;
  const char* proxy_rep_https = nullptr;
  const char* proxy_fls_https = nullptr;
  const char* proxy_rep_http = nullptr;
  const char* proxy_fls_http = nullptr;
  if (meshcoreGithubRawToRawUsercontent(url_trim, ota_url_buf, sizeof(ota_url_buf))) {
    fetch_url = ota_url_buf;
  }
  if (meshcoreRawGithubToJsdelivr(fetch_url, ota_url_alt_buf, sizeof(ota_url_alt_buf))) {
    alt_fetch_url = ota_url_alt_buf;
  }
  if (meshcoreRawGithubToMeshcomodProxy(fetch_url, true, true, ota_url_proxy_rep_https, sizeof(ota_url_proxy_rep_https))) {
    proxy_rep_https = ota_url_proxy_rep_https;
  }
  if (meshcoreRawGithubToMeshcomodProxy(fetch_url, false, true, ota_url_proxy_fls_https, sizeof(ota_url_proxy_fls_https))) {
    proxy_fls_https = ota_url_proxy_fls_https;
  }
  if (meshcoreRawGithubToMeshcomodProxy(fetch_url, true, false, ota_url_proxy_rep_http, sizeof(ota_url_proxy_rep_http))) {
    proxy_rep_http = ota_url_proxy_rep_http;
  }
  if (meshcoreRawGithubToMeshcomodProxy(fetch_url, false, false, ota_url_proxy_fls_http, sizeof(ota_url_proxy_fls_http))) {
    proxy_fls_http = ota_url_proxy_fls_http;
  }

  {
    char dline[120];
    snprintf(dline, sizeof(dline), "OTA: diag heap=%u ip=%s", (unsigned)esp_get_free_heap_size(),
             WiFi.localIP().toString().c_str());
    meshcoreRepeaterTcpOtaEmitLine(dline);
    snprintf(dline, sizeof(dline), "OTA: diag gw=%s dns=%s rssi=%d", WiFi.gatewayIP().toString().c_str(),
             WiFi.dnsIP().toString().c_str(), (int)WiFi.RSSI());
    meshcoreRepeaterTcpOtaEmitLine(dline);
    httpOtaEmitHostLookup("fetch", fetch_url);
  }

  WiFiClientSecure tls_client;
  tls_client.setInsecure();
  tls_client.setTimeout(90000);
  tls_client.setHandshakeTimeout(30);

  WiFiClient plain_client;
  plain_client.setTimeout(90000);

  HTTPClient https;
  https.setTimeout(90000);
  https.setReuse(false);
  // Default is HTTPC_DISABLE_FOLLOW_REDIRECTS; `defined(HTTPC_STRICT_...)` was always false (enum, not macro).
  https.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  https.setUserAgent("MeshCore-OTA/1.0");

  auto beginAndGet = [&](const char* target_url, int attempt) -> int {
    if (attempt > 1) {
      httpOtaDisplaySet(0xFF, "OTA: reconnecting");
      char retry_line[48];
      snprintf(retry_line, sizeof(retry_line), "OTA: connect retry %d/3", attempt);
      meshcoreRepeaterTcpOtaEmitLine(retry_line);

      if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        unsigned long wait_t0 = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - wait_t0) < 8000UL) {
          delay(100);
          yield();
        }
      }
      delay(200 * (attempt - 1));
      yield();
    }
    bool is_https = (strncmp(target_url, "https://", 8) == 0);
    bool ok = false;
    if (is_https) {
      ok = https.begin(tls_client, target_url);
    } else {
      ok = https.begin(plain_client, target_url);
    }
    if (!ok) {
      tls_client.stop();
      plain_client.stop();
      delay(15);
      return HTTPC_ERROR_CONNECTION_REFUSED;
    }
    return https.GET();
  };

  auto getWithRetries = [&](const char* target_url) -> int {
    httpOtaEmitHostLookup("try", target_url);
    int c = beginAndGet(target_url, 1);
    if (c < 0) {
      String err = https.errorToString(c);
      char err_line[96];
      snprintf(err_line, sizeof(err_line), "OTA: connect err %d %s", c, err.c_str());
      meshcoreRepeaterTcpOtaEmitLine(err_line);
      https.end();
      tls_client.stop();
      plain_client.stop();
      delay(15);
      c = beginAndGet(target_url, 2);
      if (c < 0) {
        err = https.errorToString(c);
        snprintf(err_line, sizeof(err_line), "OTA: connect err %d %s", c, err.c_str());
        meshcoreRepeaterTcpOtaEmitLine(err_line);
        https.end();
        tls_client.stop();
        plain_client.stop();
        delay(15);
        c = beginAndGet(target_url, 3);
      }
    }
    if (c < 0) {
      https.end();
      tls_client.stop();
      plain_client.stop();
    }
    return c;
  };

  /* For ALLFATHER meshcomod main firmware, try several mirrors. HTTP 200 + HTML error page from one host
     must not abort OTA — skip that mirror and try the next (raw GitHub, jsDelivr, other proxies). */
  auto closeHttpClients = [&]() {
    https.end();
    tls_client.stop();
    plain_client.stop();
    delay(15);
  };

  int clen = 0;
  int remaining = 0;
  uint8_t prefix[320];
  size_t prefix_len = 0;
  size_t prefix_skip = 0;

  auto tryMirror = [&](const char* candidate_url, const char* announce) -> bool {
    if (!candidate_url) return false;
    if (announce) meshcoreRepeaterTcpOtaEmitLine(announce);
    closeHttpClients();
    int code = getWithRetries(candidate_url);
    if (code != HTTP_CODE_OK) return false;
    if (!httpOtaResponseLooksLikeFirmwareBody(https)) {
      meshcoreRepeaterTcpOtaEmitLine("OTA: skip mirror (content-type)");
      closeHttpClients();
      return false;
    }
    clen = https.getSize();
    remaining = clen;
    WiFiClient* stream = https.getStreamPtr();
    if (!stream) {
      closeHttpClients();
      return false;
    }
    prefix_len = 0;
    prefix_skip = 0;
    if (httpOtaPeekEsp32ImagePrefix(stream, https, &remaining, prefix, sizeof(prefix), &prefix_len, &prefix_skip) !=
        1) {
      closeHttpClients();
      return false;
    }
    fetch_url = candidate_url;
    return true;
  };

  bool mirror_ok = false;
  if (proxy_rep_http || proxy_fls_http || proxy_fls_https || alt_fetch_url) {
    meshcoreRepeaterTcpOtaEmitLine("OTA: robust mirror rounds");
    const char* candidates[] = {
      proxy_fls_https, proxy_fls_http, fetch_url, alt_fetch_url, proxy_rep_https, proxy_rep_http
    };
    const char* labels[] = {
      "OTA: flasher https", "OTA: flasher http", "OTA: raw github",
      "OTA: jsdelivr", "OTA: repeater https", "OTA: repeater http"
    };
    const int rounds = 3;
    for (int round = 1; round <= rounds && !mirror_ok; round++) {
      char round_line[48];
      snprintf(round_line, sizeof(round_line), "OTA: mirror round %d/%d", round, rounds);
      meshcoreRepeaterTcpOtaEmitLine(round_line);
      for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (!candidates[i]) continue;
        mirror_ok = tryMirror(candidates[i], labels[i]);
        if (mirror_ok) break;
      }
      if (!mirror_ok) {
        if (WiFi.status() != WL_CONNECTED) {
          WiFi.reconnect();
          unsigned long wait_t0 = millis();
          while (WiFi.status() != WL_CONNECTED && (millis() - wait_t0) < 6000UL) {
            delay(100);
            yield();
          }
        }
        delay(250 * round);
      }
    }
  } else {
    /* Non-meshcomod URLs: keep only the explicit URL the user provided (no mirrors). */
    mirror_ok = tryMirror(fetch_url, "OTA: direct");
  }

  if (!mirror_ok) {
    strcpy(reply, "ERR: no usable OTA mirror");
    meshcoreRepeaterTcpOtaEmitLine("OTA: ERR all mirrors failed");
    closeHttpClients();
    httpOtaDisplayReset();
    return true;
  }

  WiFiClient* stream = https.getStreamPtr();
  if (!stream) {
    strcpy(reply, "ERR: no stream");
    closeHttpClients();
    httpOtaDisplayReset();
    return true;
  }

  httpOtaDisplaySet(0, "OTA: install started");
  meshcoreRepeaterTcpOtaEmitLine("OTA: HTTP OK, flashing");

  /* Unknown length uses full OTA partition; avoids bad Content-Length from proxies breaking Update. */
  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    snprintf(reply, 128, "ERR: %s", Update.errorString());
    https.end();
    tls_client.stop();
    plain_client.stop();
    httpOtaDisplayReset();
    return true;
  }

  size_t first_flash = prefix_len - prefix_skip;
  if (Update.write(prefix + prefix_skip, first_flash) != first_flash) {
    snprintf(reply, 128, "ERR: write %s", Update.errorString());
    Update.abort();
    https.end();
    tls_client.stop();
    plain_client.stop();
    httpOtaDisplayReset();
    char err_line[96];
    snprintf(err_line, sizeof(err_line), "OTA: ERR flash write (%s)", Update.errorString());
    meshcoreRepeaterTcpOtaEmitLine(err_line);
    return true;
  }

  uint8_t buf[2048];
  unsigned long t0 = millis();
  size_t total_written = first_flash;

  while (https.connected() && (remaining > 0 || remaining == -1)) {
    if (millis() - t0 > 180000UL) {
      Update.abort();
      https.end();
      tls_client.stop();
      plain_client.stop();
      httpOtaDisplayReset();
      strcpy(reply, "ERR: timeout");
      meshcoreRepeaterTcpOtaEmitLine("OTA: ERR timeout");
      return true;
    }

    size_t av = stream->available();
    if (!av) {
      if (remaining == 0) break;
      if (remaining < 0 && !stream->connected() && !av) break;
      delay(2);
      yield();
      continue;
    }

    size_t to_read = av > sizeof(buf) ? sizeof(buf) : av;
    int rd = stream->readBytes(buf, to_read);
    if (rd <= 0) break;

    if (Update.write(buf, (size_t)rd) != (size_t)rd) {
      snprintf(reply, 128, "ERR: write %s", Update.errorString());
      Update.abort();
      https.end();
      tls_client.stop();
      plain_client.stop();
      httpOtaDisplayReset();
      char err_line[96];
      snprintf(err_line, sizeof(err_line), "OTA: ERR flash write (%s)", Update.errorString());
      meshcoreRepeaterTcpOtaEmitLine(err_line);
      return true;
    }

    total_written += (size_t)rd;
    if (remaining > 0) remaining -= rd;

    httpOtaEmitProgressThrottled(clen, total_written, "OTA: downloading");
    yield();
  }

  if (clen > 0 && remaining != 0) {
    snprintf(reply, 128, "ERR: incomplete body rem=%d", remaining);
    Update.abort();
    https.end();
    tls_client.stop();
    plain_client.stop();
    httpOtaDisplayReset();
    meshcoreRepeaterTcpOtaEmitLine("OTA: ERR size mismatch");
    return true;
  }
  if (total_written < 65536) {
    strcpy(reply, "ERR: download too small");
    Update.abort();
    https.end();
    tls_client.stop();
    plain_client.stop();
    httpOtaDisplayReset();
    meshcoreRepeaterTcpOtaEmitLine("OTA: ERR download too small");
    return true;
  }

  https.end();

  httpOtaDisplaySet(100, "OTA: verifying");
  meshcoreRepeaterTcpOtaEmitLine("OTA: verifying");

  if (!Update.end(true)) {
    snprintf(reply, 128, "ERR: %s", Update.errorString());
    httpOtaDisplayReset();
    meshcoreRepeaterTcpOtaEmitLine("OTA: ERR verify");
    return true;
  }

  strcpy(reply, "> OK rebooting");
  httpOtaDisplaySet(100, "OTA: rebooting");
  meshcoreRepeaterTcpOtaEmitLine("OTA: rebooting");
  s_http_ota_reboot_pending = true;
  return true;
}

void ESP32Board::pollHttpOtaReboot() {
  if (s_http_ota_reboot_pending) {
    s_http_ota_reboot_pending = false;
    delay(300);
    esp_restart();
  }
}

#else
bool ESP32Board::startOTAUpdate(const char* id, char reply[]) {
  return false; // not supported
}

bool ESP32Board::startHttpOtaFromUrl(const char* url, char reply[]) {
  (void)url;
  (void)reply;
  return false;
}

void ESP32Board::emitHttpOtaNetDiagnosticLines() {}

void ESP32Board::pollHttpOtaReboot() {}
#endif

#endif
