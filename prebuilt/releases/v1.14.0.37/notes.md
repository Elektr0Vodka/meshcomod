## Companion firmware v1.14.0.37

Date: 2026-03-23

### Highlights
- Fixes companion OTA command-path behavior where `ota url ...` could appear stalled with little or no feedback.
- Runs URL OTA in a background task and streams OTA progress lines back over Meshcomod during download/flash.
- Includes final OTA completion/error status over the same companion command transport.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
