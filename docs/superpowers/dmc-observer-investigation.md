# Investigation: adding the DMC observer companion to meshcomod

Date: 2026-11-23. Read-only investigation, nothing changed in code.

Sources read:

* `Dutch-MeshCore/MeshCore`, branch `dmc-observer-dev` at `24c6636a` (2026-11-23,
  PR #40 "companion observer"), plus `dmc-dev` at `b9a2dff3` for the DMC docs
  (`dmc-docs/releases.en.md`, PR #43). Cloned into a scratch directory.
* This repo at `main` (`a509c903`).
* The in-flight MeshCore `dev` sync (`Chore/meshcore-dev-sync-2026-11`) only from the
  brief: it has no commits past `main` yet, so nothing on it was read.

Not used: `G:/Github/repositories/Elektr0Vodka/MeshCore-MQTT`. It is a checkout of
`Dutch-MeshCore/DutchMeshCore.nl-MQTT` (an older agessaman MQTT-bridge fork, branch
`mqtt-bridge-implementation-flex-dmc`, 256 commits behind its remote). It is not the
observer source.

The branch `enhancement/dmc-observer-dev-snmp-tier1` named in the brief no longer
exists; SNMP is on `dmc-observer-dev` itself (`src/helpers/SNMPAgent.cpp`,
`MQTT_SNMP.md`).

---

## 1. What "the dmc-observer companion" is

DMC (`dmc-observer-dev`) ships two observer firmwares. Only the second is a companion.

1. **Repeater observer** (`*_repeater_observer_mqtt`, `*_observer_mqtt`,
   `*_room_server_observer_mqtt`): `simple_repeater` / `simple_room_server` plus an
   on-device MQTT uplink, OTA, web config portal, SNMP, fault alerts, self-heal,
   neighbour publishing. Not a companion, out of scope here.
2. **Companion Observer** (version string `vX.Y.Z[-dev]-dutchmeshcore.nl-comp-obs-<hash>`):
   the stock `examples/companion_radio` with an MQTT observer compiled in behind
   `-D WITH_MQTT_BRIDGE=1`. The MeshCore app connects over Wi-Fi TCP port 5000 while
   every heard packet goes to the configured MQTT brokers. Status JSON carries
   `"role": "companion"`.

Origin: merged on 2026-11-23 from agessaman's `feat/companion-observer`
(commit `4f99b2e7`), with a DMC CI commit (`e9694745`) and the `-comp-obs` tag
(`e588d0c2`). DMC docs state it is **not yet tested on hardware**.

### Envs and boards (all ESP32-S3)

| Board | Env | Notes |
| --- | --- | --- |
| Heltec V4 | `heltec_v4_companion_observer_wifi` | PSRAM, 350 contacts, 5 active slots |
| Heltec V4 expansion kit | `heltec_v4_expansionkit_companion_observer_wifi` | V4 plus second I2C bus |
| Heltec V3 | `Heltec_v3_companion_observer_wifi` | no PSRAM: 150 contacts, 32-msg offline queue, 2 TLS slots |
| Xiao S3 WIO | `Xiao_S3_WIO_companion_observer_wifi` | PSRAM |
| Waveshare S3 LR1121 | `Waveshare_S3_LR1121_companion_observer_wifi` | PSRAM |

Build flags (V4): `ENABLE_WIFI_INTERFACE`, `ENABLE_ESP_MDNS`, `WITH_MQTT_BRIDGE=1`,
`MQTT_MAX_PACKET_SIZE=1024`, `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`, `ESP32_CPU_FREQ=160`,
`MQTT_WIFI_TX_POWER=WIFI_POWER_11dBm`, plus a pre-script `scripts/generate_cert_bundle.py`
that embeds `src/certs/x509_crt_bundle.bin`. Extra libs: `elims/PsychicMqttClient@^0.2.4`
(also vendored under `lib/PsychicMqttClient`), `ArduinoJson 7.4.3`, `NTPClient`,
`Timezone`, `Time`, and DMC's `esp32_ota` set (`ESPAsyncWebServer 3.11.2`, `AsyncTCP 3.5.0`).
No `BLE_PIN_CODE`: the observer companion is **Wi-Fi only, no BLE**.

### What it adds over stock MeshCore companion

* MQTT uplink to up to 6 broker slots, each a preset (about 45 presets: letsmesh
  analyzers, `dutchmeshcore-1/2`, meshmapper, MeshRank, regional brokers) or `custom`.
  Nearly all presets are TLS (`wss://` with JWT signed by the node key, or `mqtts://`).
  Per-slot IATA region, packet-type filter, extension topics.
* Publishes `packets` (RX and TX, parsed JSON), `raw` (radio bytes with SNR/RSSI) and
  `status`, via `logRx` / `logTx` / `logRxRaw` hooks in `MyMesh`.
* Observer CLI (`get/set mqttN.*`, `mqtt.iata`, `wifi.ssid/pwd`, `wifi.txpower`,
  `wifi.ipv6`, `timezone`, `get mqtt.status`, `mqttN.diag`, NTP diag), reachable from
  USB serial (115200), the app's CLI and remote admin.
* Wi-Fi is owned by DMC's `NetworkLink` (credentials in `/mqtt.json`, joined by the MQTT
  task); the app TCP server is opened from `loop()` once the link has an address.
* `/mqtt.json` and the identity are shared with the repeater observer, so a node can be
  reflashed between the two roles and keep brokers, IATA, Wi-Fi, timezone and key.
  Node name, radio and `repeat.disable` carry over through upstream's `/prefs.json`.
* On PSRAM boards the whole `MyMesh` object is placed in PSRAM
  (`COMPANION_MESH_IN_PSRAM`) to leave internal DRAM for Wi-Fi and TLS.
* Crash report capture at boot, `NodeDiagCLI` (memory / radio diag), OLED line
  `MQTT n/m up`, live mDNS name refresh.
* DMC's companion receive filter (`filter ...`) is included.

Explicitly **not** in the Companion Observer: pull OTA, SNMP, web portal, fault alerts,
MQTT self-heal, neighbour publishing. Update over USB.

### Base version

`dmc-observer-dev` = `dmc-dev` (MeshCore `dev` at `33cd5dbd`, synced 2026-11-15, reports
`v1.18.0`) + agessaman `observer-firmware-dev` (catch-up 2026-11-22) + agessaman
`feat/companion-observer`. So it sits on roughly the same upstream `dev` that the
in-flight `Chore/meshcore-dev-sync-2026-11` brings into this fork. DMC already has
`NmeaGpsDriver` in its tree.

DMC's companion is upstream's companion: settings in **`/prefs.json`** (upstream JSON
prefs, `FIRMWARE_VER_CODE 14`). It also carries code ported from this fork:
`CompanionSNMP.cpp` ("Ported from meshcomod"), `CompanionOta.cpp`, `CompanionNet.cpp`,
and `MeshcomodImport.cpp` / `MeshcomodPrefs.h`, which read this fork's 141-byte binary
`/new_prefs` on the first boot after a meshcomod-to-DMC flash.

### Configuration and release at DMC

* Setup: `set wifi.ssid`, `set wifi.pwd`, `set mqtt.iata <code>`, `set mqtt1.preset <name>`,
  reboot.
* Release: the "DMC Firmware Release" workflow button, firmware `companion-observer`,
  branch `dmc-observer-dev`, envs `*_companion_observer_wifi`. Channel `dev` rebuilds the
  rolling pre-release `dmc-companion-observer-dev`; channel `release` creates a draft
  `dmc-companion-observer-vX.Y.Z`. A dispatch-only fallback workflow
  `build-companion-observer-firmwares.yml` exists. The rolling build feeds the Toolbox
  flasher only, never the OTA server. The Toolbox flasher already lists the meshcomod
  companion too.

---

## 2. How it differs from this fork's companion

| Area | meshcomod companion (`main`) | DMC Companion Observer |
| --- | --- | --- |
| App transports | USB + TCP 5000 + WebSocket 8765 + BLE at once (`MULTI_TRANSPORT_COMPANION`) | Wi-Fi TCP 5000 only; USB is a CLI console |
| Wi-Fi owner | `main.cpp` state machine, creds in NVS (`WifiRuntimeStore`), BLE coexistence, `wifi.enabled`, `wifi.apply`, touch UI | `NetworkLink` singleton, creds in `/mqtt.json` |
| Prefs | binary `NodePrefs` in `/new_prefs` (141 bytes, CAD byte) | upstream `/prefs.json` (ConfigSerializer) |
| SNMP | `CompanionSNMP.cpp`, OIDs .1 to .5, .9, .10; `.3.4.0` only in the MQTT group | none |
| OTA | manifest OTA (`ota check/update/url`) | none |
| Clock | SNTP kicked from `main.cpp` when Wi-Fi associates | MQTTBridge's own NTP + `Timezone` |
| Capture | `FullCapture`, `logRxRaw`, `logRxBad` | `logRx`, `logTx`, `logRxRaw` to MQTT |
| Boards | about 59 envs, ESP32 + nRF52 + RP2040 | 5 ESP32-S3 envs |

Overlap of boards: Heltec V4, Heltec V3 and Xiao S3 WIO are in both lists. Heltec V4 R8,
V4 TFT and Xiao S3 are ours only. Waveshare S3 LR1121 and the V4 expansion kit are DMC
only.

---

## 3. Size of the observer stack

Include closure from `CompanionObserver`, `MQTTBridge`, `ObserverConfig`, `ObserverCLI`,
`NetworkLink`, `JWTHelper`, `MQTTMessageBuilder`, `MQTTPayloadBuilder`, `CrashReport`,
`NodeDiagCLI`: **75 files, about 22,100 lines that do not exist in this fork** (largest:
`MQTTBridge.cpp` 6,087, `ObserverCLI.cpp` 1,628, `NetworkLink.cpp` 1,200,
`ObserverConfig.cpp` 818, MQTTPrefs codec/serializer/storage about 1,800). Some of that
closure is repeater baggage that can be trimmed (`AlertReporter`, `SNMPAgent`,
`CommonGpsPrefs`, `GpsGuard`).

Files that exist in both trees but differ: `CommonCLI.h` (+212/-15 at DMC: it embeds the
repeater `NodePrefs` ConfigSerializer class and includes `MQTTPrefsStorage.h` and
`ObserverConfig.h`), `CommonCLI.cpp` (+431/-127), `Dispatcher.h` (+32/-21, likely the
upstream `dev` delta), `IdentityStore.h` (-4). `ObserverCLI.cpp` includes `CommonCLI.h`
only for `StrHelper`, so that include can be pointed at `TxtDataHelpers.h` instead of
taking DMC's `CommonCLI`.

The companion-side glue itself is small: about 200 lines across `MyMesh.cpp/.h`,
`main.cpp` and one UI line, plus the 260-line `CompanionObserver` wrapper.

---

## 4. Conflicts with our transport and prefs

1. **Wi-Fi ownership (biggest).** Our `main.cpp` owns Wi-Fi (NVS creds, `WiFi.begin`,
   BLE coexistence modem sleep, `wifi.enabled` off/on, touch settings). DMC's
   `NetworkLink` wants to own it too, from `/mqtt.json`. Good news: `NetworkLink` is an
   abstract interface with a build-selected singleton (`activeNetworkLink()`), and
   compatibility Wi-Fi builds call `begin()` / `maintain()` from the MQTT task. A
   passive `ExternalWiFiNetworkLink` (begin is a no-op, status read from `WiFi`) lets
   our Wi-Fi manager stay in charge with no change to `main.cpp` Wi-Fi logic.
   Cost of that choice: Wi-Fi creds no longer round-trip through `/mqtt.json` when a node
   is reflashed to a DMC repeater observer. Mirroring our creds into `/mqtt.json` on
   `wifi.apply` would keep that.
2. **Internal RAM.** `docs/COMPANION_OTA_MEMORY_ROOT_CAUSE.md` shows one HTTPS session
   already failed with `SSL - Memory allocation failed` while BLE was up. MQTT keeps 1 to
   6 TLS sessions open permanently (about 40 KB internal heap each, per DMC). DMC drops
   BLE entirely and moves `MyMesh` to PSRAM. For us that means: PSRAM boards first, adopt
   `COMPANION_MESH_IN_PSRAM`, cap active slots (2 by default), and probably refuse MQTT
   while BLE is enabled on boards without PSRAM. Needs a heap reading on hardware.
3. **Prefs.** No `NodePrefs` layout change is needed: MQTT settings live in their own
   `/mqtt.json` (DMC codec, kept byte-compatible by vendoring it unchanged), and the
   bridge only asks the host for node name, radio params, forwarding state and role
   through `MQTTBridgeHost`. The repeater-to-companion `/prefs.json` carry-over (node
   name, radio, repeat) does not apply, because we keep binary `/new_prefs`. Our
   `DataStore` root prefix (`_rp()`, SD under Launcher) must be honoured when passing
   the filesystem.
4. **CLI.** Name collisions: `get/set wifi.*` (ours: ssid, pwd, clear, apply, enabled,
   status, ip, mac, radio), `ota` (ours has manifest OTA, the observer refuses `ota`),
   `timezone`, NodeDiagCLI's `memory` / `stats-*`. Route `mqtt*`, `timezone` and NTP
   diag to the observer, keep `wifi.*`, `ota`, `snmp` on ours. `cli_command` has to grow
   from 80 to 161 bytes for broker hostnames and tokens.
5. **Packet hooks.** We already override `logRxRaw` (UI echo tracking, full capture) and
   `logRxBad`; adding `logRx` / `logTx` overrides is additive.
6. **Clock.** Both our SNTP kick and MQTTBridge's NTP would set the clock. Keep one;
   `.3.4.0` should keep meaning "clock set by NTP this boot".
7. **SNMP contract.** With MQTT in the build, our agent can register `.3.1.0` (connected
   slots), `.3.2.0` (queue depth) and `.3.3.0` (skipped publishes) with the observer's
   meaning and types. That stays inside the contract (observer owns .1 to .8, companion
   adds .9 and .10) and needs a `docs/companion_snmp.md` update.
8. **Libraries.** New: PsychicMqttClient (esp-mqtt based), ArduinoJson 7.4.3, NTPClient,
   Timezone, Time, the cert bundle pre-script and embedded bundle. DMC pins
   `ESPAsyncWebServer 3.11.2` / `AsyncTCP 3.5.0`, we pin `3.10.3`; only relevant if the
   web portal were ported (it should not be).
9. **After the dev sync.** UIs move to `src/helpers/ui/companion-*`, `ui_task` moves to
   `target.cpp`, GPS becomes `NmeaGpsDriver`. The observer touches only one UI line and no
   GPS code, and `Dispatcher.h` should converge with DMC's after the sync, so doing this
   **after** the sync lands is cheaper than before.
10. **Cross-flash with DMC.** Identity files match. DMC's `MeshcomodImport` reads our
    141-byte `/new_prefs`; any future `NodePrefs` growth here should be announced to it.
    Going from DMC back to meshcomod leaves `/prefs.json` and `/mqtt.json` unused and
    reads the stale `/new_prefs` DMC left behind.

---

## 5. Options

### A. Do nothing here, point to DMC's build

The Toolbox flasher already offers both. Zero work. Users who want MQTT lose BLE,
WebSocket, USB app link, SNMP and OTA.

### B. Vendor DMC's whole companion as a second example (`examples/companion_observer`)

Builds DMC's companion as-is from this tree. It brings DMC's `/prefs.json` DataStore, its
own filter, SNMP and OTA copies, and its `CommonCLI` changes, so two companion codebases
diverge in one repo, and devices flipping between them hit the binary/JSON prefs split.
High maintenance, little gain over option A. **Not recommended.**

### C. Port the observer into our companion behind a build flag (recommended)

`-D WITH_MQTT_BRIDGE=1` on new `*_companion_radio_usb_tcp_mqtt` envs (or on the existing
PSRAM envs once proven), keeping USB + TCP + WS (+ BLE where RAM allows), SNMP, manifest
OTA and full capture.

* Vendor the observer helper set unchanged from a pinned DMC commit into `src/helpers/`
  (and `lib/PsychicMqttClient` if DMC's vendored copy has patches), recorded with the
  source commit so later syncs are a file refresh, not a merge. Do not merge DMC branches:
  they carry the whole DMC repeater stack.
* Add our own `CompanionObserver` glue in `examples/companion_radio/` (DMC's wrapper is a
  good template), a passive `NetworkLink`, CLI routing, SNMP `.3.1` to `.3.3`, OLED line.
* Release through `companion_boards.ini` like every other env; README table regenerated;
  `changelog_ev.md` entry.

Rough size:

| Phase | Content | Size |
| --- | --- | --- |
| 1 | Vendor helpers, passive link, glue, CLI routing, Heltec V4 env, build green | about 22k lines copied, 400 to 700 lines written; 2 to 3 days |
| 2 | Heap work: mesh in PSRAM, slot cap, BLE policy, hardware soak on V4 | 1 to 2 days plus soak time |
| 3 | SNMP `.3.x`, docs (`companion_mqtt.md`, SNMP doc), more boards | 1 day |
| 4 | Optional: mirror Wi-Fi creds into `/mqtt.json` for DMC cross-flash | half a day |

### D. Upstream contribution instead

Offer meshcomod's multi-transport to DMC so their Companion Observer gains BLE/WS/USB.
Larger and against DMC's "downstream-only" stance; not a fit for this repo's goals.

---

## 6. Decisions (user, 2026-11-23)

* **BLE must stay toggleable** on the MQTT builds.
* **Separate `_mqtt` envs**, for example `heltec_v4_companion_radio_usb_tcp_mqtt`. The
  existing `usb_tcp` envs stay as they are.
* **Base: `dmc-observer-dev`.** The helpers are copied from a pinned DMC commit, not from
  agessaman's branches.
* **The mbedTLS side finding (7.1) is part of the MQTT work**, not a separate PR.

---

## 7. How DMC fits the Heltec V3 (and the other no-PSRAM limits)

DMC (agessaman's work, merged into `dmc-observer-dev`) uses several measures together. All
of them are in the tree at `24c6636a`.

### 7.1 Reduced-TLS mbedTLS archives (the big one)

`docs/mbedtls-tls-footprint.md` (commit `fda23612`, 2026-11-10):

* Root cause: `framework-arduinoespressif32` for `espressif32@6.11.0` (Arduino 2.0.17,
  IDF 4.4) ships mbedTLS precompiled with symmetric 16 KiB in and out record buffers. Each
  TLS session therefore needs **2 x 16 KiB of contiguous internal DRAM**. Every reconnect
  frees and reallocates those buffers, so the largest free block shrinks over time:
  measured on a V3 with two WSS slots, it fell from 62,452 to 16,372 bytes over 50
  reconnects.
* **A project `-D` cannot change this**, because the buffer sizes are compiled into
  `libmbedtls.a`. DMC rebuilds the four mbedTLS archives with
  `ASYMMETRIC_CONTENT_LEN=y`, `OUT_CONTENT_LEN=4096` (IN stays 16384) and
  `KEEP_PEER_CERTIFICATE=n`. That saves about 16 KiB per connection.
* Plumbing: `scripts/mbedtls_4k.py` (a pre-script in `esp32_base.extra_scripts`),
  `scripts/fetch_mbedtls_4k.sh`, and `scripts/mbedtls_4k_manifest.txt`, which pins
  sha256 hashes of the replacement archives and fingerprints of the stock archives. The
  build stops if the framework changes underneath. `MESHCORE_REDUCED_TLS=auto` applies
  it to every env that has `WITH_MQTT_BRIDGE` and no `BOARD_HAS_PSRAM`. `build.sh` sets
  `auto` for observer builds. Only `esp32s3` archives exist; classic ESP32 builds stock.
  A link-map check prints `reduced-TLS: <env>: active`, and the boot log prints
  `TLS outbound record: 4096 bytes (reduced)`.
* The archives are downloaded from **`agessaman/MeshCore` releases, tag `mbedtls-4k`**
  (the default `BASE_URL`; `MBEDTLS_4K_BASE_URL` overrides it). Same platform pin as ours
  (`platformio/espressif32@6.11.0`), so the same archives fit our builds.
* Measured on a V3 with two WSS slots, repeater observer: boot free heap went from 70.3 K
  to 94.9 K and the largest block from 52.2 K to 75.8 K. After four slot reconfigures the
  reduced build still had a 73.7 K block, while the stock build was down to 37.9 K after
  two.

**Side finding for this fork:** our `usb_tcp` envs set
`-D CONFIG_MBEDTLS_ASYMMETRIC_CONTENT_LEN=1`, `-D CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=16384`
and `-D CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=4096` (for example `variants/heltec_v3/platformio.ini`,
`Heltec_v3_companion_radio_usb_tcp`). By DMC's analysis these are no-ops: the linked
archive still uses 16 KiB out buffers. That probably explains part of
`docs/COMPANION_OTA_MEMORY_ROOT_CAUSE.md`. It still has to be confirmed with a link map
or a heap reading. The reduced-TLS archives would help our HTTPS OTA too, with or without MQTT.

### 7.2 Static caps on the V3 companion observer

`Heltec_v3_companion_observer_wifi` (commit `2eb0414d`, measured on hardware by agessaman
with the app connected):

* `MAX_CONTACTS=150` (we ship 350 on V3), `OFFLINE_QUEUE_SIZE=32`, `MAX_GROUP_CHANNELS=40`.
* At most 2 active MQTT slots: `MQTTBridge::getMaxActiveSlots()` returns
  `MQTT_MAX_ACTIVE_SLOTS_NO_PSRAM` (default 2) without PSRAM and 5 with it. Further
  slots stay saved and show `(inactive)`.
* Result with two TLS slots up: 51 KB free internal heap, 34.8 KB largest block, 36 KB
  minimum since boot. One slot left 114 KB free. These numbers were taken **before** the
  reduced-TLS archives existed, so with them V3 should have roughly 25 KB more.
* No `ENABLE_ESP_MDNS` on V3 (V4 has it).
* The smaller `adafruit` cert bundle on V3. V4 and Xiao use `adafruit-full` (commit
  `5addae82`), so a broker that verifies on the repeater observer also verifies on the
  companion.
* No BLE at all.

### 7.3 Other memory measures in the observer stack

* `scripts/asynctcp_stack.py` (commit `dedb33bd`): caps AsyncTCP's never-deleted service
  task stack from 16 KiB to 8 KiB on `WITH_MQTT_BRIDGE` builds without PSRAM (measured
  peak 2.2 KB). We link AsyncTCP through `esp32_ota`, so this applies to us too.
* `mbedtls_platform_set_calloc_free(psram_calloc, psram_free)` once per boot on PSRAM
  boards (commit `6d1e7bec`). All TLS, ours included, then allocates from PSRAM. That
  would also remove the OTA TLS allocation failure on PSRAM boards.
* `COMPANION_MESH_IN_PSRAM` (commits `3a9f5eaa`, `cca5dd4a`): on a V4.2 with five brokers,
  internal heap had dropped to 29 KB free with a 14 KB largest block. Moving `MyMesh`
  (contacts, offline queue, frame buffers, about 120 KB) into PSRAM, allocated at the
  start of `setup()`, cut static internal RAM from 177 KB to 54 KB.
* JSON scratch pools and bridge text buffers in PSRAM; no stack fallback buffers on the
  MQTT task (a missing buffer skips the publish and counts as a drop, SNMP `.3.3.0`).
* Wi-Fi: `ESP32_CPU_FREQ=160`, and a TX power cap of `MQTT_WIFI_TX_POWER=WIFI_POWER_11dBm`,
  adjustable at runtime with `set wifi.txpower` and reapplied after every association.
* Self-heal reboots, the neighbour table, the web portal and fault alerts are left out of
  the companion build.

### 7.4 What this means with BLE kept

Our V3 `usb_tcp` image already runs BLE + Wi-Fi + TCP + WS with 350 contacts, and our own
touch build gates BLE co-init on 50 KB free / 20 KB largest block. Our BLE toggle
(`MultiTransportCompanionInterface::enableBle/disableBle`, persisted with
`wifiConfigSetBleEnabled`) **does not free RAM when switched off**: NimBLE stays
resident, because deinit while Wi-Fi coexistence is active crashes. Only the OTA path
calls `NimBLEDevice::deinit(true)`. So on the MQTT builds the toggle has to work like this:

* **BLE off at boot**: NimBLE is never started and its RAM goes to TLS. Switching it on
  later brings it up lazily (the code already does that through the stored
  `prepareBle()` params), but only if the heap gate passes.
* **BLE switched off at runtime**: advertising stops at once; the RAM comes back after
  the next reboot. The UI and CLI should say so.
* **Slot budget follows BLE**: with BLE on at boot, the active-slot cap on no-PSRAM boards
  drops by one (2 becomes 1 on V3). This needs a small hook in `getMaxActiveSlots()` or a
  value set before the bridge starts.
* **Runtime heap gate** before a slot connects or BLE comes up (DMC has only the static
  cap). Reuse our touch thresholds for BLE (50 KB free, 20 KB block) and require about
  40 KB contiguous (20 KB with reduced TLS) before opening a TLS slot.

---

## 8. Recommendation (updated)

Option C with the decisions above. Order of work:

1. **After** `Chore/meshcore-dev-sync-2026-11` is merged, copy the observer helper set,
   `lib/PsychicMqttClient`, `scripts/generate_cert_bundle.py`, `scripts/mbedtls_4k.py`,
   `scripts/fetch_mbedtls_4k.sh`, `scripts/mbedtls_4k_manifest.txt` and
   `scripts/asynctcp_stack.py` from a pinned `dmc-observer-dev` commit. Record that commit
   in a short `docs/` note so the next refresh is a diff.
2. Our own glue: a `CompanionObserver`-style wrapper, a passive `NetworkLink` (our Wi-Fi
   manager stays the owner), the `logRx`/`logTx` hooks, CLI routing, and
   `COMPANION_MESH_IN_PSRAM` plus the PSRAM mbedTLS allocator on PSRAM boards.
3. **Heltec V4** `heltec_v4_companion_radio_usb_tcp_mqtt` first: BLE on, 5 slots, mesh in
   PSRAM. Hardware soak with BLE on and off.
4. Heltec V4 R8, Xiao S3 WIO (and other PSRAM S3 boards on request).
5. **Heltec V3** `Heltec_v3_companion_radio_usb_tcp_mqtt` last, using DMC's recipe:
   reduced-TLS archives (`MESHCORE_REDUCED_TLS=auto`), AsyncTCP 8 KiB stack,
   `MAX_CONTACTS=150`, `OFFLINE_QUEUE_SIZE=32`, `adafruit` bundle, no mDNS, 2 slots with
   BLE off and 1 with BLE on, plus the heap gates from 7.4. Go or no-go after a heap
   reading on hardware with BLE, the app on BLE and one WSS slot all up.
6. SNMP `.3.1` to `.3.3`, `docs/companion_mqtt.md`, `docs/companion_snmp.md`,
   `companion_boards.ini` sections for the `_mqtt` envs, README table, changelog.

### Risks to note in the docs and on the release

* **Contact cap on V3.** Flashing the V3 `_mqtt` image over a V3 `usb_tcp` node with more
  than 150 contacts loads only the first 150, and the next save drops the rest. The
  release notes need a warning, or the V3 `_mqtt` image should refuse to save while the
  file still holds more contacts than it loaded.
* **Release CI.** Release CI must fetch the reduced-TLS archives. That means a binary
  download from `agessaman/MeshCore` during the build, checked against the pinned hashes.
  Mirroring the four archives to a release in this repo (the fetch script honours
  `MBEDTLS_4K_BASE_URL`) avoids depending on a third-party release staying online.
* **Not tested on hardware.** The DMC docs list the Companion Observer as not hardware
  tested, though agessaman's commits include V3 and V4.2 heap measurements. Our BLE, WS
  and SNMP additions on top are untested anyway.

## 9. Open questions and decisions

Decided (user, 2026-11-23):

* V3 `_mqtt` image: `MAX_CONTACTS=150`, as DMC does.
* The reduced-TLS archives are mirrored to a release in this repo; the build sets
  `MBEDTLS_4K_BASE_URL` to it. The hashes in `mbedtls_4k_manifest.txt` stay the check.

* Wi-Fi: **one-way adopt**. On boot, if our NVS has no SSID and `/mqtt.json` has one,
  copy it into NVS once. We never write Wi-Fi into `/mqtt.json`. The table below
  describes what that means per reflash direction.

Background, why Wi-Fi creds are not written into `/mqtt.json`:

Our Wi-Fi creds stay in NVS (`WifiRuntimeStore`, namespace `meshcomod`), and DMC's firmwares
read Wi-Fi from `/mqtt.json`. The observer's `wifi.*` commands are routed to our store, so
the Wi-Fi fields in `/mqtt.json` stay at whatever was there before. Brokers, slots, IATA,
timezone and identity are shared either way.

| Reflash | Without mirroring |
| --- | --- |
| meshcomod `_mqtt` to DMC **Companion Observer** | Wi-Fi still comes up. On first boot DMC's `importMeshcomodSettings()` (`MyMesh::begin`) reads our NVS `wifi_ssid` / `wifi_pwd` when `/prefs.json` is absent and `/new_prefs` is present. `CompanionObserver::loadPrefs` then adopts it into `/mqtt.json` if that has no SSID. |
| meshcomod `_mqtt` to DMC **repeater observer** | No import exists there. With an empty SSID in `/mqtt.json` the node has no Wi-Fi: set `wifi.ssid` / `wifi.pwd` over USB, or use the repeater's setup AP and web portal. If `/mqtt.json` still holds an SSID from an earlier DMC flash, the repeater joins that **old** network, which may be wrong. |
| DMC (either) to meshcomod `_mqtt` | NVS survives an update flash, so our old creds come back. They may be stale if Wi-Fi was changed on the DMC side. After a full erase, set Wi-Fi as on any meshcomod companion. |

Cheaper middle ground, recommended: a **one-way adopt** on our side, mirroring what DMC
does. On boot, if our NVS has no SSID and `/mqtt.json` has one, copy it into NVS once.
That covers DMC to meshcomod. Full mirroring (writing `/mqtt.json` on every `wifi.apply`)
is only needed for a smooth meshcomod to DMC repeater observer flash. It also brings two
copies of the creds that can drift apart.

Also decided: the side finding in 7.1 (our `-D CONFIG_MBEDTLS_*` flags are probably no-ops) is
**folded into the MQTT work**, not a separate PR. The MQTT PR brings in the reduced-TLS
plumbing (`mbedtls_4k.py`, fetch script, manifest) for the `_mqtt` envs. The same PR
confirms with a link map whether the existing `-D CONFIG_MBEDTLS_*` flags in the
`usb_tcp` envs are no-ops. Moving those envs to the reduced archives as well, and
dropping the dead flags, can then be done in that work too.
