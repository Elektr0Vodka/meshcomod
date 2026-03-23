# Heltec V4 prebuilt firmware (meshcomod)

App images (`*.bin`) are **OTA-ready** application partitions (not full merged flash at `0x0`).

## OLED (SSD1306)

| File | PlatformIO environment |
|------|-------------------------|
| `heltec_v4_companion_radio_usb.bin` | USB serial companion |
| `heltec_v4_companion_radio_ble.bin` | BLE companion |
| `heltec_v4_companion_radio_wifi.bin` | Wi-Fi TCP companion (compile-time SSID) |
| `heltec_v4_companion_radio_usb_tcp.bin` | USB + TCP + WebSocket + BLE (meshcomod) |
| `heltec_v4_repeater.bin` | OLED repeater |
| `heltec_v4_repeater_tcp.bin` | OLED repeater + TCP/WS companion (Wi-Fi NVS) |

## TFT (ST7789)

| File | PlatformIO environment |
|------|-------------------------|
| `heltec_v4_tft_companion_radio_usb.bin` | TFT USB companion |
| `heltec_v4_tft_companion_radio_ble.bin` | TFT BLE companion |
| `heltec_v4_tft_companion_radio_wifi.bin` | TFT Wi-Fi TCP companion |
| `heltec_v4_tft_companion_radio_usb_tcp_touch.bin` | TFT + capacitive touch, USB+TCP+WS+BLE, WSS, merge-bin |
| `heltec_v4_tft_repeater.bin` | TFT repeater |
| `heltec_v4_tft_repeater_tcp.bin` | TFT repeater + TCP/WS companion |

**Companion build:** `v1.14.0.41` (see `examples/companion_radio/MyMesh.h`).

Rebuild locally from `variants/heltec_v4`:

```bash
pio run -e heltec_v4_companion_radio_usb -e heltec_v4_companion_radio_ble \
  -e heltec_v4_companion_radio_wifi -e heltec_v4_companion_radio_usb_tcp \
  -e heltec_v4_repeater -e heltec_v4_repeater_tcp \
  -e heltec_v4_tft_companion_radio_usb -e heltec_v4_tft_companion_radio_ble \
  -e heltec_v4_tft_companion_radio_wifi -e heltec_v4_tft_companion_radio_usb_tcp_touch \
  -e heltec_v4_tft_repeater -e heltec_v4_tft_repeater_tcp
```

Then refresh copies in this folder from `.pio/build/<env>/firmware.bin` or `out/<env>.bin`.
