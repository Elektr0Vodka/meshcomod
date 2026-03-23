## repeater-1.0.11 — 2026-03-23

> **Superseded for new downloads:** TCP repeater prebuilts for this generation are promoted under **[`v1.14.1.0/`](../v1.14.1.0/)** with aligned naming **`meshcomod-v1.14.1.0-repeater-tcp-<sha>`**. This folder is kept for **rollback / history** only.

**Train:** meshcomod TCP repeater images built against **MeshCore 1.14.1** merge (companion release **v1.14.1.0**).

**Compile-time version string:** `meshcomod-repeater-1.0.11-<gitsha>` (this build: **`47c3fb1c`** in `out/` filenames).

**Highlights**

- Same **Wi‑Fi‑only OTA** gating and **minimal transport** behavior as companion v1.14.1.0.
- **Heltec V4 TFT** repeater TCP included (merged + app-only where built).

**Prebuilt binaries**

| Device | Merged (0x0) | App-only |
|--------|----------------|----------|
| Heltec V4 (OLED) TCP | [heltec_v4_repeater_tcp-merged.bin](heltec_v4_repeater_tcp-merged.bin) | [heltec_v4_repeater_tcp.bin](heltec_v4_repeater_tcp.bin) |
| Heltec V4 TFT TCP | [heltec_v4_tft_repeater_tcp-merged.bin](heltec_v4_tft_repeater_tcp-merged.bin) | [heltec_v4_tft_repeater_tcp.bin](heltec_v4_tft_repeater_tcp.bin) |
| Heltec V3 TCP | [Heltec_v3_repeater_tcp-merged.bin](Heltec_v3_repeater_tcp-merged.bin) | [Heltec_v3_repeater_tcp.bin](Heltec_v3_repeater_tcp.bin) |

**Procedure:** [`docs/REPEATER_RELEASE_PROCEDURE.md`](../../../docs/REPEATER_RELEASE_PROCEDURE.md), [`scripts/copy-repeater-release-bins.sh`](../../../scripts/copy-repeater-release-bins.sh).
