# Release procedure (companion firmware)

Use this for every new fix or release. **Rule: every release gets a new version number; never overwrite an existing version’s binaries.**

Current latest version in the repo: **v1.14.0.38** (bump to next for the following release).

---

## 1. Bump version

- Choose the next version (e.g. `v1.14.0.20`).
- In **`examples/companion_radio/MyMesh.h`**, update the fallback:
  ```c
  #define FIRMWARE_VERSION "v1.14.0.20"
  ```

---

## 2. Build both targets

From **`MeshCore/`**:

```bash
export FIRMWARE_VERSION=v1.14.0.20

sh build.sh build-firmware heltec_v4_companion_radio_usb_tcp
sh build.sh build-firmware Heltec_v3_companion_radio_usb_tcp
sh build.sh build-firmware heltec_v4_tft_companion_radio_usb_tcp_touch
```

- Builds go to `out/`. For ESP32 meshcomod **`companion_radio_usb_tcp*`** envs, **`build.sh` also runs `mergebin`** and copies **`…-merged.bin`** next to the app-only bin (same flow for OLED V4, V3, and TFT+touch V4).
- Both runs add their bins to `out/` (script does not clear `out/` for `build-firmware`).

---

## 3. Copy release binaries

```bash
sh scripts/copy-release-bins.sh v1.14.0.20
```

- Copies the companion binaries from `out/` into:
  - **`prebuilt/`** (latest)
  - **`prebuilt/releases/v1.14.0.20/`** (versioned)
- Script expects versioned filenames in `out/` (e.g. `heltec_v4_companion_radio_usb_tcp-v1.14.0.20-...bin`).

---

## 4. Update docs

- **`prebuilt/releases/v1.14.0.20/notes.md`**  
  Add release notes (highlights, fixes, build date).

- **`RELEASES.md`**  
  Add a new section at the **top** (below “Release process”):
  - Version and date
  - Highlights (bullet list)
  - Table with links to the binaries in `prebuilt/releases/v1.14.0.20/` (V4 OLED, V4 TFT+touch, V3 — merged + non-merged where applicable)

Use the existing v1.14.0.19 block in `RELEASES.md` as the template (heading, **Firmware version**, **Highlights**, **Prebuilt binaries** table).

---

## 5. Commit and push

- Stage: prebuilts, `RELEASES.md`, `notes.md`, and any code (e.g. `MyMesh.h`).
- Commit with a clear message (e.g. `Release v1.14.0.20 – WSS fixes`).
- Push to **allfather**:  
  `git push allfather main`

---

## Summary

| Step | Action |
|------|--------|
| 1 | Bump version in `examples/companion_radio/MyMesh.h` |
| 2 | `export FIRMWARE_VERSION=vX.Y.Z.W` then build V4 and V3 with `build.sh` |
| 3 | `sh scripts/copy-release-bins.sh vX.Y.Z.W` |
| 4 | Add `prebuilt/releases/vX.Y.Z.W/notes.md` and new section in `RELEASES.md` |
| 5 | Commit and `git push allfather main` |

- **`prebuilt/`** = latest only (overwritten each release).
- **`prebuilt/releases/<version>/`** = one folder per version (bins + `notes.md`); keep all for rollback.

---

## Repeater TCP prebuilts

Repeater releases use **`repeater-X.Y.Z`** (not companion **`v1.14.0.x`**). Procedure: **[`REPEATER_RELEASE_PROCEDURE.md`](REPEATER_RELEASE_PROCEDURE.md)**.

---

## Optional: GitHub Actions (other firmware)

For **companion / repeater / room-server** firmware built by GitHub Actions, push a tag:

- `companion-v1.0.0`
- `repeater-v1.0.0`
- `room-server-v1.0.0`

A draft GitHub Release is created; update the release notes and publish it. See **`RELEASE.md`** in the repo root.
