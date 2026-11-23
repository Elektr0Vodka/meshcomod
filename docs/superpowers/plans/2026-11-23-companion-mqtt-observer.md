# Companion MQTT Observer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** New `*_companion_radio_usb_tcp_mqtt` envs that run our multi-transport companion (USB + TCP + WS + toggleable BLE, SNMP, manifest OTA, full capture) plus DMC's MQTT observer uplink.

**Architecture:** DMC's observer helpers are copied unchanged, apart from three small local patches, from a pinned `Dutch-MeshCore/MeshCore` `dmc-observer-dev` commit by a script. They are compiled only under `WITH_MQTT_BRIDGE`. Our own glue (`examples/companion_radio/CompanionMqtt.*`) drives DMC's `CompanionObserver`. A passive `NetworkLink` lets our existing Wi-Fi manager stay the owner of Wi-Fi. Boards without PSRAM use DMC's reduced-TLS mbedTLS archives, mirrored to a release in this repo.

**Tech Stack:** PlatformIO `espressif32@6.11.0` (Arduino 2.0.17, IDF 4.4), PsychicMqttClient (esp-mqtt), ArduinoJson 7.4.3, NTPClient, Timezone, Time, googletest (native).

**Spec:** `docs/superpowers/dmc-observer-investigation.md` (sections 6 to 9 hold the decisions).

**Base branch:** `Chore/meshcore-dev-sync-2026-11` once merged to `main`; work on `Feat/companion-mqtt-observer`.

## Global Constraints

- No em-dashes or en-dashes in any text; no co-authoring trailers; conventional-commit prefixes (AGENTS.md).
- Vendored source: `Dutch-MeshCore/MeshCore` branch `dmc-observer-dev`, pinned commit `24c6636a` (refresh = rerun the script with a new commit).
- Existing envs (all `usb_tcp`, BLE, nRF52, RP2040) must build byte-for-byte unchanged in behavior: every vendored `.cpp` is guarded by `WITH_MQTT_BRIDGE` or `ESP_PLATFORM` and must compile to nothing elsewhere.
- BLE stays toggleable on `_mqtt` envs.
- Wi-Fi creds stay in NVS (`WifiRuntimeStore`). One-way adopt from `/mqtt.json` only; never write Wi-Fi into `/mqtt.json`.
- V3 `_mqtt`: `MAX_CONTACTS=150`, `OFFLINE_QUEUE_SIZE=32`, no `ENABLE_ESP_MDNS`, cert source `adafruit`, reduced TLS, AsyncTCP stack 8192, 2 MQTT slots with BLE off at boot and 1 with BLE on.
- PSRAM `_mqtt` envs: cert source `adafruit-full`, mesh object in PSRAM, PSRAM mbedTLS allocator, 5 slots.
- Reduced-TLS archives come from this repo's release tag `mbedtls-4k` (`MBEDTLS_4K_BASE_URL`), checked against the pinned hashes in `scripts/mbedtls_4k_manifest.txt`.
- SNMP contract: observer owns `.1` to `.8`, companion `.9` and `.10`. New: `.3.1.0` connected slots, `.3.2.0` queue depth, `.3.3.0` skipped publishes, all INTEGER as in `MQTT_SNMP.md`.
- Every PR adds a `changelog_ev.md` entry under Unreleased with its PR number.

## Review Focus

1. Flashing V3 `_mqtt` over a node with more than 150 contacts: the extra contacts must survive on flash until the user saves contacts. The release note must warn about this.
2. Empty or missing `/mqtt.json` on first boot: the bridge must start with the DMC default presets (analyzer-us, analyzer-eu) and must not touch Wi-Fi.
3. Wi-Fi down or credentials changed at runtime (`wifi.apply`): MQTT reconnects through the passive link without a reboot, and the app TCP/WS servers keep working.
4. BLE toggled on at runtime on V3 with 2 slots up: the heap gate must refuse BLE cleanly, with a reply that says why, and must never crash.
5. A remote admin `get mqtt1.password` or `get mqtt1.token`: the secret is masked (sender_timestamp != 0), as in DMC.

---

### Task 1: Vendor the observer helpers

**Files:**
- Create: `scripts/vendor_dmc_observer.sh`, `scripts/dmc_observer_files.txt`, `scripts/dmc_observer_patches/0001-snmp-guard.patch`, `scripts/dmc_observer_patches/0002-external-network-link.patch`, `scripts/dmc_observer_patches/0003-slot-cap-and-skipped.patch`, `docs/dmc_observer_vendor.md`
- Create (by the script): the files listed in `dmc_observer_files.txt` under `src/helpers/` and `lib/PsychicMqttClient/`
- Modify: `platformio.ini` (`[arduino_base]` excludes `-<helpers/MQTTMessageBuilder.cpp>` and `-<helpers/JWTHelper.cpp>`; `[env:native]` adds `+<../src/helpers/MQTTPayloadBuilder.cpp>`)
- Test: DMC's native suites copied unchanged: `test/test_mqtt_*` (all), `test/test_observer_bridge_control`, `test/test_observer_validation`, `test/test_network_policy`, `test/test_network_hostname`, `test/test_ntp_validation`, `test/test_wifi_tx_power_policy`, `test/test_wifi_power_save_policy`, plus any mocks they need from DMC `test/mocks/`

**Interfaces:**
- Produces: `scripts/vendor_dmc_observer.sh <dmc-checkout-dir> <commit>` (copies the list, applies the patches in order, writes the commit into `docs/dmc_observer_vendor.md`).
- Produces (patch 0001): every `WITH_SNMP` guard in the vendored `MQTTBridge.{h,cpp}` becomes `DMC_OBSERVER_SNMP`, so our `WITH_SNMP` companion agent never pulls in DMC's `SNMPAgent`. `SNMPAgent.*` is not vendored.
- Produces (patch 0002): `activeNetworkLink()` in `NetworkLink.cpp` returns `externalNetworkLink()` when `NETWORK_LINK_EXTERNAL_WIFI` is defined; declaration `NetworkLink& externalNetworkLink();` in `NetworkLink.h`.
- Produces (patch 0003): `static void MQTTBridge::setActiveSlotCap(int cap)` (0 = no extra cap; `getMaxActiveSlots()` returns `min(cap, default)` when cap > 0) and `int MQTTBridge::getSkippedPublishes() const`.

The file list is the include closure of `CompanionObserver.cpp`, `bridges/MQTTBridge.cpp`, `ObserverConfig.cpp`, `ObserverCLI.cpp`, `NetworkLink.cpp`, `JWTHelper.cpp`, `MQTTMessageBuilder.cpp`, `MQTTPayloadBuilder.cpp`, `CrashReport.cpp` and the four `ESP32*Fix` / `ESP32Ipv6Preference` / `ESP32TcpBindReuse` sources, minus files already in our tree, minus `SNMPAgent.*`, `NodeDiagCLI.*`, `esp32/WebConfigServer.*`, `ESP32ManifestOTA.*`, `CommonGpsPrefs.*`, `sensors/GpsGuard.h`. If a dropped file turns out to be needed to compile, add it to the list rather than editing vendored code. If `ObserverCLI.cpp` fails only because of DMC's larger `CommonCLI.h`, add a fourth patch that switches its include to `TxtDataHelpers.h`.

- [ ] **Step 1:** Copy the DMC test suites above into `test/`, add `MQTTPayloadBuilder.cpp` to `[env:native]`. Run `pio test -e native`. Expected: the new suites FAIL to compile (headers missing).
- [ ] **Step 2:** Write the script, file list and three patches; run `bash scripts/vendor_dmc_observer.sh <scratch>/dmc 24c6636a`.
- [ ] **Step 3:** Run `pio test -e native`. Expected: all suites PASS, including the existing ones.
- [ ] **Step 4:** Guard check, sequentially: `FIRMWARE_VERSION=v0.0.0.0-probe bash build.sh build-firmware <env>` for `heltec_v4_companion_radio_usb_tcp`, `Heltec_v2_companion_radio_ble`, `RAK_4631_companion_radio_ble`. Expected: all OK. The V4 `usb_tcp` flash size moves by less than 1 KB from the base branch, because the vendored files compile to nothing without `WITH_MQTT_BRIDGE`.
- [ ] **Step 5:** Commit `chore(mqtt): vendor DMC observer helpers at 24c6636a`.

### Task 2: Build plumbing (reduced TLS, AsyncTCP stack, cert bundle)

**Files:**
- Create: `scripts/mbedtls_4k.py`, `scripts/fetch_mbedtls_4k.sh`, `scripts/mbedtls_4k_manifest.txt`, `scripts/asynctcp_stack.py`, `scripts/generate_cert_bundle.py` (all copied from DMC; manifest `release mbedtls-4k`, same hashes)
- Modify: `platformio.ini` `[esp32_base]` `extra_scripts` add `pre:scripts/mbedtls_4k.py` and `pre:scripts/asynctcp_stack.py`; `build.sh` `build_firmware()`: for envs ending in `_mqtt`, run `scripts/fetch_mbedtls_4k.sh --all` and default `MESHCORE_REDUCED_TLS=auto`; `MBEDTLS_4K_BASE_URL` default is this repo's release URL; `.gitignore` adds `.mbedtls-4k/` and `src/certs/`
- Release: GitHub release `mbedtls-4k` on Elektr0Vodka/meshcomod with the four esp32s3 archives (tarball name as the fetch script expects), hashes verified before upload

**Interfaces:**
- Produces: any env with `WITH_MQTT_BRIDGE` and no `BOARD_HAS_PSRAM`, built through `build.sh`, links the reduced archives and prints `reduced-TLS: <env>: active`.

- [ ] **Step 1:** Fetch the tarball from agessaman's `mbedtls-4k` release, check the sha256 values against the manifest, then publish the same file to this repo's `mbedtls-4k` release.
- [ ] **Step 2:** Add the scripts and the `build.sh` change.
- [ ] **Step 3:** Run `FIRMWARE_VERSION=v0.0.0.0-probe bash build.sh build-firmware Heltec_v3_companion_radio_usb_tcp`. Expected: OK, with no `reduced-TLS` line and no `asynctcp:` line (not an MQTT env).
- [ ] **Step 4:** Verify the side finding: in `.pio/build/Heltec_v3_companion_radio_usb_tcp/firmware.map`, `libmbedtls.a` resolves to the framework path. Record the result in the PR text. No change to `usb_tcp` images in this PR.
- [ ] **Step 5:** Commit `build(mqtt): reduced-TLS archives and AsyncTCP stack cap for no-PSRAM MQTT builds`.

### Task 3: Passive network link and Wi-Fi adopt

**Files:**
- Create: `src/helpers/esp32/ExternalWiFiNetworkLink.{h,cpp}`, `examples/companion_radio/CompanionMqttPolicy.h` (header-only, native-testable)
- Test: `test/test_companion_mqtt_policy/test_companion_mqtt_policy.cpp`

**Interfaces:**
- Produces: `class ExternalWiFiNetworkLink : public NetworkLink`. `begin()` and `bootstrap()` do not touch the radio and return `WiFi.status() == WL_CONNECTED`. `configValid()` returns true. `maintain()` reports connect and disconnect transitions from `WiFi.status()`. `isConnected`, `localIP`, `rssi`, `resolveHost` and diagnostics read from `WiFi`. `setWifiTxPower` returns `WifiTxPowerPolicy::kNotRunning`. `NetworkLink& externalNetworkLink()` returns a static instance.
- Produces in `CompanionMqttPolicy.h`, namespace `CompanionMqttPolicy`:
  - `bool shouldAdoptObserverWifi(const char* nvs_ssid, const char* observer_ssid)`: true only when `nvs_ssid` is null or empty and `observer_ssid` is non-empty and shorter than `WIFI_CONFIG_SSID_MAX`.
  - `int slotCap(bool has_psram, bool ble_on_at_boot)`: 0 when `has_psram`, else 1 when `ble_on_at_boot`, else 2.
  - `bool isObserverCommand(const char* cmd)`: true for `get `/`set ` followed by a key starting with `mqtt` or equal to `timezone`, and for `get link.status`; false for every `wifi.*`, `ota`, `snmp*` and everything else.

- [ ] **Step 1:** Write the tests: `shouldAdoptObserverWifi("", "home")` true; `("home", "other")` false; `("", "")` false; `(nullptr, "home")` true; 32-char SSID false. `slotCap(true, true)` 0, `slotCap(false, true)` 1, `slotCap(false, false)` 2. `isObserverCommand("set mqtt1.preset analyzer-eu")` true, `("get mqtt.status")` true, `("set timezone Europe/Amsterdam")` true, `("get link.status")` true, `("set wifi.ssid x")` false, `("get wifi.txpower")` false, `("ota check")` false, `("get snmp")` false.
- [ ] **Step 2:** Run `pio test -e native -f test_companion_mqtt_policy`. Expected: FAIL (header missing).
- [ ] **Step 3:** Implement `CompanionMqttPolicy.h` and `ExternalWiFiNetworkLink`.
- [ ] **Step 4:** Run the native tests. Expected: PASS.
- [ ] **Step 5:** Commit `feat(companion): passive Wi-Fi link and MQTT policy helpers`.

### Task 4: Companion glue and the Heltec V4 `_mqtt` env

**Files:**
- Create: `examples/companion_radio/CompanionMqtt.{h,cpp}` (all `#ifdef WITH_MQTT_BRIDGE`)
- Modify: `examples/companion_radio/MyMesh.{h,cpp}` (`MQTTBridgeHost` base; `logRx`/`logTx` overrides; `logRxRaw` forwards; observer dispatch in `handleCommand`; `cli_command` 161 bytes under `WITH_MQTT_BRIDGE`; restart the bridge after a key import), `examples/companion_radio/main.cpp` (`CrashReport::captureAtBoot()`, mesh in PSRAM, adopt after `wifiConfigBegin()`, slot cap before `the_mesh.begin`'s observer start, BLE heap gate also on `_mqtt` builds), `variants/heltec_v4/platformio.ini` (new env)
- Test: build only (hardware later)

**Interfaces:**
- Consumes: `CompanionObserver` (vendored), `externalNetworkLink()`, `CompanionMqttPolicy::*`, `MQTTBridge::setActiveSlotCap`.
- Produces: `void companionMqttBegin(MyMesh& mesh, FILESYSTEM* fs)`, `void companionMqttLoop()`, `bool companionMqttCommand(uint32_t sender_timestamp, const char* cmd, char* reply)` (false when not an observer command), `void companionMqttAdoptWifi()`, `int companionMqttConnectedSlots()`, `int companionMqttConfiguredSlots()`, `int companionMqttQueueSize()`, `int companionMqttSkippedPublishes()`, `bool companionMqttRunning()`.
- BLE heap gate: `MultiTransportCompanionInterface::enableBle()` becomes `bool enableBle()`. On `WITH_MQTT_BRIDGE` builds, when NimBLE is not started yet, it requires `ESP.getFreeHeap() >= 50 * 1024` and `ESP.getMaxAllocHeap() >= 20 * 1024` (the touch build's thresholds). Otherwise it leaves BLE off, does not persist the toggle, and returns false. Callers (the `MyMesh` CLI, `companion-new` and touch UI) show `BLE needs more free memory, reboot with fewer MQTT slots`. The boot co-init uses the same gate on `_mqtt` builds.
- `MyMesh` implements `MQTTBridgeHost`: `getObserverNodeName()` returns `_prefs.node_name`, `getObserverRadioParams()` returns the prefs radio values, `isObserverForwarding()` returns the client-repeat state, `getObserverRole()` returns `"companion"`.
- Env `heltec_v4_companion_radio_usb_tcp_mqtt`: extends the V4 `usb_tcp` env flags plus `-D WITH_MQTT_BRIDGE=1 -D NETWORK_LINK_EXTERNAL_WIFI=1 -D MQTT_MAX_PACKET_SIZE=1024 -D CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`, `board_ssl_cert_source = adafruit-full`, `board_build.embed_files = src/certs/x509_crt_bundle.bin`, extra scripts `pre:scripts/generate_cert_bundle.py`, src `+<helpers/bridges/MQTTBridge.cpp> +<helpers/MQTTMessageBuilder.cpp> +<helpers/JWTHelper.cpp>`, the DMC lib_deps (PsychicMqttClient 0.2.4, ArduinoJson 7.4.3, NTPClient, Timezone, Time 1.6.1). The `linker --wrap` flags from DMC's `esp32_base` (`esp_transport_ws_init`, `lwip_getaddrinfo`, `dns_gethostbyname`, `tcp_bind`) go in this env only. Our SNTP kick stays; the observer's NTP runs too, and `.3.4.0` keeps our meaning.

- [ ] **Step 1:** Add the env; build it. Expected: FAIL (glue missing).
- [ ] **Step 2:** Implement the glue and the `MyMesh`/`main.cpp` hooks.
- [ ] **Step 3:** Build `heltec_v4_companion_radio_usb_tcp_mqtt`. Expected: OK; record flash and static RAM in the PR. Static internal RAM must be lower than the V4 `usb_tcp` image, because the mesh object is in PSRAM.
- [ ] **Step 4:** Rebuild `heltec_v4_companion_radio_usb_tcp` and `Heltec_v3_companion_radio_usb_tcp`. Expected: OK, unchanged sizes.
- [ ] **Step 5:** Commit `feat(companion): MQTT observer uplink on Heltec V4 (_mqtt env)`.

### Task 5: More boards (V4 R8, Xiao S3 WIO, Heltec V3)

**Files:**
- Modify: `variants/heltec_v4/platformio.ini` (`heltec_v4_r8_companion_radio_usb_tcp_mqtt`), `variants/xiao_s3_wio/platformio.ini` (`Xiao_S3_WIO_companion_radio_usb_tcp_mqtt`), `variants/heltec_v3/platformio.ini` (`Heltec_v3_companion_radio_usb_tcp_mqtt` with the V3 values from Global Constraints)

- [ ] **Step 1:** Add the three envs; build each through `build.sh`. Expected: OK; the V3 log shows `reduced-TLS: Heltec_v3_companion_radio_usb_tcp_mqtt: active` and `asynctcp: CONFIG_ASYNC_TCP_STACK_SIZE=8192`.
- [ ] **Step 2:** Commit `feat(companion): _mqtt envs for V4 R8, Xiao S3 WIO and Heltec V3`.

### Task 6: SNMP, UI line, docs, release list

**Files:**
- Modify: `examples/companion_radio/CompanionSNMP.cpp` (under `WITH_MQTT_BRIDGE`: register `.3.1.0`, `.3.2.0`, `.3.3.0` via `addDynamicIntegerHandler` reading the `companionMqtt*` getters), `src/helpers/ui/companion-new/UITask.cpp` (`MQTT n/m up` line, as DMC), `docs/companion_snmp.md`, `docs/companion_mqtt.md` (new: setup, presets, slots, BLE and slot budget, V3 limits and the contact-cap warning, cross-flash with DMC, one-way Wi-Fi adopt), `companion_boards.ini` (add the four `_mqtt` envs to their boards; status `Build only, experimental`), README table via `python3 scripts/companion_boards.py readme --write`, `changelog_ev.md`
- Test: `python3 scripts/companion_boards.py check`

- [ ] **Step 1:** Implement the SNMP and UI changes; rebuild the V4 `_mqtt` env. Expected: OK.
- [ ] **Step 2:** Write the docs and the board list; run `python3 scripts/companion_boards.py readme --write` and `python3 scripts/companion_boards.py check`. Expected: check passes.
- [ ] **Step 3:** Run `pio test -e native`. Expected: PASS.
- [ ] **Step 4:** Commit `docs(companion): MQTT observer setup, SNMP .3 group, release envs`; push and open the PR (`feat(companion): MQTT observer uplink (_mqtt envs)`), linking the investigation report and listing hardware tests still to do: V4 soak with BLE on and off, a V3 heap reading with BLE plus one WSS slot.
