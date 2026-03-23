## Companion firmware v1.14.0.47

Date: 2026-03-24

### Highlights

- **OTA HTTP vs HTTPS client:** `startHttpOtaFromUrl` uses `WiFiClient` for `http://` URLs and `WiFiClientSecure` only for `https://`. Fixes companion failures on meshcomod HTTP proxy fallbacks (`connection refused` while DNS resolved correctly).
- **No TCP/WSS shutdown** for OTA; existing app connections are not intentionally dropped.

### Binaries

- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
