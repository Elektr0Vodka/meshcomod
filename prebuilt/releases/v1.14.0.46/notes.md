## Companion firmware v1.14.0.46

Date: 2026-03-23

### Root cause (companion vs repeater OTA)

- **Repeater TCP:** `ota url` is handled in the repeater’s command path while the main loop is in the **TCP/WS poll phase** (before `the_mesh.loop()`), so outbound HTTP OTA runs with the same scheduling as other CLI work.
- **Companion (before this release):** OTA was **deferred to the start of the next `MyMesh::loop()`**, interleaving with radio mesh work and multi-transport servicing. That timing differed from the repeater and correlated with `ERR: HTTP -1 (connection refused)` on the same network where repeater OTA succeeded.

### Fix

- **Companion `ota url`:** runs **`board.startHttpOtaFromUrl` synchronously** inside the meshcomod command handler (after a quick transport ack where needed), matching repeater-style “handle CLI now” semantics without pausing TCP/WSS.
- **Shared transport (`ESP32Board`):** identical OTA diagnostics and retry cleanup (`https.end()` / `client.stop()` between attempts), **`ota netdiag`** via `emitHttpOtaNetDiagnosticLines()`, and a guard against overlapping OTA.

### Parity check (on your network)

1. Flash repeater TCP and companion to **v1.14.0.46** (or build from this tag).
2. On the **same Wi‑Fi**, run the **same** `ota url <...>` on both.
3. Compare lines starting with **`OTA: diag`** and **`OTA: try`** — host/DNS/IP and HTTP codes should match when behavior is healthy; if not, save both logs for analysis.

### Binaries

- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
