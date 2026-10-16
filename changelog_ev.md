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

- **Seeed Wio Tracker L1 / L1 Pro companions**: the existing upstream `WioTrackerL1_companion_radio_ble` and `WioTrackerL1_companion_radio_usb` envs are now in the release list, so the fork builds and releases them (`.uf2` + DFU `.zip`). The nRF52840 has no Wi-Fi, so these are single-transport (BLE or USB) with no TCP, SNMP or manifest OTA. The CAD toggle and receive filter build in. Builds, not tested on hardware.

### CI and build

- **PR build check covers nRF52**: `WioTrackerL1_companion_radio_ble` joins the PR matrix so fork companion code that only builds on ESP32 is caught.

### Docs

- **Fork changelog** (#11): this file, `changelog_ev.md`.
- **README describes the fork** (#11): an "About this fork" section with per-feature test status, a hardware test column on the device table, flashing from this fork's pre-releases, and sync steps from upstream meshcomod.
- **Dark theme for the docs site** (#11): a light/dark toggle in the header. Light is the original look; the choice is remembered per browser.

### Fixed

- **Companion builds on nRF52**: two pieces of upstream meshcomod companion code assumed ESP32 and broke any non-ESP32 companion build. The backup import used the ESP32 PSRAM allocator (`heap_caps_*`); it now uses the default allocator elsewhere. `ota status` read HTTP OTA progress globals that only `ESP32Board.cpp` defines; it now reports `ota: idle` on boards without HTTP OTA.
- **Docs site deploy**: the Pages workflow no longer writes upstream's `docs.meshcore.nz` CNAME into `gh-pages`, and `mkdocs.yml` points at this fork (`elektr0vodka.github.io/meshcomod`) instead of `meshcore-dev`. GitHub Pages must serve the `gh-pages` branch for the mkdocs site to show; it was serving `main` `/docs` through Jekyll.

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
