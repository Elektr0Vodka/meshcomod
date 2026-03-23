#!/usr/bin/env bash
# Copy additional Heltec V4 meshcomod builds from out/ into prebuilt/ and prebuilt/releases/<version>.
# Run after: export FIRMWARE_VERSION=<version> and sh build.sh build-firmware <each extra env>
# (meshcomod USB+TCP / TFT touch / V3 are handled by copy-release-bins.sh.)
#
# Usage: sh scripts/copy-heltec-v4-meshcomod-extras.sh <version>
# Example: sh scripts/copy-heltec-v4-meshcomod-extras.sh v1.14.1.0

set -e
VERSION="${1:?Usage: $0 <version> e.g. v1.14.0.41}"
RELDIR="prebuilt/releases/${VERSION}"
mkdir -p "prebuilt" "$RELDIR"

copy_latest() {
  local globpat="$1"
  local dest_name="$2"
  local src
  src=$(ls -t $globpat 2>/dev/null | head -1)
  if [ -z "$src" ] || [ ! -f "$src" ]; then
    echo "Not found: $dest_name (glob: $globpat)"
    exit 1
  fi
  cp "$src" "prebuilt/$dest_name"
  cp "$src" "$RELDIR/$dest_name"
  echo "Copied -> prebuilt/$dest_name and $RELDIR/$dest_name"
}

# App-only OTA images; version segment in out/ is ${VERSION}-<gitsha>
copy_latest "out/heltec_v4_companion_radio_usb-${VERSION}-*.bin" "heltec_v4_companion_radio_usb.bin"
copy_latest "out/heltec_v4_companion_radio_ble-${VERSION}-*.bin" "heltec_v4_companion_radio_ble.bin"
copy_latest "out/heltec_v4_companion_radio_wifi-${VERSION}-*.bin" "heltec_v4_companion_radio_wifi.bin"

copy_latest "out/heltec_v4_tft_companion_radio_usb-${VERSION}-*.bin" "heltec_v4_tft_companion_radio_usb.bin"
copy_latest "out/heltec_v4_tft_companion_radio_ble-${VERSION}-*.bin" "heltec_v4_tft_companion_radio_ble.bin"
copy_latest "out/heltec_v4_tft_companion_radio_wifi-${VERSION}-*.bin" "heltec_v4_tft_companion_radio_wifi.bin"

copy_latest "out/heltec_v4_repeater-${VERSION}-*.bin" "heltec_v4_repeater.bin"
copy_latest "out/heltec_v4_tft_repeater-${VERSION}-*.bin" "heltec_v4_tft_repeater.bin"

echo "Done. Heltec V4 meshcomod extras in prebuilt/ and $RELDIR"
