## Companion firmware v1.14.0.40

Date: 2026-03-23

### Highlights
- **HTTPS OTA on Arduino loop task:** `ota url` is queued and runs on the next `MyMesh::loop()` iteration instead of a separate FreeRTOS task, avoiding `ERR: HTTP -1` (connection failed) that often occurs when TLS/HTTP runs off the main Arduino task.
- **Live OTA lines over binary push:** Implements `meshcoreRepeaterTcpOtaEmitLine` for companion/WiFi builds so `ESP32Board` can stream progress (`0x8C`) during the blocking download, matching repeater behaviour.
- **Cleaner OTA text:** Removes the extra `loop()`-based progress path that prefixed `OTA:` onto display strings that already contained `OTA:`, fixing `OTA: OTA: connecting`-style duplication.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
