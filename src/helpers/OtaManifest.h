#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Manifest pull OTA (`ota check` / `ota update`): the pure decisions, kept free of
// network and flash so they run in the native tests. ESP32Board does the I/O.
//
// The device reads <OTA_MANIFEST_BASE>/<OTA_VARIANT>.json, served by the
// DutchMeshCore OTA server in the same shape as for the DMC observer builds:
//   {"file":"<image url>","version":"v1.18.0.2","hash":"abc1234","partSig":"..."}
// build.sh bakes OTA_MANIFEST_BASE and OTA_VARIANT (the PlatformIO env name) in.

struct OtaVersion {
  int part[4];   // "v1.18.0.1" -> 1, 18, 0, 1; missing parts stay 0
  int count;     // number of parts present
  char hash[16]; // token after the last '-', or "" when there is no '-'
};

// Parse "v1.18.0.1[-...]-<hash>". The numeric version ends at the first '-'.
// Returns false when the text does not start with a version.
static inline bool ota_version_parse(const char* s, OtaVersion* out) {
  memset(out, 0, sizeof(*out));
  if (s == nullptr) return false;
  const char* p = s;
  if (*p == 'v' || *p == 'V') p++;
  while (out->count < 4) {
    if (*p < '0' || *p > '9') return false;
    int n = 0;
    while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
    out->part[out->count++] = n;
    if (*p != '.') break;
    p++;
  }
  if (*p != '\0' && *p != '-') return false;
  const char* dash = strrchr(s, '-');
  if (dash != nullptr) {
    strncpy(out->hash, dash + 1, sizeof(out->hash) - 1);
  }
  return true;
}

enum OtaVerdict : uint8_t {
  OTA_VERDICT_CURRENT = 0,  // the published build is the one running
  OTA_VERDICT_NEWER,        // the published build has a higher version
  OTA_VERDICT_OLDER,        // the running build is newer than the published one
  OTA_VERDICT_REBUILD,      // same version, different commit
};

// Shared-prefix compare absorbs the 7 vs 8 character git abbreviation.
static inline bool ota_hash_equal(const char* a, const char* b) {
  size_t la = strlen(a), lb = strlen(b);
  size_t m = la < lb ? la : lb;
  return m >= 7 && strncmp(a, b, m) == 0;
}

// Compare the running FIRMWARE_VERSION with a manifest's version and hash.
static inline OtaVerdict ota_compare(const char* running, const char* avail_version, const char* avail_hash) {
  OtaVersion own, avail;
  bool own_ok = ota_version_parse(running, &own);
  bool avail_ok = ota_version_parse(avail_version, &avail);
  if (!own_ok || !avail_ok) {
    // No usable version on one side: only the commit can tell them apart.
    char own_hash[16] = {0};
    const char* dash = running ? strrchr(running, '-') : nullptr;
    if (dash) strncpy(own_hash, dash + 1, sizeof(own_hash) - 1);
    return ota_hash_equal(own_hash, avail_hash ? avail_hash : "") ? OTA_VERDICT_CURRENT : OTA_VERDICT_REBUILD;
  }
  for (int i = 0; i < 4; i++) {
    if (avail.part[i] > own.part[i]) return OTA_VERDICT_NEWER;
    if (avail.part[i] < own.part[i]) return OTA_VERDICT_OLDER;
  }
  if (own.hash[0] && avail_hash && avail_hash[0] && !ota_hash_equal(own.hash, avail_hash)) {
    return OTA_VERDICT_REBUILD;
  }
  return OTA_VERDICT_CURRENT;
}

// One partition-table entry, as esp_partition reports it.
struct OtaPartEntry {
  uint8_t type, subtype;
  uint32_t offset, size;
};

// Canonical partition-table signature. MUST match scripts/partition_signature.py:
// each entry "type:subtype:offset:size" in lowercase hex, sorted by offset, joined
// by ','. Sorts `e` in place.
static inline void ota_partsig_format(OtaPartEntry* e, int n, char* out, size_t out_sz) {
  for (int i = 1; i < n; i++) {
    OtaPartEntry k = e[i];
    int j = i - 1;
    while (j >= 0 && e[j].offset > k.offset) { e[j + 1] = e[j]; j--; }
    e[j + 1] = k;
  }
  if (out_sz == 0) return;
  out[0] = 0;
  size_t pos = 0;
  for (int i = 0; i < n; i++) {
    int w = snprintf(out + pos, out_sz - pos, "%s%x:%x:%x:%x", i ? "," : "", e[i].type, e[i].subtype,
                     (unsigned)e[i].offset, (unsigned)e[i].size);
    if (w < 0 || (size_t)w >= out_sz - pos) {  // truncated: keep what fits, stop
      out[out_sz - 1] = 0;
      return;
    }
    pos += (size_t)w;
  }
}

// Text after "http://" or "https://", or nullptr for any other scheme.
static inline const char* ota_url_after_scheme(const char* url) {
  if (url == nullptr) return nullptr;
  if (strncmp(url, "https://", 8) == 0) return url + 8;
  if (strncmp(url, "http://", 7) == 0) return url + 7;
  return nullptr;
}

// Manifest URL for `ota check` / `ota update`, always over plain HTTP: it only
// carries version info, and skipping TLS keeps the check cheap on RAM-tight
// builds. The image itself is fetched from the manifest's `file`.
static inline bool ota_manifest_url(const char* base, const char* variant, char* out, size_t out_sz) {
  const char* rest = ota_url_after_scheme(base);
  if (rest == nullptr) return false;
  int n = snprintf(out, out_sz, "http://%s/%s.json", rest, variant);
  return n > 0 && (size_t)n < out_sz;
}

// A manifest may only point at images under the server's firmware path for this
// product: base ".../meshcomod/v" -> ".../meshcomod/fw/". Either scheme is accepted.
static inline bool ota_firmware_url_allowed(const char* url, const char* base) {
  const char* u = ota_url_after_scheme(url);
  const char* b = ota_url_after_scheme(base);
  if (u == nullptr || b == nullptr) return false;
  size_t blen = strlen(b);
  if (blen < 2 || strcmp(b + blen - 2, "/v") != 0) return false;
  size_t root = blen - 1;  // keep the '/' before "v"
  if (strncmp(u, b, root) != 0 || strncmp(u + root, "fw/", 3) != 0) return false;
  return strstr(u, "..") == nullptr;
}
