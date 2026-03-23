## Companion firmware v1.14.0.38

Date: 2026-03-23

### Highlights
- Fixes companion OTA trigger path used by the OTA panel (`CMD_SEND_RAW_DATA` with empty path + `ota url ...` text).
- Routes that OTA command to local companion Meshcomod OTA handler instead of sending it as generic mesh raw payload.
- Mirrors OTA progress and final status as `PUSH_CODE_BINARY_RESPONSE` lines so the companion OTA panel receives live output consistently.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
