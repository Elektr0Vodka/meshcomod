#include "CompanionSNMP.h"

#ifdef WITH_SNMP

#ifndef MULTI_TRANSPORT_COMPANION
#error "WITH_SNMP needs a Wi-Fi companion build (MULTI_TRANSPORT_COMPANION)"
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <SNMP_Agent.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>
#include <string>

#include <helpers/esp32/MultiTransportCompanionInterface.h>
#include <helpers/esp32/WifiRuntimeStore.h>
#include "MyMesh.h"   // the_mesh, FIRMWARE_VERSION; pulls in target.h (radio_driver, board)

extern MultiTransportCompanionInterface serial_interface;

// Temporary, unregistered enterprise number, the same one the observer firmware uses.
#define MESHCORE_OID_BASE ".1.3.6.1.4.1.99999"

// Same "no value" marker as the observer for a board without an MCU temperature sensor.
#define SNMP_TEMP_UNAVAILABLE (-127)

extern SimpleMeshTables tables;

// Agent and socket are created on first start and kept, so toggling never leaks.
static WiFiUDP* s_udp = nullptr;
static SNMPAgent* s_agent = nullptr;
static bool s_running = false;
static bool s_loaded = false;
static bool s_enabled = false;

// Values the agent serves from a copy instead of reading them per request.
// Hardware (battery ADC, die temperature) is read every 30 s: the Heltec V4 battery read
// blocks for 10 ms and neither value moves quickly. Counter32 handlers need a variable to
// point at, so those totals are copied every 2 s. All of it happens in companionSnmpLoop(),
// on the main loop with the mesh, never inside a request.
static int s_battery_mv = 0;
static int s_mcu_temp_c = SNMP_TEMP_UNAVAILABLE;
static int s_reset_reason = 0;
static char s_reset_reason_text[48] = "";
static bool s_ntp_synced = false;
static uint32_t s_flood_dups = 0;
static uint32_t s_direct_dups = 0;
static uint32_t s_rx_air_secs = 0;
static uint32_t s_cf_malformed = 0;
static uint32_t s_cf_age = 0;
static uint32_t s_cf_path = 0;
static uint32_t s_cf_sender = 0;
static uint32_t s_cf_text = 0;
static uint32_t s_last_stats_ms = 0;
static uint32_t s_last_health_ms = 0;
static bool s_health_read = false;

static void readHealth() {
  s_battery_mv = (int)board.getBattMilliVolts();
  const float t = board.getMCUTemperature();
  s_mcu_temp_c = isnan(t) ? SNMP_TEMP_UNAVAILABLE : (int)lroundf(t);
}

static void copyCounters() {
  s_flood_dups = tables.getNumFloodDups();
  s_direct_dups = tables.getNumDirectDups();
  s_rx_air_secs = (uint32_t)(the_mesh.getReceiveAirTime() / 1000);
  const Counters& c = the_mesh.snmpFilterCounters();
  s_cf_malformed = c.malformed;
  s_cf_age = c.age;
  s_cf_path = c.path;
  s_cf_sender = c.sender;
  s_cf_text = c.text;
}

// Layout: the observer's MQTT_SNMP.md (Dutch-MeshCore dmc-observer-dev). Shared numbers keep
// the observer's meaning, type and unit; an OID the companion has no data for is not
// registered, so it reads noSuchObject instead of a 0 that looks real. Companion-only values
// use .9 and up, which the observer keeps free.
static void registerHandlers(SNMPAgent* a) {
  // System (.1.x.0)
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".1.1.0", []() -> int { return (int)(millis() / 1000); });
  a->addDynamicReadOnlyStringHandler(MESHCORE_OID_BASE ".1.2.0", []() -> const std::string { return FIRMWARE_VERSION; });
  a->addDynamicReadOnlyStringHandler(MESHCORE_OID_BASE ".1.3.0", []() -> const std::string {
    return std::string(the_mesh.getNodePrefs()->node_name);
  });
  a->addIntegerHandler(MESHCORE_OID_BASE ".1.4.0", &s_battery_mv);
  a->addIntegerHandler(MESHCORE_OID_BASE ".1.5.0", &s_mcu_temp_c);
  a->addIntegerHandler(MESHCORE_OID_BASE ".1.6.0", &s_reset_reason);
  a->addDynamicReadOnlyStringHandler(MESHCORE_OID_BASE ".1.7.0", []() -> const std::string {
    return std::string(s_reset_reason_text);
  });

  // Radio (.2.x.0). .2.16 (TX duty cycle) is left out: this firmware does not track it.
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.1.0", []() -> int { return (int)radio_driver.getPacketsRecv(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.2.0", []() -> int { return (int)radio_driver.getPacketsSent(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.3.0", []() -> int { return (int)radio_driver.getPacketsRecvErrors(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.4.0", []() -> int { return radio_driver.getNoiseFloor(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.5.0", []() -> int { return (int)radio_driver.getLastRSSI(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.6.0", []() -> int { return (int)(radio_driver.getLastSNR() * 4); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.7.0", []() -> int { return (int)the_mesh.getNumSentFlood(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.8.0", []() -> int { return (int)the_mesh.getNumSentDirect(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.9.0", []() -> int { return (int)the_mesh.getNumRecvFlood(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.10.0", []() -> int { return (int)the_mesh.getNumRecvDirect(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.11.0", []() -> int { return (int)(the_mesh.getTotalAirTime() / 1000); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.12.0", []() -> int { return the_mesh.snmpTxQueueLen(); });
  a->addCounter32Handler(MESHCORE_OID_BASE ".2.13.0", &s_flood_dups);
  a->addCounter32Handler(MESHCORE_OID_BASE ".2.14.0", &s_direct_dups);
  a->addCounter32Handler(MESHCORE_OID_BASE ".2.15.0", &s_rx_air_secs);
  // Bits 1, 2 and 4 as on the observer. Bit 8 (radio watchdog) does not exist here and stays 0.
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".2.17.0", []() -> int { return (int)the_mesh.snmpErrFlags(); });

  // MQTT (.3.x.0): no bridge on the companion, so only the NTP flag, which it does have.
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".3.4.0", []() -> int { return s_ntp_synced ? 1 : 0; });

  // Memory (.4.x.0). heap_caps reports 0 for PSRAM on boards without it.
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.1.0", []() -> int { return (int)ESP.getFreeHeap(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.2.0", []() -> int { return (int)ESP.getMaxAllocHeap(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.3.0", []() -> int { return (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.4.0", []() -> int { return (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.5.0", []() -> int { return (int)ESP.getMinFreeHeap(); });

  // Network (.5.x.0)
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".5.1.0", []() -> int {
    return WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : -127;
  });

  // .6 duty gate, .7 packet filter and .8 neighbours are repeater features: not registered.

  // Companion (.9.x.0), companion only
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".9.1.0", []() -> int { return serial_interface.getTcpConnectedCount(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".9.2.0", []() -> int { return serial_interface.getWsConnectedCount(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".9.3.0", []() -> int { return the_mesh.getNumContacts(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".9.4.0", []() -> int { return the_mesh.getNumChannels(); });

  // Receive filter (.10.x.0), companion only. Not the repeater's .7: these count messages
  // kept from the user (dropped as junk or hidden by taste), not packets kept off the air.
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".10.1.0", []() -> int { return the_mesh.snmpFilterEnabled() ? 1 : 0; });
  a->addCounter32Handler(MESHCORE_OID_BASE ".10.2.0", &s_cf_malformed);
  a->addCounter32Handler(MESHCORE_OID_BASE ".10.3.0", &s_cf_age);
  a->addCounter32Handler(MESHCORE_OID_BASE ".10.4.0", &s_cf_path);
  a->addCounter32Handler(MESHCORE_OID_BASE ".10.5.0", &s_cf_sender);
  a->addCounter32Handler(MESHCORE_OID_BASE ".10.6.0", &s_cf_text);
}

static void loadPrefs() {
  if (s_loaded) return;
  s_loaded = true;
  s_enabled = wifiConfigGetSnmpEnabled();
}

static void startAgent() {
  char community[WIFI_CONFIG_SNMP_COMMUNITY_MAX];
  wifiConfigGetSnmpCommunity(community, sizeof(community));
  if (!s_agent) {
    const uint32_t reason = board.getResetReason();
    s_reset_reason = (int)reason;
    strncpy(s_reset_reason_text, board.getResetReasonString(reason), sizeof(s_reset_reason_text) - 1);
    s_reset_reason_text[sizeof(s_reset_reason_text) - 1] = '\0';

    s_udp = new WiFiUDP();
    s_agent = new SNMPAgent(community);
    registerHandlers(s_agent);
    s_agent->sortHandlers();
    s_agent->setUDP(s_udp);   // registers the socket and binds UDP 161
  } else {
    s_agent->setReadWriteCommunity(community);
    s_agent->restartUDP();    // the old socket died with the link
  }
  s_running = true;
  Serial.printf("[snmp] agent up on udp/161 (free=%u)\n", (unsigned)ESP.getFreeHeap());
}

static void stopAgent() {
  s_agent->stop();
  s_running = false;
  Serial.println("[snmp] agent stopped");
}

void companionSnmpLoop(bool link_up) {
  loadPrefs();
  const bool want = s_enabled && link_up;
  if (want && !s_running) {
    startAgent();
  } else if (!want && s_running) {
    stopAgent();
  }
  if (!s_running) return;

  const uint32_t now = millis();
  if (!s_health_read || now - s_last_health_ms >= 30000) {
    s_health_read = true;
    s_last_health_ms = now;
    readHealth();
  }
  if (s_last_stats_ms == 0 || now - s_last_stats_ms >= 2000) {
    s_last_stats_ms = now;
    copyCounters();
  }
  s_agent->loop();
}

void companionSnmpSetNtpSynced() {
  s_ntp_synced = true;
}

bool companionSnmpCommand(const char* command, bool remote, char* reply) {
  if (strcmp(command, "get snmp") == 0) {
    loadPrefs();
    strcpy(reply, s_enabled ? "> on" : "> off");
    return true;
  }
  if (strcmp(command, "get snmp.status") == 0) {
    loadPrefs();
    if (!s_enabled)      strcpy(reply, "> off");
    else if (s_running)  strcpy(reply, "> running on udp/161");
    else                 strcpy(reply, "> on, waiting for Wi-Fi");
    return true;
  }
  if (memcmp(command, "set snmp ", 9) == 0) {
    const char* v = &command[9];
    bool on;
    if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0)        on = true;
    else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0)  on = false;
    else { strcpy(reply, "Error: use set snmp on|off"); return true; }
    loadPrefs();
    wifiConfigSetSnmpEnabled(on);
    s_enabled = on;   // the main loop starts or stops the agent
    sprintf(reply, "> snmp is now %s", on ? "on" : "off");
    return true;
  }
  if (strcmp(command, "get snmp.community") == 0) {
    if (remote) {   // the community is the only access control, keep it off the mesh
      strcpy(reply, "Error: not available remotely");
    } else {
      char community[WIFI_CONFIG_SNMP_COMMUNITY_MAX];
      wifiConfigGetSnmpCommunity(community, sizeof(community));
      sprintf(reply, "> %s", community);
    }
    return true;
  }
  if (memcmp(command, "set snmp.community ", 19) == 0) {
    const char* v = &command[19];
    if (!wifiConfigSetSnmpCommunity(v)) {
      sprintf(reply, "Error: 1 to %d characters", WIFI_CONFIG_SNMP_COMMUNITY_MAX - 1);
      return true;
    }
    if (s_agent) s_agent->setReadWriteCommunity(v);   // applies to the next request
    strcpy(reply, "> snmp.community updated");
    return true;
  }
  return false;
}

#endif // WITH_SNMP
