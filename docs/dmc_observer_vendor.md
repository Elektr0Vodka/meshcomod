# Vendored Dutch-MeshCore observer code

The `_mqtt` companion envs build the MQTT observer from
[Dutch-MeshCore/MeshCore](https://github.com/Dutch-MeshCore/MeshCore), branch
`dmc-observer-dev`. The files are copied, not merged, so a refresh is a file
diff instead of a merge of DMC's whole repeater stack.

Pinned commit: `24c6636a572f5f293cc1ae73afbec0006ffbcfe9`

## Refreshing

```bash
git clone https://github.com/Dutch-MeshCore/MeshCore.git /tmp/dmc
bash scripts/vendor_dmc_observer.sh /tmp/dmc <new commit>
pio test -e native
```

Then build every `*_mqtt` env and one plain `usb_tcp` env. If a patch no longer
applies, fix it against the new upstream text and regenerate it with `git diff`.

## What is copied

`scripts/dmc_observer_files.txt` lists the files: the include closure of the
companion observer sources (`CompanionObserver`, `MQTTBridge`, `ObserverConfig`,
`ObserverCLI`, `NetworkLink`, the MQTT message and payload builders, `JWTHelper`,
`CrashReport` and the ESP32 network fixes), minus files this tree already has,
plus DMC's patched `lib/PsychicMqttClient`. Left out on purpose: DMC's
`SNMPAgent` (the companion has its own), `NodeDiagCLI`, the web config portal,
DMC's manifest OTA and the GPS guard.

The vendored `.cpp` files are kept out of `[arduino_base]` and added back only by
`[mqtt_observer]`, so no other env compiles them. The DMC native test suites
for this code (`test/test_mqtt_*`, `test/test_observer_*`, `test/test_network_*`,
`test/test_ntp_validation`, `test/test_wifi_*`) are copied unchanged.

DMC's `ConfigSerializer` parses more strictly than this fork's and adds
`defStrict`, which the `/mqtt.json` schema needs. The fork's own
`ConfigSerializer` (used by the repeater, room and sensor prefs) is left as it is.
The script writes DMC's version as a renamed copy,
`src/helpers/ObserverConfigSerializer.{h,cpp}`, and points `MQTTPrefsSerializer.h`
at it.

## Local patches

| Patch | Why |
| --- | --- |
| `0001-snmp-guard.patch` | DMC's bridge feeds its own SNMP agent under `WITH_SNMP`. The companion defines `WITH_SNMP` for its own agent, so the bridge's guards become `DMC_OBSERVER_SNMP`, which this fork never defines. |
| `0002-external-network-link.patch` | With `NETWORK_LINK_EXTERNAL_WIFI`, `activeNetworkLink()` returns `externalNetworkLink()`, a passive `ExternalWiFiNetworkLink` added to `NetworkLink.cpp` (it reuses that file's private base class for outage, DNS and diagnostics). It never starts, stops or reconnects Wi-Fi: the companion's own Wi-Fi manager stays the owner. |
| `0003-slot-cap-and-skipped.patch` | `MQTTBridge::setActiveSlotCap()` lowers the active slot limit at run time (fewer slots while BLE is on, on boards without PSRAM), and `getSkippedPublishes()` exposes the count for SNMP `.3.3.0`. |
| `0004-mqttprefs-include-and-no-alerts.patch` | `MQTTBridge.h` includes `MQTTPrefsStorage.h` itself (DMC gets it through its larger `CommonCLI.h`). With `OBSERVER_NO_FAULT_ALERTS` the `AlertReporter` class compiles out (it needs the repeater's prefs); the banned-channel helpers the observer CLI uses stay. |

The vendored code also uses a few small core APIs that DMC added and this tree now has too: `StrHelper::stripSurroundingQuotes`, `mesh::Radio::getPacketsRecvErrors()` (virtual, the RadioLib wrapper already had it), `mesh::Dispatcher::getErrFlags()`, and `MainBoard::otaFromManifest` / `otaCheck` (defaults answer "not supported").
