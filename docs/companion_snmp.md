# Companion SNMP agent

## Overview

An optional SNMPv2c agent in the Wi-Fi companion firmware, so monitoring tools
(LibreNMS, Zabbix, PRTG, Nagios, RTFM-EV's SNMP poller) can poll a companion for
radio, memory and link health. It is ported from the Dutch-MeshCore observer
firmware (`dmc-observer-dev`, `MQTT_SNMP.md`) and uses the same OID tree, so one
poller reads both.

> Hardware testing note: verified to build for the envs below. It has not yet
> been tested on hardware.

* Protocol: SNMPv2c, read-only (GET, GETNEXT, GETBULK). No SET, no traps.
* Port: UDP 161.
* Library: [0neblock/SNMP_Agent](https://github.com/0neblock/Arduino_SNMP) 2.1.0.
* **Off by default.** `set snmp on` starts it, no reboot needed.

Builds with the agent (compile flag `-D WITH_SNMP=1`):

* `heltec_v4_companion_radio_usb_tcp`
* `Heltec_v3_companion_radio_usb_tcp` (and its `_boot_debug` variant)

The touch build and the other companions do not include it yet.

The agent only runs while Wi-Fi is up with an address. It stops when the link
drops and starts again when it comes back. While it is off, nothing is
allocated, so a build that contains it but leaves it off costs flash only.

---

# Quick start

```text
set snmp on
get wifi.ip
```

Then, from a host on the same network:

```bash
snmpwalk -v2c -c public <companion-ip> 1.3.6.1.4.1.99999
```

---

# Commands

The commands run through the companion CLI: from the app's command console
(companion protocol `CMD_RUN_CLI_COMMAND`), over USB serial, or as remote CLI
from a contact that is allowed to use it.

| Command | Reply |
| --- | --- |
| `get snmp` | `> on` or `> off` |
| `set snmp on` / `set snmp off` | `> snmp is now on` (applies live) |
| `get snmp.status` | `> running on udp/161`, `> on, waiting for Wi-Fi`, or `> off` |
| `get snmp.community` | `> public` (local only; remote CLI gets an error) |
| `set snmp.community <string>` | `> snmp.community updated` (1 to 23 characters, applies live) |

The community string is the only access control SNMPv2c has, and it travels in
clear text. Change it from `public` on any network you do not fully trust.

Settings are kept in NVS next to the Wi-Fi settings, not in the node prefs file,
so they survive a firmware update and do not change the prefs layout.

---

# OID reference

All OIDs are under the temporary, unregistered enterprise number
`1.3.6.1.4.1.99999`, the same one the observer firmware uses. All values are
INTEGER unless noted, read at the moment the request arrives.

## System (.1.x.0), same as the observer

| OID | Description |
| --- | --- |
| `.1.1.0` | Uptime (seconds) |
| `.1.2.0` | Firmware version (STRING) |
| `.1.3.0` | Node name (STRING) |
| `.1.4.0` | Battery voltage (mV, 0 when the board cannot measure it) |

`.1.4.0` is new in both firmwares: it was added to the observer agent at the same
time (Dutch-MeshCore `dmc-observer-dev`).

## Radio (.2.x.0), same as the observer

| OID | Description |
| --- | --- |
| `.2.1.0` | Packets received |
| `.2.2.0` | Packets sent |
| `.2.3.0` | Receive errors |
| `.2.4.0` | Noise floor (dBm) |
| `.2.5.0` | Last RSSI (dBm) |
| `.2.6.0` | Last SNR (dB x 4) |
| `.2.7.0` | Flood packets sent |
| `.2.8.0` | Direct packets sent |
| `.2.9.0` | Flood packets received |
| `.2.10.0` | Direct packets received |
| `.2.11.0` | TX air time (seconds) |

## MQTT (.3.x.0)

Not present. The companion has no MQTT bridge. A poller asking for these gets
`noSuchObject`.

## Memory (.4.x.0), same as the observer

| OID | Description |
| --- | --- |
| `.4.1.0` | Free heap (bytes) |
| `.4.2.0` | Largest allocatable block (bytes) |
| `.4.3.0` | Free internal RAM (bytes) |
| `.4.4.0` | Free PSRAM (bytes, 0 without PSRAM) |

## Network (.5.x.0), same as the observer

| OID | Description |
| --- | --- |
| `.5.1.0` | Wi-Fi RSSI (dBm, -127 when not connected) |

## Companion (.6.x.0), companion only

| OID | Description |
| --- | --- |
| `.6.1.0` | Connected TCP companion clients |
| `.6.2.0` | Connected WebSocket clients |
| `.6.3.0` | Contacts |
| `.6.4.0` | Group channels in use |
| `.6.5.0` | RX air time (seconds) |

---

# Size

Measured with `pio run`, `DISABLE_DEBUG=1`, against the same tree without
`WITH_SNMP`:

| Env | Flash | Static RAM | App slot used |
| --- | --- | --- | --- |
| `Heltec_v3_companion_radio_usb_tcp` | +33,556 B | +336 B | 48.6% of 3.19 MB |
| `heltec_v4_companion_radio_usb_tcp` | +33,340 B | +344 B | 24.8% of 6.25 MB |

The static RAM figure is the cost while the agent is off. Turning it on
allocates the agent (a 1.4 KB packet buffer plus the OID table) and a UDP socket
on the heap, a few KB in total. The `[snmp] agent up` line on the serial log
prints free heap at that moment.

---

# Adding it to another companion env

The env must be a Wi-Fi companion (`MULTI_TRANSPORT_COMPANION`). Add to its
`build_flags`:

```ini
  -D WITH_SNMP=1
```

and to its `lib_deps`:

```ini
  0neblock/SNMP_Agent @ 2.1.0
```

Measure internal RAM before shipping it on a board where BLE and Wi-Fi already
compete for heap, such as the touch build.
