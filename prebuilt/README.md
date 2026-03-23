# Prebuilt firmware (flasher / releases)

Binaries are **not** always committed; this folder defines **where promoted builds live** so tools and humans match the same layout.

## Layout

| Path | Meaning |
|------|--------|
| **`prebuilt/<short-name>.bin`** | Latest **promoted** build for that product (overwritten when you run the copy script for a new release). |
| **`prebuilt/releases/<version>/`** | **Immutable** copy for that version. Add a `notes.md` here when you cut a release. |

**Companion** versions look like **`v1.14.1.0`** (meshcomod radio). **Repeater TCP** uses the **same base** plus **`‑repeater-tcp`** (e.g. build with **`REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp`**, then **`copy-repeater-release-bins.sh v1.14.1.0`** → same **`prebuilt/releases/v1.14.1.0/`** folder). Legacy **`repeater-1.0.x`** labels still work — see [`docs/REPEATER_RELEASE_PROCEDURE.md`](../docs/REPEATER_RELEASE_PROCEDURE.md).

**Companion** (USB+TCP meshcomod radio): after versioned builds in `out/`, run:

```bash
sh scripts/copy-release-bins.sh <version>
```

Produces e.g. `prebuilt/heltec_v4_companion_radio_usb_tcp.bin` and `prebuilt/releases/<version>/…`.

**Heltec V4 meshcomod extras** (OLED/TFT **USB-only**, **BLE**, **Wi‑Fi** companions; **plain** OLED/TFT repeaters — not the meshcomod USB+TCP row above): after the same `FIRMWARE_VERSION` builds in `out/`, run:

```bash
sh scripts/copy-heltec-v4-meshcomod-extras.sh <version>
```

Copies stable names into `prebuilt/` and `prebuilt/releases/<version>/`. See [`releases/v1.14.1.0/notes.md`](releases/v1.14.1.0/notes.md) for the file list.

**Repeater TCP** (Heltec V4 / V3 Wi‑Fi companion subset): build with **`REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp`**, then:

```bash
sh scripts/copy-repeater-release-bins.sh v1.14.1.0
```

Produces / updates:

- `prebuilt/heltec_v4_repeater_tcp.bin`, optional **`heltec_v4_tft_repeater_tcp.bin`** (+ merged variants when built)
- `prebuilt/Heltec_v3_repeater_tcp.bin`
- Same stable names under **`prebuilt/releases/v1.14.1.0/`** (with companion binaries for that release)

**Merged** images (`*-merged.bin`, flash from **0x0**): `build.sh build-firmware` / `build-repeater-firmwares` runs `mergebin` for `companion_radio_usb_tcp*` and `*_repeater_tcp` envs and copies `*-merged.bin` into `out/` before you run the copy script.

## Build inputs (versioned `out/` names)

`build.sh build-firmware …` requires **`FIRMWARE_VERSION`** in the environment and writes:

`out/<env>-<version>-<gitsha>.bin`

Repeater example (aligned with companion **v1.14.1.0**):

```bash
export REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp
sh build.sh build-repeater-firmwares
sh scripts/copy-repeater-release-bins.sh v1.14.1.0
```

Ad-hoc **`pio run`** (without `build.sh`) still drops **`out/<env>.bin`** and stamped copies via `merge-bin.py`; those names **do not** match the copy scripts — use **`build.sh`** (or manual rename) before promoting to `prebuilt/`.

See also: [`docs/RELEASE_PROCEDURE.md`](../docs/RELEASE_PROCEDURE.md) (companion), [`docs/REPEATER_RELEASE_PROCEDURE.md`](../docs/REPEATER_RELEASE_PROCEDURE.md) (repeater), [`WHERE_IS_REPEATER_FIRMWARE.md`](../WHERE_IS_REPEATER_FIRMWARE.md), [`docs/REPEATER_TCP_COMPANION.md`](../docs/REPEATER_TCP_COMPANION.md).
