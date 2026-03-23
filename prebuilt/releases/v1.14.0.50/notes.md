## Companion firmware v1.14.0.50

Date: 2026-03-23

### Highlights

- **Skip bad OTA mirrors:** HTML / gzip / wrong magic on one host no longer aborts OTA; the next mirror in the chain is tried until one serves a valid ESP `.bin` or all fail.

### Binaries

- `heltec_v4_companion_radio_usb_tcp.bin` / `-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin` / `-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin` / `-merged.bin`
