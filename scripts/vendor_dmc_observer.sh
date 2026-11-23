#!/usr/bin/env bash
# Copies the Dutch-MeshCore observer sources this fork builds into the _mqtt
# companion envs, then applies the fork's local patches.
#
#   bash scripts/vendor_dmc_observer.sh <dmc-checkout> <commit>
#
# <dmc-checkout> is a clone of https://github.com/Dutch-MeshCore/MeshCore.
# <commit> is checked out there first (normally a dmc-observer-dev commit).
# The file list is scripts/dmc_observer_files.txt, the patches are
# scripts/dmc_observer_patches/*.patch (applied in name order). The commit is
# recorded in docs/dmc_observer_vendor.md. See that file for why each patch exists.
set -euo pipefail

if [ $# -ne 2 ]; then
  echo "usage: $0 <dmc-checkout> <commit>" >&2
  exit 2
fi
src="$1"
commit="$2"
root="$(cd "$(dirname "$0")/.." && pwd)"

git -C "$src" checkout -q "$commit"
full="$(git -C "$src" rev-parse HEAD)"

while IFS= read -r path; do
  path="${path%$'\r'}"
  case "$path" in ''|'#'*) continue ;; esac
  if [ -d "$src/$path" ]; then
    rm -rf "${root:?}/$path"
    mkdir -p "$root/$path"
    # Library sources and metadata only; examples and helper scripts stay upstream.
    (cd "$src/$path" && git ls-files | grep -v -E '^(examples|scripts)/') | while IFS= read -r f; do
      mkdir -p "$root/$path/$(dirname "$f")"
      cp "$src/$path/$f" "$root/$path/$f"
    done
  else
    mkdir -p "$root/$(dirname "$path")"
    cp "$src/$path" "$root/$path"
  fi
done < "$root/scripts/dmc_observer_files.txt"

# DMC's ConfigSerializer parses more strictly than ours (and adds defStrict, which
# the /mqtt.json schema needs). Ours stays as it is for every other prefs file; the
# observer gets DMC's as a renamed copy, ObserverConfigSerializer.
for ext in h cpp; do
  sed -e 's/\bConfigSerializer\b/ObserverConfigSerializer/g' \
      -e 's/"ConfigSerializer\.h"/"ObserverConfigSerializer.h"/' \
      "$src/src/helpers/ConfigSerializer.$ext" > "$root/src/helpers/ObserverConfigSerializer.$ext"
done
sed -i -e 's/\bConfigSerializer\b/ObserverConfigSerializer/g' \
       -e 's#<helpers/ConfigSerializer\.h>#<helpers/ObserverConfigSerializer.h>#' \
       "$root/src/helpers/MQTTPrefsSerializer.h"

for p in "$root"/scripts/dmc_observer_patches/*.patch; do
  [ -e "$p" ] || continue
  git -C "$root" apply --whitespace=nowarn "$p"
  echo "applied $(basename "$p")"
done

# Sources DMC compiles everywhere. Here only the _mqtt envs (and native tests) may:
# about 30 variant files re-add +<helpers/*.cpp> after the base filter's excludes,
# so each file also carries its own guard.
guard='#if defined(WITH_MQTT_BRIDGE) || !defined(ARDUINO)   // meshcomod: _mqtt envs and native tests only'
for f in DutyCycleLimits.cpp MQTTPayloadBuilder.cpp NetworkLink.cpp ObserverConfigSerializer.cpp \
         ESP32DnsThreadFix.cpp ESP32Ipv6Preference.cpp ESP32TcpBindReuse.cpp ESP32WsTransportFix.cpp; do
  p="$root/src/helpers/$f"
  { printf '%s\n' "$guard"; cat "$p"; printf '\n#endif  // meshcomod guard\n'; } > "$p.tmp" && mv "$p.tmp" "$p"
done

sed -i "s/^Pinned commit: .*/Pinned commit: \`$full\`/" "$root/docs/dmc_observer_vendor.md"
echo "vendored Dutch-MeshCore/MeshCore $full"
