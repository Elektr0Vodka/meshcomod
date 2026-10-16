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
#include <string.h>
#include <string>

#include <helpers/esp32/MultiTransportCompanionInterface.h>
#include <helpers/esp32/WifiRuntimeStore.h>
#include "MyMesh.h"   // the_mesh, FIRMWARE_VERSION; pulls in target.h (radio_driver, board)

extern MultiTransportCompanionInterface serial_interface;

// Temporary, unregistered enterprise number, the same one the observer firmware uses.
#define MESHCORE_OID_BASE ".1.3.6.1.4.1.99999"

// Agent and socket are created on first start and kept, so toggling never leaks.
static WiFiUDP* s_udp = nullptr;
static SNMPAgent* s_agent = nullptr;
static bool s_running = false;
static bool s_loaded = false;
static bool s_enabled = false;

// Every value is read when a request arrives. The agent runs in the main loop, the same
// context as the mesh, so these reads need no locking and are never stale.
static void registerHandlers(SNMPAgent* a) {
  // System (.1.x.0), same as the observer
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".1.1.0", []() -> int { return (int)(millis() / 1000); });
  a->addDynamicReadOnlyStringHandler(MESHCORE_OID_BASE ".1.2.0", []() -> const std::string { return FIRMWARE_VERSION; });
  a->addDynamicReadOnlyStringHandler(MESHCORE_OID_BASE ".1.3.0", []() -> const std::string {
    return std::string(the_mesh.getNodePrefs()->node_name);
  });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".1.4.0", []() -> int { return (int)board.getBattMilliVolts(); });

  // Radio (.2.x.0), same as the observer
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

  // The observer's .3 MQTT group has no companion equivalent and is left out.

  // Memory (.4.x.0), same as the observer. heap_caps reports 0 for PSRAM on boards without it.
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.1.0", []() -> int { return (int)ESP.getFreeHeap(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.2.0", []() -> int { return (int)ESP.getMaxAllocHeap(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.3.0", []() -> int { return (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".4.4.0", []() -> int { return (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM); });

  // Network (.5.x.0), same as the observer
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".5.1.0", []() -> int {
    return WiFi.status() == WL_CONNECTED ? (int)WiFi.RSSI() : -127;
  });

  // Companion (.6.x.0), companion builds only
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".6.1.0", []() -> int { return serial_interface.getTcpConnectedCount(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".6.2.0", []() -> int { return serial_interface.getWsConnectedCount(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".6.3.0", []() -> int { return the_mesh.getNumContacts(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".6.4.0", []() -> int { return the_mesh.getNumChannels(); });
  a->addDynamicIntegerHandler(MESHCORE_OID_BASE ".6.5.0", []() -> int { return (int)(the_mesh.getReceiveAirTime() / 1000); });
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
  if (s_running) s_agent->loop();
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
