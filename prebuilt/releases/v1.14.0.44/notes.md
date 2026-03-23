## Companion firmware v1.14.0.44

Date: 2026-03-23

### Highlights
- **Companion OTA transport parity fix:** temporarily pauses TCP/WSS server sockets while OTA URL download runs, then restores TCP on non-rebooting failures.
- **Targets companion-only failure mode:** avoids socket/resource contention that can block outbound OTA connects in multi-transport companion mode.
- **Retains all prior URL fallbacks:** raw GitHub → jsDelivr → repeater/flasher proxy hosts (HTTPS/HTTP).

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
