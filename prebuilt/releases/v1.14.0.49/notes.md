## Companion firmware v1.14.0.49

Date: 2026-03-23

### Highlights

- **Proxy BOM/whitespace:** OTA peek strips UTF-8 BOM and leading ASCII whitespace before checking ESP image magic (`0xE9`).
- **Stream vs flash length:** end-of-download validation uses remaining HTTP body bytes, not bytes written to flash.

### Binaries

- `heltec_v4_companion_radio_usb_tcp.bin` / `-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin` / `-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin` / `-merged.bin`

