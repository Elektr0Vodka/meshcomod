# Companion CAD runtime toggle

## Problem

The companion firmware hardcodes `MyMesh::getCADEnabled()` to return `true`
(`examples/companion_radio/MyMesh.cpp`), so hardware Channel Activity Detection
before TX is always on with no way to turn it off at runtime. Repeaters, room
servers, and sensors already expose `get cad` / `set cad on|off` through
`CommonCLI` backed by `NodePrefs.cad_enabled`, but the companion has no text CLI
and a separate `NodePrefs` struct, so it never got the toggle.

## Goal

Make CAD a persisted, runtime-toggleable setting on the companion, reachable
both from apps (binary frame protocol) and on-device (UI menu), defaulting to
on so current behavior is preserved. Document the frame protocol addition so
third-party companion apps can add the toggle.

## Non-goals

- Changing the repeater/room/sensor CAD path (already works).
- Adding a general text CLI to the companion.
- Shipping the mobile app UI control. This spec covers firmware only; the
  official app and third-party apps consume the documented frame fields.

## Delivery

Two phases. Phase 1 lands first and is independently useful.

- Phase 1 (frame protocol first): persisted pref, `getCADEnabled()` reads it,
  live apply, frame get/set, docs.
- Phase 2: on-device UI toggle across all four UI variants.

## Design

### Persisted preference (phase 1)

- Add `uint8_t cad_enabled = 1;` to `examples/companion_radio/NodePrefs.h`.
- Persist in `examples/companion_radio/DataStore.cpp` by appending one byte
  after `default_scope_key` in both `savePrefs` and `loadPrefsInt`.
- Appending at the end keeps existing saved-prefs files readable. A short read
  (old file with no CAD byte) leaves the default value 1 in place, so
  upgraders keep CAD on and gain the ability to turn it off.
- Initialize `cad_enabled = 1` in the fresh-boot defaults path alongside the
  other prefs defaults.

### Apply the preference (phase 1)

- `MyMesh::getCADEnabled()` returns `_prefs.cad_enabled` instead of `true`.
- On change, apply live without a reboot by calling the radio wrapper's
  `setCADEnabled()` (the same call `Dispatcher` makes at init via
  `_radio->setCADEnabled(getCADEnabled())`). Confirm the exact accessor from
  `MyMesh` during implementation.
- Remove the stale `// disabled by default, until configurable` comment.

### App control surface: tuning-params frames (phase 1)

The companion is driven by binary frames. Wire CAD into the existing
tuning-params get/set pair, both backward-compatibly.

- `CMD_SET_TUNING_PARAMS`: read an optional trailing byte, length gated, for
  cad on/off. Apps that send only rx + af are unaffected. Save prefs and apply
  live.
- `CMD_GET_TUNING_PARAMS` / `RESP_CODE_TUNING_PARAMS`: append a trailing cad
  byte. Old apps read rx + af and ignore the extra byte.

Rationale for tuning-params over `CMD_SET_OTHER_PARAMS`: the "other params"
group is read back through `RESP_CODE_SELF_INFO`, which ends in an unbounded
node-name field, so there is no clean place for a trailing byte without
shifting existing fields and breaking current app parsers.

### On-device UI toggle (phase 2)

- Add a CAD on/off item to the settings menu in all four UI variants:
  `ui-touch`, `ui-tiny`, `ui-orig`, `ui-new`.
- The item writes `_prefs.cad_enabled`, saves prefs, and applies live, matching
  the pattern used by existing boolean toggles (for example GPS enabled).

### Documentation (phase 1)

- Document the new CAD field in the companion frame-protocol reference so
  third-party app authors (for example Remote-Terminal-for-MeshCore) can add
  the toggle. Confirm the exact doc file during planning.

## Backward compatibility

- Existing prefs files load unchanged. Missing CAD byte defaults to on.
- Existing apps keep working. Optional set byte and appended get byte are
  ignored by older clients.
- Default on preserves the behavior shipped by commit
  "enabled CAD for companion by default".

## Testing

- Prefs round-trip: save then load preserves cad_enabled both on and off.
- Old-file load: a prefs file without the CAD byte loads with cad_enabled = 1.
- Frame set: sending `CMD_SET_TUNING_PARAMS` with and without the optional byte
  behaves correctly and toggles the radio live.
- Frame get: `CMD_GET_TUNING_PARAMS` returns the appended byte; a legacy-length
  parser still reads rx + af.
- Manual on-device: UI toggle flips CAD and persists across reboot (phase 2).
- Build check: companion firmware compiles for at least one representative env.
