## repeater-1.0.7 — 2026-03-23

**TCP repeater prebuilt** (Heltec WiFi LoRa 32 **V4** + **V3**). Train: **`repeater-X.Y.Z`**, independent of companion **`v1.14.0.x`**.

**Compile-time version string:** `meshcomod-repeater-1.0.7-<gitsha>` (example build: `a995cc02`).

**Changes vs repeater-1.0.6**

- **Boot OLED:** `repeater_tcp/main.cpp` now draws a **“Loading…”** full frame immediately after `display.begin()` (same pattern as companion), so the panel does not show random framebuffer noise (“static”) during radio/SPIFFS/mesh setup.
- **Loop order:** **`ui_task.loop()`** runs **before** **`the_mesh.loop()`** so the splash timer and first UI paint are not delayed by mesh work.
- **Prebuilts:** **`build.sh`** now runs **`mergebin`** for **`*_repeater_tcp`** envs; this drop includes **`*-merged.bin`** (flash from **0x0**) alongside app-only bins.

**Build:** **`DISABLE_DEBUG=1`**.

**Images**

| Device | Merged (0x0) | App-only (typ. 0x10000) |
|--------|----------------|-------------------------|
| Heltec V4 | [heltec_v4_repeater_tcp-merged.bin](heltec_v4_repeater_tcp-merged.bin) | [heltec_v4_repeater_tcp.bin](heltec_v4_repeater_tcp.bin) |
| Heltec V3 | [Heltec_v3_repeater_tcp-merged.bin](Heltec_v3_repeater_tcp-merged.bin) | [Heltec_v3_repeater_tcp.bin](Heltec_v3_repeater_tcp.bin) |

**Procedure:** [`docs/REPEATER_RELEASE_PROCEDURE.md`](../../../docs/REPEATER_RELEASE_PROCEDURE.md). **Protocol / client:** [`docs/REPEATER_TCP_COMPANION.md`](../../../docs/REPEATER_TCP_COMPANION.md).
