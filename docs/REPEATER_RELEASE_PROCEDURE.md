# TCP repeater release procedure (prebuilt / flasher)

**Repeater TCP** uses the **same base version** as the meshcomod companion (**`v1.14.1.x`**) plus a fixed suffix **`‑repeater-tcp`** so users see one family of versions. **`build.sh`** prefixes the compile-time macro with **`meshcomod-`**, e.g. **`meshcomod-v1.14.1.0-repeater-tcp-<gitsha>`** on the device / in `out/` filenames.

**Legacy:** older drops used **`repeater-1.0.x`** directories; those are no longer in this repo (see **git history**). The copy script still accepts **`repeater-X.Y.Z`** if you recreate that layout in a fork.

---

## 1. Choose the version (aligned with companion)

When you ship companion **`v1.14.1.0`**, build repeater TCP with:

- **`REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp`**

Bump the **`v1.14.1.x`** part whenever you bump the companion for that release train. If you ever need a **repeater-only** fix without a companion bump, increment the patch segment (e.g. **`v1.14.1.1-repeater-tcp`**) and document it in **`RELEASES.md`**.

---

## 2. Build from `MeshCore/`

```bash
export REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp
export DISABLE_DEBUG=1   # recommended for release
sh build.sh build-repeater-firmwares
```

- You do **not** need **`FIRMWARE_VERSION`** if **`REPEATER_FIRMWARE_VERSION`** is set.
- `out/` will contain e.g. **`heltec_v4_repeater_tcp-meshcomod-v1.14.1.0-repeater-tcp-<sha>.bin`** and the V3 / V4 TFT analogues, plus optional **`…-merged.bin`** (full image at **0x0**) after **`mergebin`**.

---

## 3. Promote to `prebuilt/`

Pass the **companion base** (four-part **`vX.Y.Z.W`**) so TCP repeater bins land in the **same** folder as that companion release:

```bash
sh scripts/copy-repeater-release-bins.sh v1.14.1.0
```

You can also pass **`v1.14.1.0-repeater-tcp`** explicitly — same result.

Produces / updates:

- **`prebuilt/heltec_v4_repeater_tcp.bin`**, **`prebuilt/Heltec_v3_repeater_tcp.bin`**, optional **`prebuilt/heltec_v4_tft_repeater_tcp*.bin`**
- The same stable names under **`prebuilt/releases/v1.14.1.0/`** (alongside companion binaries)

Document TCP repeater rows in **`prebuilt/releases/v1.14.1.0/notes.md`** and the top section of **`RELEASES.md`**.

**Legacy copy** (unchanged behavior):

```bash
export REPEATER_FIRMWARE_VERSION=repeater-1.0.0
sh build.sh build-repeater-firmwares
sh scripts/copy-repeater-release-bins.sh repeater-1.0.0
```

→ **`prebuilt/releases/repeater-1.0.0/`**

---

## 4. Commit (and push)

Stage **`prebuilt/`**, **`prebuilt/releases/<version>/`**, **`RELEASES.md`**, and notes. Push to your meshcomod remote (e.g. **`allfather`**).

**Git tag (optional):** bookkeeping tags are fine; canonical binaries for flasher / OTA live under **`prebuilt/`** on **`main`**. CI: **Actions → Build Repeater Firmwares** — use the same **`REPEATER_FIRMWARE_VERSION`** string you built with.

---

## Summary

| Step | Action |
|------|--------|
| 1 | Set **`REPEATER_FIRMWARE_VERSION=vX.Y.Z.W-repeater-tcp`** (same base as companion) |
| 2 | **`sh build.sh build-repeater-firmwares`** |
| 3 | **`sh scripts/copy-repeater-release-bins.sh vX.Y.Z.W`** |
| 4 | Update **`notes.md`** / **`RELEASES.md`**, commit, push |

See also: [`prebuilt/README.md`](../prebuilt/README.md), [`REPEATER_TCP_COMPANION.md`](REPEATER_TCP_COMPANION.md).
