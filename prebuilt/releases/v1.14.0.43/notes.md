## Companion firmware v1.14.0.43

Date: 2026-03-23

### Highlights
- **Companion OTA network fallback chain:** after raw GitHub and jsDelivr, firmware now retries meshcomod firmware proxies (`repeater.meshcomod.com` / `flasher.meshcomod.com`) over HTTPS and then HTTP.
- **Designed for restricted Wi‑Fi paths:** if direct outbound 443 to GitHub/CDN is blocked or refused, OTA can still fetch through meshcomod proxy hosts used by repeater tooling.
- **Keeps verbose diagnostics:** retry/error lines are emitted for each stage, preserving actionable OTA telemetry.

### Binaries
- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
