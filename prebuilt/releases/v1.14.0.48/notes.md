## Companion firmware v1.14.0.48

Date: 2026-03-23

### Highlights

- **HTTP-proxy-first OTA for meshcomod main builds:** tries `http://repeater.meshcomod.com` then `http://flasher.meshcomod.com` before other mirrors.
- **OTA stream hardening:** rejects non-firmware content types, checks ESP image magic (`0xE9`) at stream start, uses `Update.begin(UPDATE_SIZE_UNKNOWN)`, and validates downloaded size.
- **Merged-bin guard:** `ota url` rejects `*-merged.bin` and requires non-merged app `.bin`.
- **No TCP/WSS teardown** during OTA.

### Binaries

- `heltec_v4_companion_radio_usb_tcp.bin`
- `heltec_v4_companion_radio_usb_tcp-merged.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch.bin`
- `heltec_v4_tft_companion_radio_usb_tcp_touch-merged.bin`
- `Heltec_v3_companion_radio_usb_tcp.bin`
- `Heltec_v3_companion_radio_usb_tcp-merged.bin`
