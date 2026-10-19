# Changelog: Elektr0Vodka fork

What this fork ([Elektr0Vodka/meshcomod](https://github.com/Elektr0Vodka/meshcomod)) changes on top of upstream [ALLFATHER-BV/meshcomod](https://github.com/ALLFATHER-BV/meshcomod). Upstream changes pulled in by a sync are not repeated here; see [`RELEASES.md`](RELEASES.md) for those.

Newest first. Each entry names the PR that landed it.

## How to keep this up to date

- Add an entry under **Unreleased** in the same PR as the change.
- Group by `Added`, `Changed`, `Fixed`, `Removed`, `CI and build`, `Docs`.
- Note hardware test status for firmware changes (built only, or verified on which board).
- When syncing upstream, record the upstream version under **Upstream syncs** and nothing else.
- To list fork-only commits: `git log --oneline --no-merges upstream/main..main`

## Unreleased

### Added

- **Thirteen more nRF52 companion boards** (#23): the existing upstream BLE and USB companion envs for the LilyGo T-Echo Lite (with and without shell), RAK4631, RAK3401, Seeed SenseCAP T1000-E, Heltec T114, T1, T096 and MeshPocket, Elecrow ThinkNode M1 and M8, B&Q Nano G2 Ultra, the nRF52840 Pro Micro DIY board and Keepteen LT1 join the release list (28 envs, `.uf2` + DFU `.zip` each). Single-transport like the other nRF52 boards. All build with at least 31% flash free. Builds, not tested on hardware.
- **Eight more multi-transport companion boards (experimental)** (#18): `usb_tcp` envs (USB + TCP + WebSocket + BLE, SNMP, manifest OTA) for the Heltec Vision Master E290 and T190, Heltec RC32, LilyGo T-Beam 1W, Meshnology W12, Station G2, Station G3 (ESP32-S3) and Elecrow ThinkNode M9, all added to the release list. All are 16 MB ESP32-S3 boards and reuse the display driver of their stock companion builds. The T-Beam 1W and both Stations default to the 4 MB partition layout, so their envs select `default_16MB.csv`. The ThinkNode M9 `variant.h` macro `DONE` (unused) is renamed `PIN_CHARGE_DONE` because it broke the SNMP library's parser enum. Builds, not tested on hardware.
- **Six more multi-transport companion boards (experimental)** (#15): `usb_tcp` envs (USB + TCP + WebSocket + BLE, SNMP, manifest OTA) for the Heltec Wireless Tracker V2, Heltec Vision Master E213, Seeed Xiao S3, RAK3112, LilyGo T-ETH Elite and LilyGo T-Beam S3 Supreme, all added to the release list. Each is an ESP32-S3 with at least 8 MB flash on its existing partition table and reuses the display driver of the board's stock companion builds (RAK3112 and T-ETH Elite are headless). The T-ETH Elite build runs over Wi-Fi; its Ethernet port is not used, as upstream has no Ethernet companion for it either. Marked experimental in the README: nobody working on the fork owns these boards, so beyond building, nothing board specific (display, buttons, battery, free heap) has been checked. Builds, not tested on hardware.
- **Seeed Wio Tracker L1 / L1 Pro companions** (#14): the existing upstream `WioTrackerL1_companion_radio_ble` and `WioTrackerL1_companion_radio_usb` envs are now in the release list, so the fork builds and releases them (`.uf2` + DFU `.zip`). The nRF52840 has no Wi-Fi, so these are single-transport (BLE or USB) with no TCP, SNMP or manifest OTA. The CAD toggle and receive filter build in. Builds, not tested on hardware.
- **LilyGo T-Echo companions** (#16): the existing upstream `LilyGo_T-Echo_companion_radio_ble` and `LilyGo_T-Echo_companion_radio_usb` envs are now in the release list (`.uf2` + DFU `.zip`). Like the Wio Tracker L1 this is an nRF52840 without Wi-Fi, so the builds are single-transport (BLE or USB) with no TCP, SNMP or manifest OTA. The e-ink ui-new UI, CAD toggle and receive filter build in. The T-Echo Lite is not included. Builds, not tested on hardware.
- **LilyGo T-Echo Card companions** (#16): `LilyGo_T-Echo_Card_companion_radio_ble` and `_usb` join the release list, single-transport like the T-Echo, with the ui-tiny UI and its CAD page. Builds, not tested on hardware.

### Changed

- **Chip features behind the board** (#28): the device MAC, the `bootloader` reboot into download mode, the SNMP memory figures and the `ota status` progress now come from new `MainBoard` methods (`getDeviceMac`, `canRebootToBootloader` / `rebootToBootloader`, `getMemStats`, `getHttpOtaProgress`) with "not available" defaults. The ESP32 code, including the S2/S3/C3-only force-download register, moves into `ESP32Board.cpp`, so the companion `MyMesh.cpp` and SNMP agent no longer carry chip `#if` guards for these. New boards and chips get a working build by default; a chip that can do more overrides the method. Replies are unchanged, except two: `bootloader` on non-ESP32 boards now says `not supported on this chip` like the classic ESP32 does, and `mac` on nRF52 boards now shows the BLE address (from FICR) instead of `n/a`. Builds (S3, classic ESP32, C6, nRF52), not tested on hardware.

### CI and build

- **One board list: `companion_boards.ini`** (#27): the released companion boards now live in one fork-owned file, one section per board (name, MCU, display, envs, status, PR-check envs). `scripts/companion_boards.py` derives the `build.sh` release list, the PR build matrix and the README device table from it, and a new Companion Boards Check workflow fails when the README table is stale, an env is listed twice or has no `[env:...]` section. Adding a board is now one ini section plus `python3 scripts/companion_boards.py readme --write`. The env list and PR matrix are unchanged by the move (same 59 release envs in the same order, same 5 PR envs).
- **Companion releases build without debug logging** (#26): the companion release workflow now sets `DISABLE_DEBUG=1`, as the repeater release already did. Before, released envs that enable `BLE_DEBUG_LOGGING` or `MESH_DEBUG` in their ini (several nRF52 BLE boards, e.g. the Wio Tracker L1) shipped with it on. The Wio Tracker L1 BLE build drops about 2.4 KB of flash.
- **PR build check builds through `build.sh`** (#26): it now runs `build.sh build-firmware` with `DISABLE_DEBUG=1`, like the release, instead of plain `pio run`, so the flags `build.sh` adds (the manifest OTA code via `OTA_VARIANT`, the debug-flag removal) compile on every PR. Changes to `build.sh` and `scripts/` now trigger the check.
- **PR build check covers classic ESP32** (#21): `Heltec_v2_companion_radio_ble` joins the PR matrix so companion code that only builds on the S3, like the `bootloader` register write fixed in #17, is caught before merge.
- **PR build check covers nRF52** (#14): `WioTrackerL1_companion_radio_ble` joins the PR matrix so fork companion code that only builds on ESP32 is caught.

### Docs

- **README lists the ThinkNode M7** (#26): its BLE and Ethernet companions were in the release list but missing from the device table. The nRF52 board count in the README and `CI_RELEASES.md` is now 17 (T-Echo Lite with and without shell are separate builds).
- **Docs match the release flow**: `AGENTS.md` gains a "Releases in this fork" section (CI tags, PR-only changes to `main`, changelog per PR) and marks the `prebuilt/` flow as upstream legacy. `RELEASE_PROCEDURE.md`, `REPEATER_RELEASE_PROCEDURE.md` and `prebuilt/README.md` say the same at the top. The docs site home links the fork docs, and `CI_RELEASES.md` names the nRF52 `.uf2` / `.zip` assets.
- **Docs follow the new boards** (#19): README notes for flashing and building nRF52 boards, CAD toggle board examples, PR tags on changelog entries.
- **Fork changelog** (#11): this file, `changelog_ev.md`.
- **README describes the fork** (#11): an "About this fork" section with per-feature test status, a hardware test column on the device table, flashing from this fork's pre-releases, and sync steps from upstream meshcomod.
- **Dark theme for the docs site** (#11): a light/dark toggle in the header. Light is the original look; the choice is remembered per browser.

### Fixed

- **Short app frames no longer act on stale bytes** (#25): `handleCmdFrame` now checks a minimum length for the commands that read fixed payload bytes without one (channel text, remove/share/get contact by key, radio, TX power, tuning, other params, auto-add, reboot, factory reset) and answers `ERR_CODE_ILLEGAL_ARG`. A channel text frame shorter than 7 bytes used to pass a negative length to `memcpy` in `sendGroupMessage` (inherited from MeshCore 1.18.0); `sendGroupMessage` now also refuses a negative length. Builds, not tested on hardware.
- **WebSocket frames of 126 bytes or more** (#25): the WebSocket server read the 16- and 64-bit extended payload length little-endian. RFC 6455 sends it big-endian, so any client frame of 126 bytes or more broke the stream. Builds, not tested on hardware.
- **`get cad` / `set cad` in the companion CLI** (#25): both answered `Error: unsupported` although CAD has been a runtime setting since the CAD toggle. They now read and set it live, with the same syntax and replies as the repeater and room server. The tuning-params frame now applies CAD through the same `setCADEnabled` path. Builds, not tested on hardware.
- **ESP32-C6 companions build** (#22): the C6 envs run Arduino core 3.x but inherited NimBLE-Arduino 1.4.3, which only builds against core 2.x. `esp32c6_base` now uses NimBLE-Arduino 2.3.9, and `SerialBLEInterface` implements both callback APIs (picked at compile time), so the S3 boards stay on 1.4.3. All C6 companion envs (Xiao C6, Meshimi, WHY2025 badge, T-LoRa C6, M5Stack Unit C6L) build. Builds, not tested on hardware.
- **`DISABLE_DEBUG=1` builds on nRF52** (#20): `build.sh` undefined `CFG_DEBUG`, which the nRF52 Arduino core reads unconditionally, so every nRF52 build with `DISABLE_DEBUG=1` failed. It now sets `CFG_DEBUG=0`.
- **T-Echo Card companions build again** (#16): the ui-tiny UI still took the upstream 1.18 `MultiSerialInterface*`, while meshcomod passes the board's own serial interface. It now takes `BaseSerialInterface*` like ui-new.
- **Companion builds on classic ESP32** (#17): the `bootloader` CLI command wrote the RTC force-download bit, which only the S2, S3 and C3 have, so every classic ESP32 companion (Heltec V2, T-Beam, T-LoRa and others) failed to compile. Chips without the bit now reply `bootloader: not supported on this chip`. The ESP32-C6 envs still do not build: they run Arduino core 3.x while `esp32_base` pins NimBLE-Arduino 1.4.3, which needs core 2.x.
- **Companion builds on nRF52** (#14): two pieces of upstream meshcomod companion code assumed ESP32 and broke any non-ESP32 companion build. The backup import used the ESP32 PSRAM allocator (`heap_caps_*`); it now uses the default allocator elsewhere. `ota status` read HTTP OTA progress globals that only `ESP32Board.cpp` defines; it now reports `ota: idle` on boards without HTTP OTA.
- **Docs site deploy** (#12): the Pages workflow no longer writes upstream's `docs.meshcore.nz` CNAME into `gh-pages`, and `mkdocs.yml` points at this fork (`elektr0vodka.github.io/meshcomod`) instead of `meshcore-dev`. GitHub Pages must serve the `gh-pages` branch for the mkdocs site to show; it was serving `main` `/docs` through Jekyll.

## 2026-10-16

### Added

- **Heltec Wireless Tracker companion** (#10): new `Heltec_Wireless_Tracker_companion_radio_usb_tcp` env (USB + TCP + WebSocket + BLE, SNMP, HTTP OTA) on the ui-new ST7735 screen. Added to the release list. Builds, not tested on hardware.
- **More multi-transport companion boards** (#7, released in #9): `usb_tcp` envs for Heltec V4 R8, V4 R8 TFT, V4 TFT (non-touch, ui-new) and WSL3 (headless). They use the board's default partition table, so `partSig` refuses an OTA between the touch and non-touch V4 TFT builds. Built, not tested on hardware.
- **Manifest OTA: `ota check` and `ota update`** (#6): companions read `<OTA_MANIFEST_BASE>/<OTA_VARIANT>.json` from the DutchMeshCore OTA server and decide whether a newer build exists. Numeric version compare, no downgrades, partition layout changes refused (needs a USB flash). Manifest fetched over plain HTTP so the check costs no TLS heap. Logic lives in `src/helpers/OtaManifest.h` with native unit tests. See [`docs/companion_manifest_ota.md`](docs/companion_manifest_ota.md).
- **SNMPv2c agent** (#4, #7): ported from the Dutch-MeshCore observer. Compile-flag gated (`WITH_SNMP`) and off at runtime until `set snmp on`. Enabled on V3, V4 OLED, Wireless Paper, Xiao S3 WIO and all new `usb_tcp` boards. Settings in NVS, so the NodePrefs layout is unchanged. Costs about 33 KB flash. See [`docs/companion_snmp.md`](docs/companion_snmp.md).
- **Receive filter** (#3): `filter` CLI that drops malformed group text, group text older than N minutes and floods relayed by a blocked repeater prefix, and hides text by sender or content rules (block, throttle, dose). Junk is dropped before repeating; sender and text rules only hide, so the mesh is never censored. Direct and addressed traffic is never filtered. Off by default, settings in `/cfilter.json`. Ported from the Dutch-MeshCore companion filter. See [`docs/companion_receive_filter.md`](docs/companion_receive_filter.md).

### Changed

- **SNMP OIDs aligned with the observer tier 1 layout** (#8): shared OIDs keep the observer's meaning, type and unit; values the companion lacks read `noSuchObject`. Added MCU temperature, reset reason, TX queue, flood/direct dups, RX air time (moved from `.6.5` to `.2.15`), error flags, NTP synced and lowest free heap. Companion-only values moved to `.9` (clients, contacts, channels) and `.10` (receive filter totals). Battery and temperature sampled every 30 s off the hot path.
- **`ver` reports `-dmc-filter`** (#3) on builds with the receive filter. `FIRMWARE_VERSION` itself stays plain because OTA and the flasher parse it.
- **`build.sh` bakes OTA settings** (#6): for `*companion_radio_usb_tcp*` envs it passes `OTA_VARIANT` and `OTA_MANIFEST_BASE` (default `https://ota.dutchmeshcore.nl/meshcomod/v`, override with `OTA_MANIFEST_BASE_URL`) and writes `out/<env>.partsig` via `scripts/partition_signature.py`.

### Fixed

- **`build.sh` leaked per-env flags** (#6): `PLATFORMIO_BUILD_FLAGS` is now restored after each env, so one env's `-D` flags no longer carry into the next in a multi-env build.

### Removed

- `CNAME` for GitHub Pages (#6).

### CI and build

- **Matrix companion build** (#9): one runner per board, cached PlatformIO toolchains and libdeps. Board list comes from `build.sh get-meshcomod-companion-firmwares-to-build`. The release only runs when every board built.
- **Pre-releases** (#5): `prerelease` input on manual runs, and tags with a suffix (e.g. `companion-v1.18.0.2-rc1`), publish a GitHub pre-release. Never marked latest and not served over the air. `prune-releases.sh` keeps 5 stable and 3 pre-releases separately.
- **Manual builds use the firmware line** (#3): `workflow_dispatch` reads `FIRMWARE_VERSION` from `MyMesh.h` (or an optional `version` input) instead of the branch name.

## 2026-09-10 (v1.17.0.4-DMC-EV)

Prebuilt drop: [`prebuilt/releases/companion/v1.17.0.4-DMC-EV/`](prebuilt/releases/companion/v1.17.0.4-DMC-EV/) for V4 OLED, V3, V4 TFT touch, Wireless Paper and Xiao S3 WIO. Only V4 and V3 verified on hardware.

### Added

- **Runtime CAD toggle** (#1): CAD before TX is now a persisted `cad_enabled` pref (default on) applied live without a reboot. Exposed through an optional trailing byte on `CMD_SET_TUNING_PARAMS` and `CMD_GET_TUNING_PARAMS`, backward compatible with older apps. See [`docs/companion_cad_toggle.md`](docs/companion_cad_toggle.md) and [`docs/companion_protocol.md`](docs/companion_protocol.md).
- **On-device CAD controls** (#1): dedicated CAD page in ui-new (V4, V3) and ui-tiny, a switch in the ui-touch Experimental settings, and a read-only status on the ui-orig home screen.

### Changed

- CAD enabled by default for companions.

### Fixed

- **V4 TFT touch build on Windows** (#1): partition CSV comments rewritten in ASCII, since PlatformIO reads it as cp1252 and failed on box-drawing characters.

### Docs

- Contribution conventions added to [`AGENTS.md`](AGENTS.md).

## Upstream syncs

| Date | Upstream | PR |
|------|----------|----|
| 2026-10-15 | ALLFATHER-BV/meshcomod at MeshCore v1.18.0 (companion `v1.18.0.1`) | #2 |
