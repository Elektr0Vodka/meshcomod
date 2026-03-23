## repeater-1.0.10 — 2026-03-23

**TCP repeater prebuilt** (Heltec WiFi LoRa 32 **V4 OLED**, **V4 TFT**, **V3**). Train: **`repeater-X.Y.Z`**, independent of companion **`v1.14.0.x`**.

**Compile-time version string:** `meshcomod-repeater-1.0.10-<gitsha>` (this build: **`872fd926`** in `out/` filenames).

**Changes vs repeater-1.0.9**

- **Wi‑Fi‑only HTTP OTA:** `ota url` only from **TCP or WebSocket** meshcomod session; USB serial rejected with clear `ERR:`.
- **OTA minimal mode:** Stops the non‑active TCP or WS server during download; restores on failure.
- **Heltec V4 TFT:** Added **heltec_v4_tft_repeater_tcp** prebuilts (app + merged) alongside OLED and V3.

**Build:** **`DISABLE_DEBUG=1`**. App + **merged** (flash **0x0**) where present.

| Device | Merged (0x0) | App-only |
|--------|--------------|----------|
| Heltec V4 OLED | [heltec_v4_repeater_tcp-merged.bin](heltec_v4_repeater_tcp-merged.bin) | [heltec_v4_repeater_tcp.bin](heltec_v4_repeater_tcp.bin) |
| Heltec V4 TFT | [heltec_v4_tft_repeater_tcp-merged.bin](heltec_v4_tft_repeater_tcp-merged.bin) | [heltec_v4_tft_repeater_tcp.bin](heltec_v4_tft_repeater_tcp.bin) |
| Heltec V3 | [Heltec_v3_repeater_tcp-merged.bin](Heltec_v3_repeater_tcp-merged.bin) | [Heltec_v3_repeater_tcp.bin](Heltec_v3_repeater_tcp.bin) |

**Procedure:** [`docs/REPEATER_RELEASE_PROCEDURE.md`](../../../docs/REPEATER_RELEASE_PROCEDURE.md).
