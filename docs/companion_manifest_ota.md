# Companion manifest OTA (`ota check` / `ota update`)

The Wi-Fi companions (`*_companion_radio_usb_tcp` envs) can look up and install the
newest published build themselves, instead of being handed a URL with `ota url`.
The logic is ported from the DMC observer firmware (`Dutch-MeshCore/MeshCore`, branch
`dmc-observer-dev`) and uses the same OTA server and manifest format.

## Commands

Send these to the meshcomod command contact (the same place as `ota url`).

| Command | What it does |
|---------|--------------|
| `ota check` | Fetches this build's manifest and reports `up to date`, `update available: <running> -> <published>`, or why it cannot update. Works from any transport while Wi-Fi is connected. |
| `ota update` | Runs the same check and, when an installable update exists, downloads and flashes it through the `ota url` path (progress lines, reboot on success). Like `ota url`, it must be started from a Wi-Fi TCP or WebSocket session on multi-transport builds. |

`ota update` does nothing when:

- the running build is the published one (`up to date`);
- the running build is newer than the published one (no downgrades);
- the published build was made for a different partition layout (`needs a USB flash`).
  OTA only rewrites the app slot, so a layout change needs the merged image over USB.

A build with the same version but a different commit counts as an update
(`same version, other build`), so a local build can be moved back onto the release.

## How it works

1. `build.sh` bakes two values into `*companion_radio_usb_tcp*` envs:
   - `OTA_VARIANT`: the env name, which selects the manifest file.
   - `OTA_MANIFEST_BASE`: default `https://ota.dutchmeshcore.nl/meshcomod/v`, overridable
     with `OTA_MANIFEST_BASE_URL`.
2. The device fetches `<base>/<env>.json` over plain HTTP (no TLS, so it is cheap on
   RAM-tight builds). The manifest looks like:

   ```json
   {"file":"https://ota.dutchmeshcore.nl/meshcomod/fw/<tag>/<env>-<version>-<hash>.bin",
    "version":"v1.18.0.2","hash":"abc1234","partSig":"1:2:9000:5000,..."}
   ```

3. The version is compared numerically (`v1.18.0.1` < `v1.18.0.2` < `v1.19.0.0`), then by
   commit hash when the versions match.
4. `partSig` is compared with the partition table flashed on the device. `build.sh`
   writes it to `out/<env>.partsig` with `scripts/partition_signature.py`, and the
   firmware computes the same string at runtime (`ota_partsig_format` in
   `src/helpers/OtaManifest.h`). Manifests without `partSig` skip this check.
5. The image URL must sit under the server's firmware path for this product
   (`<base>` with the trailing `/v` replaced by `/fw/`); anything else is refused.
   The download then runs through `startHttpOtaFromUrl`, the same code as `ota url`.

Builds made without `build.sh` have no manifest base and answer
`ERR: OTA manifest not configured (build via build.sh)`.

## Publishing

The OTA server mirrors this repo's GitHub releases tagged `companion-*` (see
[`CI_RELEASES.md`](CI_RELEASES.md)). A release needs, per env:

- `<env>-<version>-<hash>.bin` (the app image; `-merged.bin` is ignored), and
- `<env>.partsig` (optional, enables the partition-layout check).

The companion workflow uploads all of `out/`, so both come along with a normal tagged
build. Keep `FIRMWARE_VERSION` free of `-` (for example `v1.18.0.2`, not
`v1.18.0.2-DMC-EV`): the server splits asset names on `-` to find the env, version and
hash.

## Notes

- The firmware download uses HTTPS without certificate checks, exactly like `ota url`
  today. The ESP32 image checksum still catches a corrupted download.
- The touch env (`heltec_v4_tft_companion_radio_usb_tcp_touch`) is not in the CI
  companion build list, so it has no published manifest until it is added there.
