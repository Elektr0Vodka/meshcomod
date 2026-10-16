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
* `Heltec_Wireless_Paper_companion_radio_usb_tcp`
* `Xiao_S3_WIO_companion_radio_usb_tcp`
* `heltec_v4_r8_companion_radio_usb_tcp`
* `heltec_v4_r8_tft_companion_radio_usb_tcp`
* `heltec_v4_tft_companion_radio_usb_tcp` (V4 TFT without touch, `ui-new` screens)
* `Heltec_WSL3_companion_radio_usb_tcp`

The touch build does not include it yet: it needs a heap reading on hardware
first, because BLE co-init there is already gated on free heap.

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
`1.3.6.1.4.1.99999`. The layout follows the observer firmware's agent
(Dutch-MeshCore `dmc-observer-dev`, `MQTT_SNMP.md`): every number the two share
has the same meaning, type, unit and "no value" marker, so one poller template
reads both.

* Levels (queue lengths, voltages, temperatures) are INTEGER.
* Running totals from `.2.13.0` on, and the receive-filter totals, are Counter32,
  so pollers graph them as rates. They start from 0 after a reboot.
* The older totals in `.2.1.0` to `.2.11.0` stay INTEGER to match the observer.
* An OID the companion has no data for is not registered and reads
  `noSuchObject`. It is never served as a 0 that looks like a real reading.
* Companion-only values use `.9` and up. The observer keeps `.1` to `.8`.

## System (.1.x.0)

| OID | Type | Description |
| --- | --- | --- |
| `.1.1.0` | INTEGER | Uptime (seconds) |
| `.1.2.0` | STRING | Firmware version |
| `.1.3.0` | STRING | Node name |
| `.1.4.0` | INTEGER | Battery voltage (mV), 0 when the board cannot measure it. Read every 30 s |
| `.1.5.0` | INTEGER | MCU die temperature (whole °C), -127 when the board has no sensor. Read every 30 s |
| `.1.6.0` | INTEGER | Reset reason code for this boot (ESP-IDF `esp_reset_reason_t`: 1 power-on, 3 software, 4 panic, 5/6/7 watchdog, 9 brownout) |
| `.1.7.0` | STRING | Reset reason as text, for example `Brownout (low voltage)` |

## Radio (.2.x.0)

| OID | Type | Description |
| --- | --- | --- |
| `.2.1.0` | INTEGER | Packets received |
| `.2.2.0` | INTEGER | Packets sent |
| `.2.3.0` | INTEGER | Receive errors |
| `.2.4.0` | INTEGER | Noise floor (dBm) |
| `.2.5.0` | INTEGER | Last RSSI (dBm) |
| `.2.6.0` | INTEGER | Last SNR (dB x 4) |
| `.2.7.0` | INTEGER | Flood packets sent |
| `.2.8.0` | INTEGER | Direct packets sent |
| `.2.9.0` | INTEGER | Flood packets received |
| `.2.10.0` | INTEGER | Direct packets received |
| `.2.11.0` | INTEGER | TX air time (seconds) |
| `.2.12.0` | INTEGER | Packets waiting in the transmit queue |
| `.2.13.0` | Counter32 | Duplicate flood packets received |
| `.2.14.0` | Counter32 | Duplicate direct packets received |
| `.2.15.0` | Counter32 | RX air time (seconds) |
| `.2.17.0` | INTEGER | Error flags since boot (bitmask: 1 packet pool full, 2 CAD timeout, 4 start-RX timeout, 8 radio watchdog) |

Not present: `.2.16.0` (TX duty cycle %). This firmware does not track a
rolling duty cycle. Bit 8 of `.2.17.0` is never set either, because the
companion has no radio watchdog.

## MQTT (.3.x.0)

| OID | Type | Description |
| --- | --- | --- |
| `.3.4.0` | INTEGER | Clock set by NTP this boot (1 yes, 0 no) |

The companion has no MQTT bridge, so `.3.1.0` to `.3.3.0` are not present.
`.3.4.0` is, because the companion sets its clock over SNTP once Wi-Fi is up.
Time pushed from the app does not count.

## Memory (.4.x.0)

| OID | Type | Description |
| --- | --- | --- |
| `.4.1.0` | INTEGER | Free heap (bytes) |
| `.4.2.0` | INTEGER | Largest allocatable block (bytes) |
| `.4.3.0` | INTEGER | Free internal RAM (bytes) |
| `.4.4.0` | INTEGER | Free PSRAM (bytes, 0 without PSRAM) |
| `.4.5.0` | INTEGER | Lowest free heap since boot (bytes). A value that keeps dropping points to a leak |

## Network (.5.x.0)

| OID | Type | Description |
| --- | --- | --- |
| `.5.1.0` | INTEGER | Wi-Fi RSSI (dBm, -127 when not connected) |

## Not present on the companion

`.6` (duty gate), `.7` (repeater packet filter) and `.8` (neighbours) are
repeater features and are not registered.

## Companion (.9.x.0), companion only

| OID | Type | Description |
| --- | --- | --- |
| `.9.1.0` | INTEGER | Connected TCP companion clients |
| `.9.2.0` | INTEGER | Connected WebSocket clients |
| `.9.3.0` | INTEGER | Contacts |
| `.9.4.0` | INTEGER | Group channels in use |

## Receive filter (.10.x.0), companion only

The companion receive filter (`filter` CLI, see
[companion_receive_filter.md](companion_receive_filter.md)). These count
messages kept from the user, not packets kept off the air, so they do not share
the repeater filter's `.7` group. Totals start from 0 after a reboot
(`filter reset` clears the settings, not the totals).

| OID | Type | Description |
| --- | --- | --- |
| `.10.1.0` | INTEGER | Filter enabled (1 yes, 0 no) |
| `.10.2.0` | Counter32 | Malformed group texts dropped |
| `.10.3.0` | Counter32 | Group texts dropped for age |
| `.10.4.0` | Counter32 | Packets dropped by the path-prefix block list |
| `.10.5.0` | Counter32 | Group texts hidden by sender rules |
| `.10.6.0` | Counter32 | Group texts hidden by text rules |

## How values are read

The agent runs in the main loop, with the mesh. Most values are read when the
request arrives. Battery and MCU temperature are read every 30 s and served
from a copy, because the Heltec V4 battery read blocks for 10 ms. The Counter32
totals are copied every 2 s. The reset reason is read once, when the agent
first starts.

---


# Size

Measured with `pio run`, `DISABLE_DEBUG=1`, against the same tree without
`WITH_SNMP`:

| Env | Flash | Static RAM | App slot used |
| --- | --- | --- | --- |
| `Heltec_v3_companion_radio_usb_tcp` | +33,556 B | +336 B | 48.6% of 3.19 MB |
| `heltec_v4_companion_radio_usb_tcp` | +33,340 B | +344 B | 24.8% of 6.25 MB |
| `Heltec_Wireless_Paper_companion_radio_usb_tcp` | +33,156 B | +344 B | 47.0% of 3.19 MB |
| `Xiao_S3_WIO_companion_radio_usb_tcp` | +33,020 B | +344 B | 46.9% of 3.19 MB |

Wireless Paper and Xiao start from about 40 KB more static RAM than the V3
(150 KB vs 110 KB, mostly the larger offline queue and, on the Paper, the e-ink
buffer), so they are the boards to watch when checking free heap with Wi-Fi,
BLE and the agent all running.

The newer envs were added with the agent already in, so there is no build
without it to compare against. Their totals (`build.sh build-firmware`, with
manifest OTA, `DISABLE_DEBUG=1`):

| Env | Flash | Static RAM | App slot used |
| --- | --- | --- | --- |
| `heltec_v4_r8_companion_radio_usb_tcp` | 1,635,957 B | 110,448 B | 25.0% of 6.25 MB |
| `heltec_v4_r8_tft_companion_radio_usb_tcp` | 1,639,833 B | 110,584 B | 25.0% of 6.25 MB |
| `heltec_v4_tft_companion_radio_usb_tcp` | 1,634,033 B | 110,612 B | 24.9% of 6.25 MB |
| `Heltec_WSL3_companion_radio_usb_tcp` | 1,609,669 B | 110,248 B | 48.2% of 3.19 MB |

These deltas were measured when the agent first went in, before the
observer-aligned OIDs (`.1.5.0` onwards, `.9`, `.10`). With the full current
OID set, `Heltec_v3_companion_radio_usb_tcp` builds to 1,627,545 B flash and
110,576 B static RAM, and `heltec_v4_companion_radio_usb_tcp` to 1,625,713 B
and 110,564 B (`pio run`, `DISABLE_DEBUG=1`, including manifest OTA).

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
