# Companion MQTT observer

## Overview

The `_mqtt` companion builds add an MQTT uplink to the Wi-Fi companion: every
packet the radio hears or sends also goes to one or more MQTT brokers, such as
the letsmesh analyzers, the DutchMeshCore collectors or MeshMapper. The companion
keeps everything else: the app connects over USB, TCP, WebSocket or BLE as on the
`usb_tcp` build, and SNMP, manifest OTA, full capture and the receive filter all
stay.

The uplink is the Dutch-MeshCore "Companion Observer" code (branch
`dmc-observer-dev`, originally by agessaman), vendored into this tree. See
[dmc_observer_vendor.md](dmc_observer_vendor.md) for what is copied and patched.

> Hardware testing note: verified to build for the envs below. It has not yet
> been tested on hardware.

| Board | Env | Broker slots |
| --- | --- | --- |
| Heltec V4 | `heltec_v4_companion_radio_usb_tcp_mqtt` | 5 |
| Heltec V4 R8 | `heltec_v4_r8_companion_radio_usb_tcp_mqtt` | 5 |
| Seeed Xiao S3 WIO | `Xiao_S3_WIO_companion_radio_usb_tcp_mqtt` | 5 |
| Heltec V3 | `Heltec_v3_companion_radio_usb_tcp_mqtt` | 2, or 1 while BLE is on |

The version string is the same as the `usb_tcp` build's. Manifest OTA (`ota
check`, `ota update`) uses the env name as its variant, so an `_mqtt` node only
looks for `_mqtt` builds, never the plain `usb_tcp` image.

---

# Quick start

From the app's command console (or remote CLI from an allowed contact):

```text
set mqtt.iata AMS
set mqtt1.preset dutchmeshcore-1
set mqtt2.preset analyzer-eu
get mqtt.status
```

Wi-Fi is the companion's own: set it as on any meshcomod companion (`set
wifi.ssid`, `set wifi.pwd`, `wifi.apply`, or the device UI). The observer starts
its broker connections once Wi-Fi has an address, and follows the link when it
drops and comes back. No reboot is needed for broker changes.

Fresh installs start with `analyzer-us` in slot 1 and `analyzer-eu` in slot 2.

---

# Commands

The companion passes these to the observer; everything else stays with the
companion.

| Command | Purpose |
| --- | --- |
| `get/set mqttN.preset <name>` | Broker preset for slot N (1 to 6), `custom`, or `none` |
| `get/set mqttN.server`, `.port`, `.username`, `.password`, `.token`, `.topic`, `.audience` | Custom broker settings |
| `get/set mqttN.iata <code>` | Region code for one slot; bare `set` falls back to `mqtt.iata` |
| `get/set mqttN.filter <all\|none\|list>` | Packet types sent to a slot |
| `get/set mqtt.iata <code>` | Region code used in the topic paths |
| `get mqtt.status`, `get mqtt.stats`, `get mqttN.diag`, `get mqtt.presets` | Status and diagnostics |
| `get/set timezone <tz>` | Timezone the observer stamps its messages with |
| `get link.status` | Wi-Fi state as the observer sees it |

The full list, the preset table and the topic layout are in Dutch-MeshCore's
[`MQTT_IMPLEMENTATION.md`](https://github.com/Dutch-MeshCore/MeshCore/blob/dmc-observer-dev/MQTT_IMPLEMENTATION.md).
Over remote CLI, passwords and tokens are masked in `get` replies.

Not on the companion: the observer's `wifi.*` commands (the companion's own
`wifi.*` commands apply), OTA through the observer (use the companion's `ota`),
fault alerts, MQTT self-heal, neighbour publishing and the web portal.

---

# Memory, BLE and slots

Each TLS broker connection needs about 40 KB of internal RAM. The builds handle
that differently per board:

* **PSRAM boards (V4, V4 R8, Xiao S3 WIO):** up to 5 slots. The mesh object
  (contacts, offline queue) and the TLS buffers live in PSRAM, so BLE, Wi-Fi and
  the brokers fit side by side.
* **Heltec V3 (no PSRAM):** 2 slots while BLE is off, 1 while BLE is on. Extra
  slots stay saved and show as inactive. The build also links smaller mbedTLS
  buffers (4 KiB outbound instead of 16 KiB, about 16 KiB less per connection),
  caps the AsyncTCP task stack at 8 KiB, holds 150 contacts instead of 350 and
  leaves out mDNS.

BLE stays a toggle on every `_mqtt` build, with these rules:

* The slot limit is fixed at boot, from whether BLE started. Toggle BLE, then
  reboot, to move between 1 and 2 slots on the V3.
* Turning BLE off stops advertising at once, but its memory only comes back after
  a reboot.
* Turning BLE on while the brokers already hold the memory is refused with
  `ble: not started, not enough free memory`. Free a slot (`set mqtt2.preset
  none`), reboot, and try again.

> **Heltec V3 contact limit:** the `_mqtt` image keeps 150 contacts. Flashing it
> over a `usb_tcp` node with more contacts loads the first 150, and the next time
> contacts are saved the rest are gone. Export the contacts first if they matter.

---

# Switching between this firmware and DMC's

The node key, `/mqtt.json` (brokers, slots, region, timezone) and the contacts
carry over in every direction. Wi-Fi works like this:

| Reflash | Wi-Fi |
| --- | --- |
| DMC observer to meshcomod `_mqtt` | If this companion has no Wi-Fi saved, it takes the SSID from `/mqtt.json` once, at boot. |
| meshcomod `_mqtt` to DMC Companion Observer | DMC reads meshcomod's saved Wi-Fi on its first boot. |
| meshcomod `_mqtt` to DMC repeater observer | Not carried over: set Wi-Fi there (serial or its setup AP). |

The companion never writes its Wi-Fi into `/mqtt.json`.

---

# SNMP

With `set snmp on`, the `_mqtt` builds also serve the observer's MQTT group
(see [companion_snmp.md](companion_snmp.md)):

| OID | Type | Description |
| --- | --- | --- |
| `.3.1.0` | INTEGER | Connected MQTT slots |
| `.3.2.0` | INTEGER | Packets waiting in the publish queue |
| `.3.3.0` | INTEGER | Publishes skipped under memory pressure |
