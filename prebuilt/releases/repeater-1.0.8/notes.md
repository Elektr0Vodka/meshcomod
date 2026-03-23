## repeater-1.0.8 — 2026-03-23

**TCP repeater prebuilt** (Heltec WiFi LoRa 32 **V4** + **V3**). Train: **`repeater-X.Y.Z`**, independent of companion **`v1.14.0.x`**.

**Compile-time version string:** `meshcomod-repeater-1.0.8-<gitsha>` (this build: **`f0e2e52a`** in `out/` filenames).

**Changes vs repeater-1.0.7**

- **Meshcomod splash timing:** The title / version / date screen is now timed from **`UITask::begin()`** (`millis() - s_ui_started_at`), not from power-on. Long `setup()` no longer expires the 3s window before the first `ui_task.loop()`, so you actually see the meshcomod splash after **Loading…** (matches companion `ui-new` behavior).

**Build:** **`DISABLE_DEBUG=1`**. App + **merged** (flash **0x0**) binaries.

| Device | Merged (0x0) | App-only |
|--------|--------------|----------|
| Heltec V4 | [heltec_v4_repeater_tcp-merged.bin](heltec_v4_repeater_tcp-merged.bin) | [heltec_v4_repeater_tcp.bin](heltec_v4_repeater_tcp.bin) |
| Heltec V3 | [Heltec_v3_repeater_tcp-merged.bin](Heltec_v3_repeater_tcp-merged.bin) | [Heltec_v3_repeater_tcp.bin](Heltec_v3_repeater_tcp.bin) |

**Procedure:** [`docs/REPEATER_RELEASE_PROCEDURE.md`](../../../docs/REPEATER_RELEASE_PROCEDURE.md).
