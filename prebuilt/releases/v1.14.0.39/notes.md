## Companion firmware v1.14.0.39

Date: 2026-03-23

### Highlights
- Stabilizes companion OTA over HTTPS by increasing OTA task stack size for URL OTA processing.
- Removes mixed duplicate OTA output paths by emitting OTA progress/final status only as binary response frames during OTA sessions.
- Reduces garbled/duplicated OTA lines and improves OTA panel compatibility.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
