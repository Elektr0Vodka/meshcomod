## Companion firmware v1.14.0.45

Date: 2026-03-23

### Highlights
- **Reverted TCP/WSS pause during OTA:** companion no longer disables transport servers when `ota url` starts, preserving app session behavior closer to repeater UX.
- **Keeps OTA fallback chain:** raw GitHub → jsDelivr → repeater/flasher proxies (HTTPS/HTTP).
- **No transport teardown side effects:** OTA command path remains connection-friendly while diagnostics stay verbose.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
