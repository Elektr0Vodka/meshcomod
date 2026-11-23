// MQTT observer uplink for the Wi-Fi companion. See CompanionMqtt.h and
// docs/companion_mqtt.md.

#include "CompanionMqtt.h"

#ifdef WITH_MQTT_BRIDGE

#include "MyMesh.h"
#include "CompanionMqttPolicy.h"
#include <helpers/CompanionObserver.h>
#include <helpers/esp32/WifiRuntimeStore.h>
#include <helpers/TxtDataHelpers.h>
#include <new>

// src/helpers/CompanionMqttBridge.cpp (kept apart from MyMesh.h, see there).
void companionMqttBridgeSetSlotCap(int cap);
int companionMqttBridgeSkippedPublishes();

// Built in companionMqttLoad(), not at static init: it needs the mesh object,
// which lives in PSRAM on PSRAM boards and only exists once setup() has run.
static CompanionObserver* s_observer = nullptr;

void companionMqttLoad(FILESYSTEM* fs) {
  if (s_observer == nullptr) {
    s_observer = new (std::nothrow) CompanionObserver(board, the_mesh, FIRMWARE_VERSION, FIRMWARE_BUILD_DATE);
    if (s_observer == nullptr) {
      Serial.println("MQTT: out of memory, observer disabled");
      return;
    }
  }
  // No import credentials: the companion never writes its Wi-Fi into /mqtt.json.
  s_observer->loadPrefs(fs, nullptr, nullptr);
}

bool companionMqttAdoptWifi() {
  if (s_observer == nullptr || s_observer->prefs() == nullptr) return false;
  char ssid[WIFI_CONFIG_SSID_MAX];
  wifiConfigGetSsid(ssid, sizeof(ssid));
  const MQTTPrefs* obs = s_observer->prefs();
  if (!CompanionMqttPolicy::shouldAdoptObserverWifi(ssid, obs->wifi_ssid)) return false;
  if (!wifiConfigSetSsid(obs->wifi_ssid) || !wifiConfigSetPwd(obs->wifi_password)) return false;
  Serial.printf("MQTT: adopted Wi-Fi '%s' from /mqtt.json\n", obs->wifi_ssid);
  return true;
}

void companionMqttStart(mesh::MainBoard& board_ref, mesh::Dispatcher* dispatcher, mesh::Radio* radio,
                        mesh::MillisecondClock* ms, mesh::PacketManager* mgr, mesh::RTCClock* rtc,
                        mesh::LocalIdentity* identity, bool ble_on_at_boot) {
  (void)board_ref;
  if (s_observer == nullptr) return;
#if defined(BOARD_HAS_PSRAM)
  const bool has_psram = psramFound();
#else
  const bool has_psram = false;
#endif
  const int cap = CompanionMqttPolicy::slotCap(has_psram, ble_on_at_boot);
  companionMqttBridgeSetSlotCap(cap);
  Serial.printf("MQTT: starting observer (psram=%d ble=%d slot cap=%d)\n", (int)has_psram,
                (int)ble_on_at_boot, cap);
  s_observer->begin(dispatcher, radio, ms, mgr, rtc, identity);
}

void companionMqttLoop() {
  if (s_observer) s_observer->loop();
}

void companionMqttRestart() {
  if (s_observer) s_observer->restartBridge();
}

void companionMqttOnRx(mesh::Packet* pkt) {
  if (s_observer) s_observer->onPacketRx(pkt);
}

void companionMqttOnTx(mesh::Packet* pkt) {
  if (s_observer) s_observer->onPacketTx(pkt);
}

void companionMqttOnRawRx(const uint8_t* raw, int len, float snr, float rssi) {
  if (s_observer) s_observer->onRawRx(raw, len, snr, rssi);
}

bool companionMqttCommand(uint32_t sender_timestamp, const char* command, char* reply) {
  if (!CompanionMqttPolicy::isObserverCommand(command)) return false;
  if (s_observer == nullptr) {
    strcpy(reply, "Err - MQTT observer not running");
    return true;
  }
  char sub_reply[161];
  sub_reply[0] = 0;
  if (!s_observer->handleCommand(sender_timestamp, command, sub_reply)) return false;
  StrHelper::strncpy(reply, sub_reply, sizeof(sub_reply));
  return true;
}

bool companionMqttRunning() {
  return s_observer && s_observer->isBridgeRunning();
}

int companionMqttConnectedSlots() {
  return s_observer ? s_observer->connectedSlots() : -1;
}

int companionMqttConfiguredSlots() {
  return s_observer ? s_observer->configuredSlots() : -1;
}

int companionMqttQueueSize() {
  return s_observer ? s_observer->queueSize() : 0;
}

int companionMqttSkippedPublishes() {
  return companionMqttBridgeSkippedPublishes();
}

#endif  // WITH_MQTT_BRIDGE
