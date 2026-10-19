# Companion settings module

Candidate 1 of the companion architecture review (19 Oct 2026). Status: design,
not built.

## Why

Every field of the companion `NodePrefs` struct (`examples/companion_radio/NodePrefs.h`)
is written from many places. Each writer decides on its own whether to validate the
value, apply it to the hardware and save it:

- 11 app frames in `MyMesh::handleCmdFrame`
- 3 text CLIs (`handleCommand` / `handleRadioCommand`, `handleMeshcomodCommand`,
  `checkCLIRescueCmd`)
- the backup import (`MyMesh::uiImportBackup`)
- 4 UIs (ui-new, ui-orig, ui-tiny, ui-touch), which write fields through a raw
  `NodePrefs*` and then call `savePrefs()`
- defaults in the `MyMesh` constructor (MyMesh.cpp ~2498-2516), clamps after load
  (~2616-2628) and defaults inside `DataStore::loadPrefsInt`

That spread has caused real bugs:

- `get cad` / `set cad` answered "unsupported" for a month after CAD became a setting.
- The touch UI's buzzer, GPS and RX-boost toggles each forgot to apply.
- The same field has different bounds per path. For example, freq is 300-2500 MHz
  from the app and 150-2500 MHz from the CLI, with no check at all from meshcomod
  `set`, backup import or touch. Boot then clamps it to 400-2500, so an accepted value
  silently changes on the next reboot.
- Radio changes apply immediately from the app but only after a reboot from both
  text CLIs.

Adding one setting cost 8 code files (CAD). The goal is one row instead.

## Goals

1. One module, `CompanionSettings`, owns each setting's default, valid range,
   persistence and apply step.
2. Every writer goes through it. Afterwards nothing outside the module writes a
   `NodePrefs` field, and the compiler enforces that (callers only get a
   `const NodePrefs*`).
3. A setting changed from any path behaves the same: same validation, same apply,
   one save.
4. **Radio changes apply immediately on every local path** (freq, bw, sf, cr, tx
   power, RX boost, CAD), the way the app frames do today: app frames, the app's CLI
   (`CMD_RUN_CLI_COMMAND`), USB serial, the meshcomod CLI, backup import and the UIs.
   *Decided by the user.*
   **Exception: the remote CLI over the mesh** (a contact with remote-CLI permission,
   `sender_timestamp != 0` in `handleCommand`). There, radio changes (freq, bw, sf, cr)
   are saved and apply after a reboot, like repeaters, and the reply stays
   "OK - reboot to apply". Otherwise the node would switch frequency before its reply
   goes out and the admin would lose it. *Decided by the user.* Settings that don't
   move the node off its channel (tx power, CAD, RX boost) apply live remotely too
   (my call).
5. Adding a setting means adding one table row plus, if it needs one, an apply hook.
   CLI `get`/`set` come from the row. The backup export keeps today's shape (see
   "Backup import and export").
6. The validate/apply/save logic runs in the native test env.

## Non-goals

- No change to the app protocol bytes. Every frame in the table under
  "Protocol frames" keeps its request and reply layout.
- No change to the `/new_prefs` file format, beyond appending the optional
  `vibe_quiet` byte (see "My calls").
- Not the NVS-backed settings (Wi-Fi in `WifiRuntimeStore`, SNMP, the touch UI's
  `TouchPrefsStore`, the receive filter's `/cfilter.json`). They get a second phase
  behind the same interface, with its own design.
- No UI redesign. UIs keep their pages and texts and only change how they read and
  write values.
- Not the server roles' `CommonCLI` / `NodePrefs` class (repeater, room, sensor).
- Not a JSON prefs file like upstream 1.18's `/prefs.json`. The module hides the
  format, so that can come later without touching callers.

## Anti-goals (a failure even if it works)

- A value is changed silently. A clamp or rejection must show in the reply wherever
  the reply can carry it: CLI text, UI toast, or `ERR 6` on frames that already have
  an error reply. Two frames clamp silently today (SET_AUTOADD_CONFIG max_hops > 64,
  SET_CUSTOM_VAR gps_interval > 86400). Their OK-only replies can't report it, so
  they keep that behaviour.
- A stored value changes at boot because the new rules are stricter. See "Load-time
  sanitise".
- A prefs file written by an older build fails to load, or a downgrade to an older
  build loses settings.
- Flash use grows enough to matter on nRF52 (Wio Tracker L1 BLE has about 39% free).
  Budget: 3 KB per env. Measure and report it.
- App replies change, except in two cases: the CLI texts that change because of goal 4
  ("OK - reboot to apply" becomes "OK" for local callers), and the new rejections
  listed under "My calls".
  Each of those is listed in the PR that makes it.

## Constraints

- `MyMesh.cpp`, `DataStore.cpp` and the UIs are meshcomod-owned. This diff will
  conflict with upstream syncs, so keep the call sites thin and the logic in new,
  fork-owned files.
- `node_lat` / `node_lon` are stored in `/new_prefs` but owned by `sensors`
  (`MyMesh::savePrefs` writes both). GPS fixes write them directly in shared code
  (`EnvironmentSensorManager.cpp`, several variant `target.cpp` files, the touch UI's
  `updateGpsLocation`). **`sensors` stays the owner** (my call). The module's `Lat` and
  `Lon` rows are a validated write front end for the app frame, the touch position
  page and backup import: they write `sensors.node_lat/lon` through a pointer bound
  at `begin()`. `PrefsStore::save` reads them from there. GPS fixes keep writing
  directly and are not "setting changes".
- Some apply steps only exist on some builds. RX boost exists on SX1262/1268 (and
  `SX126X_RX_BOOSTED_GAIN`), GPS only with `ENV_INCLUDE_GPS`, the buzzer only with
  `PIN_BUZZER`, vibration only with `HAS_DRV2605`. The table row exists on every
  build; the apply hook reports "unsupported" where the hardware is missing.
- The BLE pin and the BLE/host name are read once at boot by the transports
  (`main.cpp`: 336, 470, 482, 496, 549). They can't apply live without restarting
  BLE, so their apply step is "reboot required", and the reply says so.
- MyMesh reads `_prefs.x` in about 15 places on the hot path (airtime, rx delay,
  multi acks, path hash, and others). Reads must stay a plain field access.

## Design

### Shape

New folder `examples/companion_radio/settings/`:

- `SettingsTable.h/.cpp`: the rows. Pure data plus pure parse/format/validate
  functions, with no Arduino includes, so it builds in the native env.
- `CompanionSettings.h/.cpp`: the module. It owns the `NodePrefs` instance (moved
  out of `MyMesh::_prefs`) plus the lat/lon pair, and talks to the rest through two
  ports.
- `SettingsPorts.h`: the two port interfaces.

### Interface (what callers see)

```cpp
enum class SettingId : uint8_t { NodeName, Freq, Bw, Sf, Cr, TxPower, AirtimeFactor,
  RxDelayBase, MultiAcks, ManualAddContacts, TelemetryBase, TelemetryLoc, TelemetryEnv,
  AdvertLocPolicy, ClientRepeat, PathHashMode, BlePin, BuzzerQuiet, VibeQuiet, GpsEnabled,
  GpsInterval, AutoaddConfig, AutoaddMaxHops, RxBoostedGain, CadEnabled, DefaultScope,
  Lat, Lon, Count };

struct SetResult {
  enum Code : uint8_t { Ok, OkReboot, Clamped, BadValue, Unsupported } code;
  // Clamped: stored a different value than asked; the reply must say so.
};

class CompanionSettings {
public:
  const NodePrefs& prefs() const;           // plain field reads, hot path
  double lat() const; double lon() const;

  SetResult setInt(SettingId, int32_t);
  SetResult setFloat(SettingId, float);
  SetResult setText(SettingId, const char*);

  // Text front end for every CLI: parses, validates, applies, saves, and
  // writes the reply ("OK", "OK - reboot to apply", "Error, must be -9 to 20" ...).
  // Origin::Remote saves radio params (freq, bw, sf, cr) without applying them.
  enum class Origin : uint8_t { Local, Remote };
  bool setByKey(const char* key, const char* value, Origin origin,
                char* reply, size_t reply_len);
  bool getByKey(const char* key, char* reply, size_t reply_len);   // "> value"

  const SettingInfo& describe(SettingId) const;   // key, label, type, min, max, unit

  // Several fields, one validation pass, one apply, one save. All or nothing.
  class Edit { /* setInt/setFloat/setText, then commit() -> SetResult */ };
  Edit edit();

  // Persistence and lifecycle
  void loadDefaults();       // table defaults (replaces the MyMesh ctor block)
  bool load();               // through PrefsStore, then sanitise with the table ranges
  void applyAll();           // at boot: radio, tx, RX boost, CAD, GPS, buzzer
};
```

`MyMesh::getNodePrefs()` becomes `const NodePrefs*`. The UIs' `_node_prefs`
becomes `const NodePrefs*`. Every remaining write is then a compile error, which
replaces any grep check.

### The table

One row per `SettingId`:

| column | meaning |
|---|---|
| key | CLI name (`"freq"`, `"cad"`, `"radio.rxgain"`), matching today's CLI keys |
| label | short UI label |
| type | bool, u8, i8, u32, float, text, pin, scope |
| location | `offsetof(NodePrefs, field)` and size, or `lat`/`lon` |
| default | value, or a function for the radio-dependent ones (`rx_boosted_gain`) |
| min, max | range, plus an optional custom validator (`ble_pin`: 0 or 100000-999999; `node_name`: `AdvertDataParser::isValidName`, 1-31 chars; `client_repeat`: `isValidClientRepeatFreq`) |
| on_out_of_range | reject or clamp (clamp only where a path clamps today, e.g. `autoadd_max_hops` > 64) |
| apply | none, radio, tx, rxboost, cad, gps, buzzer, vibe, reboot |
| cli | listed in `get`/`set` (some, like `default_scope`, stay frame-only) |

### Ports

```cpp
struct SettingsApplier {          // MyMesh implements it in production, a fake in tests
  enum Kind : uint8_t { Radio, TxPower, RxBoost, Cad, Gps };
  virtual bool supports(Kind) const = 0;      // checked during validation
  virtual bool applyRadio(float freq, float bw, uint8_t sf, uint8_t cr) = 0;
  virtual bool applyTxPower(int8_t dbm) = 0;
  virtual bool applyRxBoost(bool on) = 0;     // false = unsupported on this radio
  virtual bool applyCad(bool on) = 0;
  virtual bool applyGps(bool on, uint32_t interval_s) = 0;   // false = no GPS
  virtual void onChanged(SettingId) = 0;      // UI hook: buzzer, vibe, redraw
};
struct PrefsStore {               // DataStore implements it, MemStream fake in tests
  virtual bool load(NodePrefs&, double& lat, double& lon) = 0;
  virtual bool save(const NodePrefs&, double lat, double lon) = 0;
};
```

The buzzer and vibration motor belong to the UI task, so their apply goes through
`onChanged()`. `AbstractUITask` gets `virtual void onSettingChanged(SettingId) {}`.
Each UI applies `buzzer.quiet()` / `vibration.quiet()` there, which also fixes the
touch UI that never applied the buzzer.

### Behaviour rules

1. **Validate, apply, then save.** If apply reports unsupported, the value is not
   saved and the reply says "unsupported". That fixes `set radio.rxgain` saving on
   failure. If apply reports failure for a field that does not need the hardware
   (none today), it is still saved.
2. **One save per change.** An `Edit` with several fields saves once. That fixes the
   touch double saves.
3. **Same reply text for the same setting on every CLI**, taken from the row. Where
   two CLIs replied differently today, the `handleCommand` text wins, because that is
   what app v1.50 shows. The meshcomod `set` keeps its own command names
   (`set freq` etc.) but routes to the same rows.
4. **Radio is live on every local path.** `set radio`, meshcomod `set freq|bw|sf|cr|tx`,
   the touch radio page and backup import all apply through `applyRadio` /
   `applyTxPower`. "OK - reboot to apply" becomes "OK" for those. From a remote
   caller (`Origin::Remote`), freq, bw, sf and cr are saved only and the reply stays
   "OK - reboot to apply". `handleCommand` already knows the origin
   (`sender_timestamp != 0`) and passes it on.
5. **`Edit` is all or nothing at validation, not at apply** (my call). `commit()`
   first checks every staged field: range, validator, and whether the board supports
   the apply (`SettingsApplier::supports(kind)`, e.g. RX boost on a non-SX126x radio, or
   GPS when none was detected). If any field fails, nothing is stored, applied or
   saved. Then it applies **only the fields whose value changed**, in a fixed order
   (radio, tx, RX boost, CAD, GPS), and saves once. So the touch "experimental" page
   on a board without RX boost can still change multi_acks, as long as the RX boost
   value itself is not changed.

### Load-time sanitise

My call. At boot the module only applies the ranges boot already enforces today
(MyMesh.cpp ~2616-2628), widened where this design widens them: freq becomes 150-2500
instead of 400-2500. It never applies a rule that is stricter than today's boot clamp.
A stored `ble_pin` of 1234, a name with characters `isValidName` rejects, or a
`multi_acks` raw byte of 5 load as they are, and the stricter rule only applies to the
next `set`. No stored value is reset to a default at boot unless today's code already
does it (the legacy-layout defaults).

### Binary frames and new rules

My call. Frames that already have an `ERR` reply (SET_RADIO_PARAMS, SET_RADIO_TX_POWER,
SET_PATH_HASH_MODE, SET_DEVICE_PIN, SET_CUSTOM_VAR, SET_DEFAULT_FLOOD_SCOPE,
SET_ADVERT_LATLON) answer `ERR 6` for a value the row rejects.
SET_TUNING_PARAMS and SET_OTHER_PARAMS gain `ERR 6` for out-of-range bytes. Stock apps
never send those, and an app that does is told rather than silently stored.

**SET_ADVERT_NAME keeps today's acceptance**, so stock-app compatibility doesn't
depend on `isValidName`. The frame truncates to 31 bytes and accepts any characters,
as now. Only an empty name is rejected, with `ERR 6`. The stricter name rule applies
to the CLI and the UIs. The two silent clamps (autoadd max_hops, gps_interval) stay,
as noted under anti-goals.

### Backup import and export

My call. The export keeps today's JSON shape exactly (it matches the stock app). The
seven fields it writes carry their JSON key in their rows, and no other row is
exported. Import goes through one `Edit` per section (name, radio, position). A value
the row rejects (e.g. tx 22 dBm on a board whose maximum is 20, or bw 7.0) is skipped,
and the rest of the section is imported. `uiImportBackup` gains an optional
`int* out_skipped` count, and the touch UI's import toast shows it ("Imported, 1
setting skipped").

### Persistence

- `DataStore::savePrefs/loadPrefs` stay as they are and become the `PrefsStore`
  adapter. The layout is unchanged, plus one appended optional byte for `vibe_quiet`
  at offset 141. An older build ignores it, and a newer build reading an older file
  defaults it to 0.
- The legacy layouts (`/node_prefs`, the 13- and 14-byte tails) keep loading as
  today.
- Fix the stale offset comments in `DataStore.cpp` while touching it.

## My calls (the user can override any of these)

These are my decisions, made where the paths disagree today. None of them came from
the user.

| field | today | unified rule |
|---|---|---|
| freq | app 300-2500, CLI 150-2500, boot clamp 400-2500, others unchecked | 150-2500 MHz everywhere (upstream 1.18's lower bound). Boot no longer raises 150-400 values |
| bw | app/CLI >= 7.0, boot >= 7.8 | 7.8-500 kHz |
| airtime_factor | `set af` 0-9, `set dutycycle 1` gives 99, tuning frame unchecked, boot 0-9 | 0-9. `set dutycycle` below 10% is rejected with "Error, dutycycle must be 10-100" |
| tx_power_dbm | -9..MAX on most paths, unchecked on meshcomod `set`, import and touch presets (22 dBm) | -9..MAX_LORA_TX_POWER, rejected outside. Touch presets above MAX are clamped, and the toast says so |
| ble_pin | app: 0 or 6 digits; CLI and rescue: any number | 0 or 100000-999999 everywhere |
| node_name | 30 or 31 chars, characters checked only by the CLI | 1-31 chars, `isValidName` everywhere |
| client_repeat | app checks `isValidClientRepeatFreq`, touch does not | checked everywhere (the check runs against the current or edited freq) |
| multi_acks, advert_loc, telemetry modes | raw bytes from the app, 0/1 from touch | 0-1 for multi_acks and advert_loc; 0-2 per telemetry mode |
| gps_interval | applied only via SET_CUSTOM_VAR, never at boot | applied in `applyAll()` at boot together with gps_enabled, on every UI |
| vibe_quiet | used by ui-orig, missing from NodePrefs, so MeshTracker X1 companion envs don't build | added as an appended field |
| default_scope | touch writes the key but not the name, so GET_DEFAULT_FLOOD_SCOPE answers "none" | one `DefaultScope` setting that always writes name and key together |

Not changed, but noted: the touch UI's region presets store airtime as `pct/100`, while
the CLI uses `100/dc - 1`. That looks like a unit bug in the presets. I'm leaving it for
a separate fix, so this refactor doesn't change what a preset does.

## Rollout

Three PRs. Each one builds, ships and can be reverted on its own.

1. **Module and tests.** `settings/` folder, the table, `CompanionSettings` with both
   ports, the native tests, and `DataStore` as the `PrefsStore` adapter. MyMesh owns a
   `CompanionSettings` and uses it for defaults, load, sanitise and `applyAll()`. No
   writer moves yet. Behaviour change: only the unified load-time ranges.
2. **App frames and CLIs.** All 11 setting frames, `handleCommand` /
   `handleRadioCommand`, `handleMeshcomodCommand` `set`, the rescue CLI `set pin`, and
   backup import go through the module. This is where radio-applies-live and the
   unified bounds take effect. The CLI `get`/`set` code for those keys collapses into
   `getByKey` / `setByKey`.
3. **UIs.** All four UIs write through the module. `getNodePrefs()` and `_node_prefs`
   become const. `onSettingChanged` replaces the per-UI apply code. The
   `vibe_quiet` field lands here.

## Testing

- **Native (`pio test -e native`)**, new `test/test_companion_settings`:
  - every row's default, range edges and clamp/reject behaviour;
  - `setByKey` / `getByKey` reply texts, as golden strings;
  - `Origin::Remote` `set radio` saves without calling `applyRadio` and replies
    "OK - reboot to apply", while remote `set tx` still applies;
  - `Edit`: a validation failure stores nothing; only changed fields are applied;
    an unsupported apply on a changed field rejects the edit;
  - load-time sanitise never applies a stricter rule (ble_pin 1234 survives a load);
  - one save per change, counted with the fake store;
  - apply calls and their order, with the fake applier;
  - unsupported apply leaves the value unsaved;
  - load and save round trip through a MemStream copy of every legacy layout
    (a 91-byte file with a 13-byte tail, the 14-byte tail, a full 141-byte file, a
    file without the CAD byte), including a 142-byte file read by today's loader.
- **Builds**: the PR check matrix plus one env per UI (ui-orig: a MeshTracker X1
  companion, which should build again after PR 3; ui-tiny: T-Echo Card).
- **Flash**: report the size delta per PR for V4 usb_tcp, Wio Tracker L1 BLE and
  T-Echo Card.
- **Hardware** (V4 and V3, the boards that are tested): set radio from the app, the
  CLI and the device, check it applies without a reboot and survives one; flash an
  older build afterwards and check the settings load.

## Left to the builder

- Internal layout of the table (one `constexpr` array, X-macro, or generated), as
  long as one row per setting stays the only place to edit.
- How `Edit` stages values (a copy of `NodePrefs` is fine; it is 141 bytes).
- Whether `CompanionSettings` lives inside `MyMesh` or next to it in `main.cpp`, as
  long as callers reach it through one accessor.
- Exact error texts for new rejections, following the "Error, must be X to Y" style
  of `handleRadioCommand`.

## Protocol frames (must stay byte-identical)

All little-endian. `OK = [0]`, `ERR = [1, code]` (`ERR_CODE_ILLEGAL_ARG` = 6).

| CMD | request | reply |
|---|---|---|
| SET_ADVERT_NAME 8 | `[8, name...]` (no NUL) | OK |
| SET_ADVERT_LATLON 14 | `[14, lat i32 e6, lon i32 e6, (alt i32, ignored)]` | OK/ERR |
| SET_RADIO_PARAMS 11 | `[11, freq u32 kHz, bw u32 Hz, sf, cr, (repeat u8)]` | OK/ERR |
| SET_RADIO_TX_POWER 12 | `[12, i8 dBm]` | OK/ERR |
| SET_TUNING_PARAMS 21 | `[21, rx_delay u32 x1000, af u32 x1000, (cad u8)]` | OK (ERR after this change if out of range) |
| GET_TUNING_PARAMS 43 | `[43]` | `[23, rx u32, af u32, cad u8]` |
| SET_OTHER_PARAMS 38 | `[38, manual_add, (telem env<<4 \| loc<<2 \| base), (advert_loc), (multi_acks)]` | OK (ERR if out of range) |
| SET_PATH_HASH_MODE 61 | `[61, 0, mode 0..2]` | OK/ERR |
| SET_DEVICE_PIN 37 | `[37, pin u32]` | OK/ERR |
| SET_CUSTOM_VAR 41 | `[41, "name:value"]` | OK/ERR |
| SET_AUTOADD_CONFIG 58 | `[58, config, (max_hops)]` | OK |
| SET_DEFAULT_FLOOD_SCOPE 63 | `[63, name[31], key[16]]`, or `[63]` to clear | OK/ERR |
| GET_DEFAULT_FLOOD_SCOPE 64 | `[64]` | `[28, name[31], key[16]]` or `[28]` |
| DEVICE_QUERY 22 | `[22, ver]` | unchanged; reads `ble_pin`, `client_repeat`, `path_hash_mode` |
| APP_START 1 | `[1, ...]` | unchanged; reads tx, multi_acks, advert_loc, telemetry, manual_add, radio params, name |

The only frame-level change: SET_TUNING_PARAMS and SET_OTHER_PARAMS can now answer
`ERR 6` for out-of-range bytes, where they answered OK and stored the raw value.
