#pragma once

#include <string.h>

// Decisions the companion makes around the vendored MQTT observer (_mqtt envs).
// Header-only and free of Arduino, so it runs on `native`
// (test/test_companion_mqtt_policy).
namespace CompanionMqttPolicy {

  // Longest SSID the companion's NVS store keeps (WIFI_CONFIG_SSID_MAX bytes with
  // the terminator). A longer one would be cut and join nothing.
  static const size_t kMaxSsidLen = 31;

  // One-way Wi-Fi adopt: on boot, take the SSID a DMC observer firmware left in
  // /mqtt.json only when the companion has none of its own. Never the other way.
  inline bool shouldAdoptObserverWifi(const char* companion_ssid, const char* observer_ssid) {
    if (companion_ssid != nullptr && companion_ssid[0] != 0) return false;
    if (observer_ssid == nullptr || observer_ssid[0] == 0) return false;
    return strlen(observer_ssid) <= kMaxSsidLen;
  }

  // Active MQTT slot cap handed to MQTTBridge::setActiveSlotCap(). Each TLS slot
  // needs about 40 KB of internal RAM. With PSRAM the bridge's own limit stands
  // (0 = no extra cap). Without it, BLE running from boot leaves room for one slot,
  // otherwise two (the repeater observer's limit on the same boards).
  inline int slotCap(bool has_psram, bool ble_on_at_boot) {
    if (has_psram) return 0;
    return ble_on_at_boot ? 1 : 2;
  }

  // True when `key_start` begins with `key` followed by the end of the command or a
  // space (so "timezones" is not "timezone").
  inline bool keyIs(const char* key_start, const char* key) {
    size_t n = strlen(key);
    return strncmp(key_start, key, n) == 0 && (key_start[n] == 0 || key_start[n] == ' ');
  }

  // CLI commands the observer answers: `get|set mqtt...` (mqtt.*, mqttN.*),
  // `get|set timezone` and `get link.status`. Wi-Fi, OTA, SNMP and everything else
  // stay with the companion, which owns the radio and its own settings.
  inline bool isObserverCommand(const char* cmd) {
    if (cmd == nullptr) return false;
    bool get = strncmp(cmd, "get ", 4) == 0;
    bool set = strncmp(cmd, "set ", 4) == 0;
    if (!get && !set) return false;
    const char* key = cmd + 4;
    if (strncmp(key, "mqtt", 4) == 0 && key[4] != 0 && key[4] != ' ') return true;
    if (keyIs(key, "timezone")) return true;
    if (get && keyIs(key, "link.status")) return true;
    return false;
  }

}  // namespace CompanionMqttPolicy
