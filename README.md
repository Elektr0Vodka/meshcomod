<img width="1024" height="700" alt="meshcomod_logo_cropped" src="https://github.com/user-attachments/assets/5c4ff275-b306-4969-bb32-dd28298133c3" />

# meshcomod

A **multi-transport companion** firmware built on top of [MeshCore](https://github.com/meshcore-dev/MeshCore) for **Heltec** and **Seeed** LoRa devices (OLED / e-ink / headless). **One build serves USB + Bluetooth + TCP at the same time**, so you can drive the radio from a phone app, the web client, or Home Assistant — together.

Upstream: **[github.com/meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore)** (MeshCore is a lightweight multi-hop LoRa mesh; see their repo for the protocol, mobile clients, and the canonical flasher).

> **🖐️ Looking for the touch UI?** The standalone on-device LVGL interface for the **Heltec V4 TFT** and **LilyGo T-Deck** now lives in its own project: **[ALLFATHER-BV/wadamesh](https://github.com/ALLFATHER-BV/wadamesh)**. This repo is the OLED/e-ink companion firmware (the MeshCore core it builds on) only.

> **Experimental, use at your own risk.** This firmware is not officially supported. Flashing custom firmware may have unexpected effects; you are responsible for your use of it. No warranty is provided.

---

## About this fork

This is the **Elektr0Vodka fork** of [ALLFATHER-BV/meshcomod](https://github.com/ALLFATHER-BV/meshcomod). It tracks upstream meshcomod (currently synced at **MeshCore v1.18.0**, companion firmware `v1.18.0.1`) and adds a set of companion features on top. Every fork-only change is listed in [`changelog_ev.md`](changelog_ev.md).

> **Fork status: experimental.** Most fork features and the boards this fork added are **built in CI but not yet tested on hardware**. Builds are published as **pre-releases** only. Expect rough edges, keep a known-good merged image of your current firmware for rollback, and report problems in this fork's [issues](https://github.com/Elektr0Vodka/meshcomod/issues), not upstream.

| Fork feature | What it does | Status |
|---|---|---|
| [Runtime CAD toggle](docs/companion_cad_toggle.md) | Turn Channel Activity Detection before TX on or off from the app or the device, no reboot. On by default. | Verified on Heltec V4 and V3 (ui-new). Other UIs build only. |
| [Receive filter](docs/companion_receive_filter.md) | `filter` CLI that drops junk group text (malformed, too old, relayed by a blocked repeater) and hides channel text by sender or content. Off by default. | Builds, not tested on hardware. |
| [SNMP agent](docs/companion_snmp.md) | Read-only SNMPv2c on UDP 161, same OID layout as the Dutch-MeshCore observer. Off by default (`set snmp on`). Not in the touch build. | Builds, not tested on hardware. |
| [Manifest OTA](docs/companion_manifest_ota.md) | `ota check` and `ota update` against the DutchMeshCore OTA server. Never downgrades, refuses a partition layout change. | Builds, not tested on hardware. |
| Extra boards | `usb_tcp` builds for V4 R8, V4 R8 TFT, V4 TFT (no touch), Wireless Stick Lite V3 and Wireless Tracker, plus experimental ones for Wireless Tracker V2, Vision Master E213, Xiao S3, RAK3112, T-ETH Elite, T-Beam S3 Supreme, Vision Master E290 and T190, RC32, T-Beam 1W, Meshnology W12, Station G2 and G3, and ThinkNode M9. BLE and USB builds for the Seeed Wio Tracker L1 / L1 Pro, the LilyGo T-Echo and the T-Echo Card. | Builds, not tested on hardware. |
| CI | One runner per board, cached toolchains, pre-releases for test builds. | In use. |

A build with the receive filter reports it in `ver`, e.g. `v1.18.0.1-<sha>-dmc-filter (Build: ...)`.

---

## What's different in meshcomod

On the Wi-Fi boards (every ESP32-S3 `_usb_tcp` build), a single firmware image supports **USB, Bluetooth, and TCP** companion connections at the same time (e.g. Home Assistant on USB, the MeshCore app on BLE, the web client on TCP). Use one, two, or all three; BLE and TCP can be toggled from the device UI.

- **USB** — always on when the device is powered.
- **Bluetooth** — the pairing PIN is shown on the device's **Bluetooth tab** (state + pairing info). **Long-press** the tab to enable/disable BLE; the footer reads "ON: long press" / "OFF: long press".
- **TCP** — server on port **5000**, multiple clients. The **Network (TCP) tab** shows status, IP, port, and SSID. **Long-press** to enable/disable TCP. Wi-Fi credentials are **not** stored in the repo — set them at build time (`WIFI_SSID` / `WIFI_PWD`) or at runtime (see [Configure Wi-Fi](#configure-wi-fi)). The plain WebSocket port is **8765** (`ws://` only).

<img width="190" height="450" alt="Toggle BLE & TCP on the device" src="https://github.com/user-attachments/assets/671345e3-dd68-48a7-b3fe-6ebdacdc425f" />

Other niceties:

- **Push to all clients** — RX log, new messages, contact adverts, and path updates are sent to **every** connected client, so each app sees live updates.
- **No duplicate RX log on sync** — when one client syncs history, only that client gets the sync frames; the others don't see them re-echoed.

Otherwise this is the same codebase as MeshCore; upstream meshcomod syncs from MeshCore, this fork syncs from upstream meshcomod, and each adds its customizations on top.

### Supported devices

"Hardware tested" means a fork build has run on that board. Boards marked "build only" compile and are released, but nobody has flashed this fork's build on one yet.

Boards marked **experimental** go one step further out: their env is the board's stock companion build moved onto the meshcomod multi-transport stack, and nobody working on the fork owns one. Beyond the general fork caveats, nothing board specific has been checked on them: display and button handling, battery reading and power off, Wi-Fi range, and free heap with Wi-Fi, BLE and SNMP all running. Flash one only if you can recover the board over USB, and please report what works and what does not.

| Device | MCU | Display | Build env | Fork build |
|---|---|---|---|---|
| **Heltec WiFi LoRa 32 V4** | ESP32-S3 | 128×64 OLED | `heltec_v4_companion_radio_usb_tcp` | Hardware tested (v1.17.0.4-DMC-EV) |
| **Heltec WiFi LoRa 32 V3** | ESP32-S3 | 128×64 OLED | `Heltec_v3_companion_radio_usb_tcp` | Hardware tested (v1.17.0.4-DMC-EV) |
| **Heltec Wireless Paper** | ESP32-S3 | 213×104 e-ink | `Heltec_Wireless_Paper_companion_radio_usb_tcp` | Build only |
| **Seeed Xiao S3 WIO** | ESP32-S3 | optional 128×64 OLED | `Xiao_S3_WIO_companion_radio_usb_tcp` | Build only |
| **Heltec WiFi LoRa 32 V4 R8** (16 MB flash, 8 MB PSRAM) | ESP32-S3 | 128×64 OLED | `heltec_v4_r8_companion_radio_usb_tcp` | Build only, new in fork |
| **Heltec WiFi LoRa 32 V4 R8 TFT** | ESP32-S3 | 240×320 TFT (button UI) | `heltec_v4_r8_tft_companion_radio_usb_tcp` | Build only, new in fork |
| **Heltec WiFi LoRa 32 V4 TFT** | ESP32-S3 | 240×320 TFT (button UI, no touch) | `heltec_v4_tft_companion_radio_usb_tcp` | Build only, new in fork |
| **Heltec Wireless Stick Lite V3** | ESP32-S3 | none | `Heltec_WSL3_companion_radio_usb_tcp` | Build only, new in fork |
| **Heltec Wireless Tracker** | ESP32-S3 | 160×80 TFT, GPS | `Heltec_Wireless_Tracker_companion_radio_usb_tcp` | Build only, new in fork |
| **Heltec Wireless Tracker V2** | ESP32-S3 | 160×80 TFT, GPS | `heltec_tracker_v2_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Heltec Vision Master E213** | ESP32-S3 | 250×122 e-ink | `Heltec_E213_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Seeed Xiao S3** (Wio-SX1262 kit) | ESP32-S3 | optional 128×64 OLED | `Xiao_S3_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **RAK3112** (WisBlock) | ESP32-S3 | none | `RAK_3112_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **LilyGo T-ETH Elite** | ESP32-S3 | none | `LilyGo_TETH_Elite_sx1262_companion_radio_usb_tcp` | **Experimental**, build only, new in fork (Wi-Fi, not Ethernet) |
| **LilyGo T-Beam S3 Supreme** | ESP32-S3 | 128×64 OLED, GPS | `T_Beam_S3_Supreme_SX1262_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Heltec Vision Master E290** | ESP32-S3 | 296×128 e-ink | `Heltec_E290_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Heltec Vision Master T190** | ESP32-S3 | 320×170 TFT | `Heltec_T190_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Heltec RC32** | ESP32-S3 | TFT, rotary input, GPS | `heltec_rc32_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **LilyGo T-Beam 1W** | ESP32-S3 | 128×64 OLED, GPS | `LilyGo_TBeam_1W_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Meshnology W12** | ESP32-S3 | 128×64 OLED, GPS | `meshnology_w12_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Station G2** | ESP32-S3 | 128×64 OLED | `Station_G2_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Station G3** (ESP32-S3) | ESP32-S3 | 128×64 OLED | `Station_G3_ESP32_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Elecrow ThinkNode M9** | ESP32-S3 | 320×240 TFT, GPS | `ThinkNode_M9_companion_radio_usb_tcp` | **Experimental**, build only, new in fork |
| **Seeed Wio Tracker L1 / L1 Pro** | nRF52840 | 128×64 OLED, GPS | `WioTrackerL1_companion_radio_ble`, `WioTrackerL1_companion_radio_usb` | Build only, new in fork |
| **LilyGo T-Echo** | nRF52840 | 200×200 e-ink, GPS | `LilyGo_T-Echo_companion_radio_ble`, `LilyGo_T-Echo_companion_radio_usb` | Build only, new in fork |
| **LilyGo T-Echo Card** | nRF52840 | 72×40 OLED, GPS | `LilyGo_T-Echo_Card_companion_radio_ble`, `LilyGo_T-Echo_Card_companion_radio_usb` | Build only, new in fork |

The SNMP, receive filter and manifest OTA features are not hardware tested on any board yet, including V4 and V3.

The **Wio Tracker L1**, **T-Echo** and **T-Echo Card** have no Wi-Fi (nRF52840), so they get the stock single-transport companions: one image for BLE, one for USB, not both at once. TCP, WebSocket, SNMP and manifest OTA are not available on them. The CAD toggle and receive filter are included. Flash the `.uf2` by double-tapping reset and copying it to the USB drive that appears; the `.zip` is the DFU package for OTA over BLE.

**Env-name casing matters when building:** V4 uses lowercase `heltec_v4_…`; V3 uses a capital H `Heltec_v3_…`. Use the exact name (see [Build it yourself](#hard--build-it-yourself)).

> **Touch UI builds** (Heltec V4 TFT with the touchscreen, LilyGo T-Deck) are built and released from **[wadamesh](https://github.com/ALLFATHER-BV/wadamesh)**, not here.

<p align="left">
  <img width="200" height="200" alt="Heltec WiFi LoRa 32 V4" src="https://github.com/user-attachments/assets/1ecd81c5-644b-4df3-99a8-e544d1864a01" />
  <img width="200" height="200" alt="Heltec WiFi LoRa 32 V3" src="https://github.com/user-attachments/assets/21289b67-2e1f-496e-8d9f-65c36ee74461" />
</p>

---

## Home Assistant tab

Run the **full meshcomod web client as a tab inside Home Assistant**. Connect the device over **TCP (Wi-Fi)** or **USB** to the machine running HA, then add the [meshcomod client](https://meshcomod.com) as an iframe/panel for a dedicated **MESHCOMOD** sidebar tab: voltage, state of charge, temperature, uptime, and realtime/trend graphs alongside your other dashboards.

<p align="left">
  <img width="700" alt="Meshcomod client as a Home Assistant tab" src="docs/meshcomod-ha-tab.png" />
</p>

> **Use TCP for stability in Home Assistant.** The web console and the companion protocol share the USB serial port, so a USB-connected HA can be affected by console handling. **Connect HA over TCP (Wi-Fi)** for a separate, unaffected path.

---

## Configure Wi-Fi

Three ways, no CLI recovery mode needed. **Wi-Fi is 2.4 GHz only** (not 5 GHz-only SSIDs); credentials are stored in NVS and survive reboots.

**1. Flasher Wi-Fi GUI (easiest)** — the Wi-Fi panel on [flasher.meshcomod.com](https://flasher.meshcomod.com).

**2. Web/USB console** — [console.meshcomod.com](https://console.meshcomod.com) (or the Console tab on the flasher), device on USB:

```
set wifi.ssid YourSSID
set wifi.pwd  YourPassword     # open network: set wifi.pwd
wifi.apply
wifi.status
```

<img width="424" height="99" alt="console wifi setup" src="https://github.com/user-attachments/assets/ea127bb0-9f97-4f09-8538-31451bb9b939" />

**3. Meshcomod chat** — open the built-in **Meshcomod** contact (favourited by default, near the bottom of the contact list) and send commands. **These are handled locally on the device and are *not* sent over the LoRa mesh.** Note your client may keep the password in local chat history.

| Command | What it does |
|---|---|
| `help` | List all Meshcomod commands |
| `status` | USB/BLE/TCP and Wi-Fi state |
| `wifi set ssid "<v>"` | Set SSID (quote if it has spaces) |
| `wifi set pwd "<v>"` | Set password (`""` for open) |
| `wifi scan` / `wifi use <n>` | List nearby SSIDs, then pick one |
| `wifi status` | SSID, runtime vs compile-time creds, link state |
| `wifi apply` | Reconnect with stored credentials |
| `wifi clear` | Clear runtime creds (fall back to compile-time on next boot) |
| `tcp on` / `tcp off` / `tcp status` | Control/query the TCP server |
| `ble on` / `ble off` / `ble status` | Control/query Bluetooth |
| `mac` | Show the device Wi-Fi and BLE MAC addresses (also `get mac`, `get wifi.mac`, `get ble.mac` in the app console or over USB serial) |
| `ota check` | Ask the OTA server whether a newer build exists for this device |
| `ota update` | Install that build over Wi-Fi (see [Update over Wi-Fi](#update-over-wi-fi)) |

> Disabling wireless access is guarded: `tcp off` and `ble off` ask you to confirm with `ok` (or `cancel`) first, since you could lock yourself out.

---

## Easy — flash a prebuilt

<p align="left">
  <img width="395" height="84" alt="Easy" src="https://github.com/user-attachments/assets/cd496321-1aad-425f-b8cf-ccba2cc9478d" />
</p>

No local build needed. This fork's builds are published as **GitHub pre-releases** (tag `companion-*`) on [Elektr0Vodka/meshcomod releases](https://github.com/Elektr0Vodka/meshcomod/releases?q=companion).

> **The flasher's Easy mode installs upstream meshcomod, not this fork.** To get the fork features, use **Custom firmware** with a `.bin` from this fork's releases.

1. Open **[flasher.meshcomod.com](https://flasher.meshcomod.com)**.
2. Choose **Custom firmware** and upload the `-merged.bin` for your board from the [latest fork pre-release](https://github.com/Elektr0Vodka/meshcomod/releases?q=companion).
3. Connect via USB, BLE (PIN on the display), and/or TCP.

> **nRF52 boards** (Wio Tracker L1, T-Echo, T-Echo Card) do not use the web flasher. Double-tap reset, then copy the `.uf2` for your board onto the USB drive that appears. The `.zip` in the same release is the DFU package for updating over BLE. Pick the `_ble` or `_usb` image for the transport you want.

> If unsure which binary to flash, use **merged** (it includes the bootloader + partition table and writes from `0x0`).

Rolling back: flash an upstream build with the flasher's Easy mode, or an older fork pre-release. Upstream pinned versions are in the [Release log (RELEASES.md)](RELEASES.md) and the [upstream GitHub Releases](https://github.com/ALLFATHER-BV/meshcomod/releases).

### Update over Wi-Fi

Once a device runs a build with manifest OTA and is on Wi-Fi, it can update itself. In the **Meshcomod** contact:

- `ota check` asks the [DutchMeshCore OTA server](https://ota.dutchmeshcore.nl/stats) for the newest published build for your board and reports `up to date` or `update available: <running> -> <published>`.
- `ota update` installs it and reboots. Start it from a **TCP or WebSocket** connection (not USB or BLE), the same as `ota url`.

It never downgrades, and it refuses a build made for a different partition layout (flash the merged image over USB once in that case). Details: [Companion manifest OTA](docs/companion_manifest_ota.md).

---

## Hard — build it yourself

<p align="left">
  <img width="395" height="84" alt="Hard" src="https://github.com/user-attachments/assets/92a0ea2d-e1f1-4e8b-82f0-705a1f826fd1" />
</p>

For custom code or build flags.

**Dependencies:** `git`, `python3` + `pip`, and `platformio` (`pio` in PATH, or `python3 -m platformio`). `build.sh` falls back to `python3 -m platformio` when `pio` isn't on PATH.

- macOS: `xcode-select --install`, then `python3 -m pip install --user -U platformio`
- Linux: `sudo apt update && sudo apt install -y git python3 python3-pip`, then the pip install
- Windows: use **WSL** (Ubuntu) and follow the Linux steps

Clone the repo and build with the exact env name. Wi-Fi credentials are baked in at build time (override at runtime later).

#### Heltec V4 (OLED)

```bash
export WIFI_SSID="YourNetworkName"
export WIFI_PWD="YourPassword"          # open network: export WIFI_PWD=""
export FIRMWARE_VERSION=v1.18.0.1
sh build.sh build-firmware heltec_v4_companion_radio_usb_tcp
```

#### Heltec V3 (note the capital H)

```bash
export WIFI_SSID="YourNetworkName"
export WIFI_PWD="YourPassword"
export FIRMWARE_VERSION=v1.18.0.1
sh build.sh build-firmware Heltec_v3_companion_radio_usb_tcp
```

Other targets use the same flow with their env name from the [Supported devices](#supported-devices) table. Build all shipped companions at once with `sh build.sh build-meshcomod-companion-firmwares`.

**Outputs** (ESP32 targets): app-only `out/<env>-<version>-<sha>.bin` and **merged** `out/<env>-<version>-<sha>-merged.bin` (flash from `0x0`). The merged image is also at `.pio/build/<env>/firmware-merged.bin`. The `*_companion_radio_usb_tcp` targets also get `out/<env>.partsig` (partition layout, used by `ota update`) and have the OTA manifest base baked in (`OTA_MANIFEST_BASE_URL` overrides it). nRF52 targets produce `out/<env>-<version>-<sha>.uf2` and a DFU `.zip` instead. Do not set `DISABLE_DEBUG=1` for nRF52 targets: it undefines `CFG_DEBUG`, which the nRF52 Arduino core needs to compile.

### Black screen after flashing

If the display stays black after flashing and resetting:

1. **Flash the merged image** — it includes the bootloader + partition table. Generate with `pio run -t mergebin -e <env>` → `.pio/build/<env>/firmware-merged.bin`. (Default `out/` copies are app-only.)
2. **Use the web flasher** — on [flasher.meshcomod.com](https://flasher.meshcomod.com), **Custom firmware** → upload the merged file (writes from `0x0`).
3. **Still black?** — upload the merged file again; if the flasher offers **erase**, erase first then upload (this wipes contacts and other data).
4. **Heltec V4 only:** if a merged image still gives a black screen (while V3 is fine), try the **non-merged** `.bin` at offset `0x10000` (the device must already have a valid bootloader + partition table). Newer firmware also adds a longer pre-display delay and watchdog feeds on first boot to improve merged-boot reliability.

---

## Releases & CI

Companion / repeater / room-server firmware is built by GitHub Actions and published as **GitHub Releases**. Push a track tag (e.g. `companion-v1.18.0.1`) and the pipeline builds each shipped board on its own runner, publishes the release with the bins attached once every board has built, and prunes old releases.

In this fork, companion test builds go out as **pre-releases**: a manual run with the `prerelease` input, or a tag with a suffix (e.g. `companion-v1.18.0.2-rc1`). Pre-releases are never marked latest and are not served over the air. The companion track keeps the newest 5 stable releases and 3 pre-releases. See [`docs/CI_RELEASES.md`](docs/CI_RELEASES.md).

---

## Docs

- [Companion protocol](docs/companion_protocol.md) · [CLI commands](docs/cli_commands.md) · [QR codes](docs/qr_codes.md)
- [Packet format](docs/packet_format.md) · [Payloads](docs/payloads.md) · [Stats binary frames](docs/stats_binary_frames.md)
- [Device WebSocket / Wi-Fi](docs/DEVICE_WEBSOCKET_WIFI.md) · [Companion manifest OTA](docs/companion_manifest_ota.md) · [FAQ](docs/faq.md)
- [CI releases](docs/CI_RELEASES.md) · [Release process](docs/RELEASE_PROCEDURE.md) · [Release log](RELEASES.md)
- Fork: [Changelog](changelog_ev.md) · [CAD toggle](docs/companion_cad_toggle.md) · [Receive filter](docs/companion_receive_filter.md) · [SNMP agent](docs/companion_snmp.md)

---

## Syncing from upstream

This fork syncs from **upstream meshcomod**, which in turn syncs from MeshCore. Do the merge on a `Chore/` branch and land it by PR.

```bash
git remote add upstream https://github.com/ALLFATHER-BV/meshcomod.git   # if not already added
git fetch upstream
git switch -c Chore/upstream-sync-<version> main
git merge upstream/main
# resolve any conflicts, build, then push the branch and open a PR
```

Record the sync under **Upstream syncs** in [`changelog_ev.md`](changelog_ev.md).

Contributing to **upstream MeshCore**? Use their `dev` branch for PRs; open an Issue first for larger changes — see [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore).

### Known quirks

- **Legacy/no-channel clients** — a client that doesn't implement channel chat should ignore channel message frames (`0x08`, `0x11`).
- **Debug over USB** — serial terminals show binary companion frames on the USB transport; use BLE/TCP for app traffic if you need clean USB logs.
- **BLE first connect** — on the first BLE connection you may need to disconnect and reconnect once.
- **First Meshcomod `help`** — the first `help` in a fresh Meshcomod chat may not reply; send it again.
- **Web console retry** — a `get/set wifi.*` command may not show a reply on the first try; run it again.

---

## License

Same as MeshCore (see [license.txt](license.txt)). MeshCore is MIT.
