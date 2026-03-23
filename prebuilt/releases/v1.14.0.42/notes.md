## Companion firmware v1.14.0.42

Date: 2026-03-23

### Highlights
- **GitHub reachability fallback for OTA URL:** when `raw.githubusercontent.com` HTTPS connect is refused, OTA automatically retries through jsDelivr mirror URL (`cdn.jsdelivr.net`).
- **No UX change required:** users can keep sending the same GitHub raw URL; firmware chooses fallback automatically.
- **Retains prior diagnostics/retries:** connect retry telemetry and detailed `ERR: HTTP -1 (...)` reporting remain active.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
