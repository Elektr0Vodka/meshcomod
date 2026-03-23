#!/usr/bin/env bash
# Copy TCP repeater build outputs from out/ to prebuilt/ and prebuilt/releases/<dir>.
# Same layout as scripts/copy-release-bins.sh (companion).
#
# **Companion-aligned naming (recommended):** use the same base as companion, e.g. **v1.14.1.0**.
#   export REPEATER_FIRMWARE_VERSION=v1.14.1.0-repeater-tcp
#   sh build.sh build-repeater-firmwares
#   sh scripts/copy-repeater-release-bins.sh v1.14.1.0
# → matches `out/*-meshcomod-v1.14.1.0-repeater-tcp-<sha>.bin` and copies into **prebuilt/releases/v1.14.1.0/** (same folder as companion).
#
# You can also pass **v1.14.1.0-repeater-tcp** explicitly (same result).
#
# **Legacy:** `repeater-1.0.x` still works — release dir stays **prebuilt/releases/repeater-1.0.x/**.
#
# Usage: sh scripts/copy-repeater-release-bins.sh <companion-base|full-repeater-tag|legacy>
# Examples:
#   sh scripts/copy-repeater-release-bins.sh v1.14.1.0
#   sh scripts/copy-repeater-release-bins.sh v1.14.1.0-repeater-tcp
#   sh scripts/copy-repeater-release-bins.sh repeater-1.0.0

set -e
VERSION_ARG="${1:?Usage: $0 <version> e.g. v1.14.1.0 or v1.14.1.0-repeater-tcp}"

# Map argument → (glob tag in out/ filenames) + (releases/ subfolder).
# build.sh sets FIRMWARE_FILENAME=*-meshcomod-${REPEATER_FIRMWARE_VERSION}-${sha}.bin for *_repeater_tcp.
if [[ "$VERSION_ARG" =~ ^v[0-9]+[.][0-9]+[.][0-9]+[.][0-9]+$ ]]; then
  RELDIR_VERSION="$VERSION_ARG"
  GLOB_VERSION="${VERSION_ARG}-repeater-tcp"
elif [[ "$VERSION_ARG" == v*-repeater-tcp ]]; then
  RELDIR_VERSION="${VERSION_ARG%-repeater-tcp}"
  GLOB_VERSION="$VERSION_ARG"
else
  RELDIR_VERSION="$VERSION_ARG"
  GLOB_VERSION="$VERSION_ARG"
fi

RELDIR="prebuilt/releases/${RELDIR_VERSION}"
mkdir -p "prebuilt" "$RELDIR"
echo "Repeater copy: matching out/*-${GLOB_VERSION}-*.bin → ${RELDIR}/"

copy_one() {
  local src="$1"
  local dest_name="$2"
  if [ -z "$src" ] || [ ! -f "$src" ]; then
    echo "Not found: $dest_name"
    exit 1
  fi
  cp "$src" "prebuilt/$dest_name"
  cp "$src" "$RELDIR/$dest_name"
  echo "Copied -> prebuilt/$dest_name and $RELDIR/$dest_name"
}

copy_one_optional() {
  local src="$1"
  local dest_name="$2"
  if [ -z "$src" ] || [ ! -f "$src" ]; then
    echo "Skip (optional): $dest_name — sh build.sh build-firmware <env> runs mergebin for *_repeater_tcp into out/, or pio run -t mergebin -e <env>"
    return 0
  fi
  cp "$src" "prebuilt/$dest_name"
  cp "$src" "$RELDIR/$dest_name"
  echo "Copied -> prebuilt/$dest_name and $RELDIR/$dest_name"
}

# Newest by mtime when multiple same-version bins exist (matches copy-release-bins.sh).
# build.sh names images either heltec_v4_repeater_tcp-${GLOB_VERSION}-<sha>.bin or
# heltec_v4_repeater_tcp-meshcomod-${GLOB_VERSION}-<sha>.bin (meshcomod- prefix from build.sh).
V4_PLAIN=$(ls -t out/heltec_v4_repeater_tcp-"${GLOB_VERSION}"-*.bin out/heltec_v4_repeater_tcp-meshcomod-"${GLOB_VERSION}"-*.bin 2>/dev/null | grep -v merged | head -1)
V4_MERGED=$(ls -t out/heltec_v4_repeater_tcp-"${GLOB_VERSION}"-*-merged.bin out/heltec_v4_repeater_tcp-meshcomod-"${GLOB_VERSION}"-*-merged.bin 2>/dev/null | head -1)
V4TFT_PLAIN=$(ls -t out/heltec_v4_tft_repeater_tcp-"${GLOB_VERSION}"-*.bin out/heltec_v4_tft_repeater_tcp-meshcomod-"${GLOB_VERSION}"-*.bin 2>/dev/null | grep -v merged | head -1)
V4TFT_MERGED=$(ls -t out/heltec_v4_tft_repeater_tcp-"${GLOB_VERSION}"-*-merged.bin out/heltec_v4_tft_repeater_tcp-meshcomod-"${GLOB_VERSION}"-*-merged.bin 2>/dev/null | head -1)
V3_PLAIN=$(ls -t out/Heltec_v3_repeater_tcp-"${GLOB_VERSION}"-*.bin out/Heltec_v3_repeater_tcp-meshcomod-"${GLOB_VERSION}"-*.bin 2>/dev/null | grep -v merged | head -1)
V3_MERGED=$(ls -t out/Heltec_v3_repeater_tcp-"${GLOB_VERSION}"-*-merged.bin out/Heltec_v3_repeater_tcp-meshcomod-"${GLOB_VERSION}"-*-merged.bin 2>/dev/null | head -1)

copy_one "$V4_PLAIN" "heltec_v4_repeater_tcp.bin"
copy_one_optional "$V4_MERGED" "heltec_v4_repeater_tcp-merged.bin"
copy_one_optional "$V4TFT_PLAIN" "heltec_v4_tft_repeater_tcp.bin"
copy_one_optional "$V4TFT_MERGED" "heltec_v4_tft_repeater_tcp-merged.bin"
copy_one "$V3_PLAIN" "Heltec_v3_repeater_tcp.bin"
copy_one_optional "$V3_MERGED" "Heltec_v3_repeater_tcp-merged.bin"

echo "Done. prebuilt/ and $RELDIR updated."
