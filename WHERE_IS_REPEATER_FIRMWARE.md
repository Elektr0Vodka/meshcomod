# Repeater TCP firmware `.bin` location

After you build **`heltec_v4_repeater_tcp`** (or **`Heltec_v3_repeater_tcp`**), the **app-only** image is copied to **two** places:

| Board | Filename |
|-------|----------|
| **Heltec V4** | `heltec_v4_repeater_tcp.bin` |
| **Heltec V3** | `Heltec_v3_repeater_tcp.bin` |

**Paths (HeltecV4 workspace: repo inside `MeshCore/`):**

1. **`MeshCore/out/`** — next to the PlatformIO project  
2. **`<workspace>/out/`** — one level up (e.g. `HeltecV4/out/`) at the **root of the folder you opened in Cursor**

Each successful link writes **two** files (same bytes):

- **`heltec_v4_repeater_tcp.bin`** — always the latest build (overwritten)  
- **`heltec_v4_repeater_tcp-<YYYYMMDD_HHMMSS>-<gitsha>.bin`** — unique name so you can see **which build** is new (sort by name or mtime)

Same bytes as `.pio/build/<env>/firmware.bin`. Flash the app bin at **0x10000** when bootloader + partitions already match.

**Build:**

```bash
cd MeshCore
python3 -m platformio run -e heltec_v4_repeater_tcp
```

`out/` entries may still be hidden if your editor treats gitignored folders as invisible—use **`HeltecV4/out/`** (workspace root) or **Go to Folder** / Terminal.

## Promoting to `prebuilt/` (same layout as companion)

To match **companion** release folders for your **flasher** or app bundles:

1. Set **`REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp`** (same **`v1.14.1.x`** base as companion) and run **`sh build.sh build-repeater-firmwares`** so `out/` contains names like **`heltec_v4_repeater_tcp-meshcomod-v1.14.1.0-repeater-tcp-<gitsha>.bin`** (the **`meshcomod-`** prefix is added by **`build.sh`**).
2. Run **`sh scripts/copy-repeater-release-bins.sh v1.14.1.0`** from **`MeshCore/`** so TCP repeater bins live in **`prebuilt/releases/v1.14.1.0/`** next to companion images.

That copies into:

- **`prebuilt/heltec_v4_repeater_tcp.bin`** and **`prebuilt/Heltec_v3_repeater_tcp.bin`** (latest promoted)
- **`prebuilt/releases/v1.14.1.0/`** (same folder as that companion release)

**Legacy:** **`repeater-1.0.x`** + **`copy-repeater-release-bins.sh repeater-1.0.x`** still targets **`prebuilt/releases/repeater-1.0.x/`**.

Full procedure: [`docs/REPEATER_RELEASE_PROCEDURE.md`](docs/REPEATER_RELEASE_PROCEDURE.md).

Details: [`prebuilt/README.md`](prebuilt/README.md).

See also [`docs/REPEATER_TCP_COMPANION.md`](docs/REPEATER_TCP_COMPANION.md).
