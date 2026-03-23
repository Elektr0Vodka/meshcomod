## Companion firmware v1.14.0.41

Date: 2026-03-23

### Highlights
- **HTTP OTA connect hardening:** Adds retry logic around initial HTTPS GET with reconnect waits, reducing transient `ERR: HTTP -1` failures.
- **Actionable error text:** Negative HTTP failures now include `HTTPClient::errorToString(...)` in the final error line (e.g. `ERR: HTTP -1 (connection refused)`).
- **Progress diagnostics:** OTA emits explicit connect retry/error lines over OTA binary response channel to simplify field debugging.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
