# CI releases (GitHub Actions + GitHub Releases)

Firmware binaries are built by GitHub Actions and published as **GitHub Release
assets**. GitHub Releases are the **source of truth** for distribution (the web
flasher reads the release assets), so new bins are **not** committed into
`prebuilt/` anymore.

## How to cut a release

Push a tag on the track you want. The matching workflow builds the firmwares,
publishes a GitHub Release with the firmware files attached (`.bin` and merged
`.bin` for ESP32, `.uf2` and DFU `.zip` for nRF52), and prunes old
releases on that track (keeping the newest 5).

| Track | Tag prefix | Example tag | Workflow | Builds |
|-------|-----------|-------------|----------|--------|
| Companion | `companion-` | `companion-v1.16.0.1` | build-companion-firmwares | the envs in `MESHCOMOD_COMPANION_ENVS` in `build.sh` (V4, V3, Wireless Paper, Xiao, V4 R8, V4 R8 TFT, V4 TFT, Wireless Stick Lite V3, Wireless Tracker, Wireless Tracker V2, E213, Xiao S3, RAK3112, T-ETH Elite, T-Beam S3 Supreme, E290, T190, RC32, T-Beam 1W, Meshnology W12, Station G2, Station G3 and ThinkNode M9 `_usb_tcp`, ThinkNode M7 BLE and Ethernet, BLE and USB for 16 nRF52840 boards: Wio Tracker L1, T-Echo, T-Echo Card, T-Echo Lite, RAK4631, RAK3401, T1000-E, Heltec T114, T1, T096 and MeshPocket, ThinkNode M1 and M8, Nano G2 Ultra, Pro Micro and Keepteen LT1) |
| Room server | `room-server-` | `room-server-v1.16.0.1` | build-room-server-firmwares | `*_room_server` |
| Repeater | — | (manual `workflow_dispatch`) | build-repeater-firmwares | `*_repeater_tcp` |

> The **touch** UI firmware (Heltec V4 TFT, LilyGo T-Deck) is built and released
> from the **wadamesh** repo, not here. This repo is core / companion only.

```bash
# example: cut the companion v1.16.0.1 release
git tag companion-v1.16.0.1
git push origin companion-v1.16.0.1
```

The tag suffix becomes `FIRMWARE_VERSION` (e.g. `companion-v1.16.0.1` →
`v1.16.0.1`), so the in-firmware version string and the release name match the
tag.

The companion workflow builds each board on its own runner, in parallel. A
`prepare` job resolves the version once and reads the board list from
`build.sh get-meshcomod-companion-firmwares-to-build`, so adding an env to
`MESHCOMOD_COMPANION_ENVS` is all it takes to build and release it. PlatformIO
toolchains are cached per `platformio.ini`, and each board's libraries per
board. The release is only published when every board built; a failed board
shows in the run and blocks the release instead of shipping without it.

## Over-the-air updates

Companion releases also feed the DutchMeshCore OTA server, which mirrors the
newest stable `companion-*` release into per-board manifests for `ota check` /
`ota update` (see [Companion manifest OTA](companion_manifest_ota.md)). Each
companion env uploads `<env>.partsig` next to its `.bin`, so the device can refuse
a build made for a different partition layout. Only plain versions are served
(`companion-v1.18.0.2`): the server splits asset names on `-`, so pre-release
builds (`-pre42`, `-rc1`, see below) are not offered over the air.

## Companion pre-releases

There are two ways to publish companion bins as a GitHub **pre-release** (never
marked "latest", so `releases/latest` keeps pointing at the newest stable build):

- **Manual run:** Actions, then Build Companion Firmwares, then Run workflow, and
  tick **prerelease**. The workflow appends `-pre<run number>` to the version
  (so the firmware reports it too), creates the tag on the commit it built, and
  publishes the pre-release. Example: `companion-v1.18.0.1-pre42`.
- **Suffix tag:** push a tag with anything after the four-part version. It is
  published as a pre-release; a plain `companion-vX.Y.Z.W` tag stays a normal
  release.

```bash
git tag companion-v1.18.0.2-rc1
git push origin companion-v1.18.0.2-rc1
```

A manual run without the box ticked still only uploads the bins as a workflow
artifact.

## Rolling retention

After each release the workflow runs `scripts/ci/prune-releases.sh <prefix> 5`,
which keeps the **newest 5** releases on that track and deletes the older ones —
the GitHub Release, its git tag, and the attached assets. This keeps the release
list and the tag namespace small while still allowing rollback to the last few
versions.

The companion track counts stable releases and pre-releases separately
(`prune-releases.sh companion- 5 stable` and `prune-releases.sh companion- 3
prerelease`), so it keeps the newest 5 stable and newest 3 pre-releases and a
run of pre-releases never pushes stable ones out. The optional third argument
(`all`, `stable` or `prerelease`) defaults to `all`, which is what the
room-server track still uses.

To preview what would be pruned without deleting anything, run the workflow via
**Actions → (workflow) → Run workflow** after setting `DRY_RUN=1`, or locally:

```bash
DRY_RUN=1 scripts/ci/prune-releases.sh companion- 5
```

(Requires the GitHub CLI authenticated against the repo.)

## Migration notes

- The legacy flow committed bins to `prebuilt/` and `prebuilt/releases/<track>/<version>/`
  and logged them in `RELEASES.md`. That flow is retired for new releases; the
  large committed-bin tree can be cleaned up separately (its size lives in git
  history, so shrinking the clone needs a history rewrite — out of scope here).
- The web flasher (meshcomod-client) must read release assets from the GitHub
  Releases API (latest per track) instead of repo paths — tracked as the
  flasher-side follow-up to this change.
