#pragma once

// MQTT observer uplink for the Wi-Fi companion (_mqtt envs, -D WITH_MQTT_BRIDGE).
// Wraps the vendored Dutch-MeshCore CompanionObserver (docs/dmc_observer_vendor.md).
// Every packet the radio hears or sends also goes to the configured MQTT brokers;
// settings live in /mqtt.json, shared with the DMC observer firmwares.
//
// Wi-Fi stays with the companion: the observer runs over a passive network link
// and never starts or reconnects the radio. Credentials stay in NVS; the only
// exchange is a one-way adopt of an SSID found in /mqtt.json (CompanionMqttPolicy).

#ifdef WITH_MQTT_BRIDGE

#include <Mesh.h>
#include <stddef.h>
#include <stdint.h>
#include <helpers/IdentityStore.h>   // FILESYSTEM

// Load /mqtt.json. Before the boot-time Wi-Fi decision, so an adopted SSID is used.
void companionMqttLoad(FILESYSTEM* fs);
// One-way adopt: copy the /mqtt.json SSID into NVS when the companion has none.
// After wifiConfigBegin() and companionMqttLoad(). Returns true when it adopted.
bool companionMqttAdoptWifi();
// Start the bridge (MyMesh::startMqttObserver calls this with its own members).
void companionMqttStart(mesh::MainBoard& board, mesh::Dispatcher* dispatcher, mesh::Radio* radio,
                        mesh::MillisecondClock* ms, mesh::PacketManager* mgr, mesh::RTCClock* rtc,
                        mesh::LocalIdentity* identity, bool ble_on_at_boot);
void companionMqttLoop();
// Restart after the identity changed (device ID and JWT username come from the key).
void companionMqttRestart();

void companionMqttOnRx(mesh::Packet* pkt);
void companionMqttOnTx(mesh::Packet* pkt);
void companionMqttOnRawRx(const uint8_t* raw, int len, float snr, float rssi);

// `get|set mqtt...`, `get|set timezone`, `get link.status`. Returns false for any
// other command. `reply` must hold 161 bytes. sender_timestamp is 0 for local
// callers (app, USB) and the sender's clock for remote CLI, which gets secrets masked.
bool companionMqttCommand(uint32_t sender_timestamp, const char* command, char* reply);

bool companionMqttRunning();
int companionMqttConnectedSlots();    // -1 when the bridge is not running
int companionMqttConfiguredSlots();   // -1 when the bridge is not running
int companionMqttQueueSize();
int companionMqttSkippedPublishes();

#endif
