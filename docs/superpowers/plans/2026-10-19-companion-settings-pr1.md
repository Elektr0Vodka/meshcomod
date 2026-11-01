# Companion Settings, PR 1 (module, table, ports, tests, DataStore adapter) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the `CompanionSettings` module (table, ports, `/new_prefs` codec) with native tests, and make MyMesh use it for defaults, load, load-time sanitise, save and boot apply. No setting writer moves yet.

**Architecture:** Pure, Arduino-free code in new top-level files of `examples/companion_radio/`: a byte codec for `/new_prefs`, a table with one row per setting, and the module that validates, applies (through `SettingsApplier`) and saves (through `PrefsStore`). DataStore keeps the file I/O and calls the codec. MyMesh owns the module, implements `SettingsApplier`, and keeps its `_prefs` name as a reference into the module, so the ~200 existing reads and the not-yet-migrated writers compile unchanged.

**Tech Stack:** C++17 (native env), Arduino/PlatformIO firmware (ESP32, nRF52), googletest 1.17 via `pio test -e native`.

**Spec:** [`docs/superpowers/specs/2026-10-19-companion-settings-design.md`](../specs/2026-10-19-companion-settings-design.md)

## Global Constraints

- Branch `Feat/companion-settings-module` from `origin/main`. One PR on `origin` (Elektr0Vodka/meshcomod), never `upstream`.
- AGENTS.md: no em-dashes or en-dashes anywhere; conventional-commit prefixes; no `Co-Authored-By` or "Generated with" lines; PR text covers what, why, how tested, firmware impact.
- `changelog_ev.md`: one entry under **Unreleased** tagged with the PR number.
- `/new_prefs` layout is unchanged in this PR: 141 bytes, offsets as listed in Task 1. No `vibe_quiet` byte yet (that is PR 3).
- **File location (plan decision, deviates from the spec's `settings/` folder):** the new files live directly in `examples/companion_radio/` (`PrefsCodec`, `SettingsTable`, `SettingsPorts`, `CompanionSettings`, `DataStorePrefsStore`), next to `CompanionFilter.cpp`. Every companion env compiles `+<../examples/companion_radio/*.cpp>`, which is not recursive (89 variant files), so a subfolder would mean editing every upstream-owned env block.
- The new files include no Arduino, FS or radio headers (only `<stdint.h>`, `<stddef.h>`, `<string.h>`, `<math.h>`, `<stdio.h>`, `"NodePrefs.h"`, `<helpers/TxtDataHelpers.h>`), except `DataStorePrefsStore.h`.
- **`applyAll()` in PR 1 applies radio, tx, RX boost and CAD only (plan decision).** The spec's boot apply of GPS and buzzer moves to PR 3 together with the UIs' `onSettingChanged`, so PR 1 keeps its one-behaviour-change promise. On non-touch builds, applying GPS at boot is a behaviour change.
- PR 1 changes one behaviour only: load-time sanitise uses the table ranges. That means freq loads within 150-2500 MHz, where it was 400-2500 before. A NaN or infinite float loads as its default, and `node_name` is always NUL-terminated. All app frames, CLI replies and UI texts are byte-identical.
- Flash budget: at most 3 KB more per env. Report the delta for `heltec_v4_companion_radio_usb_tcp`, `WioTrackerL1_companion_radio_ble` and `LilyGo_T-Echo_Card_companion_radio_ble`.
- Never run two `pio` commands in the same project directory at once: a flag change wipes `.pio/build`. Build with `FIRMWARE_VERSION=v0.0.0.0-probe DISABLE_DEBUG=1 sh build.sh build-firmware <env>`.

## Review Focus

1. **A 137-byte prefs file** written by the old buggy `savePrefs` that skipped `gps_interval`: it must decode exactly as today's loader does. A user upgrading from that build must not get different values than they get now. Test: `PrefsCodec.Decode137ByteFileMatchesLegacyLoader` (Task 1).
2. **A prefs file longer than 141 bytes**, written by a later build with the PR 3 `vibe_quiet` byte: the extra bytes are ignored and everything else loads. Test: `PrefsCodec.ExtraTrailingBytesIgnored` (Task 1).
3. **A corrupt float in the file** (NaN or infinite freq, bw, airtime factor or rx delay): today `constrain` keeps NaN and the radio misbehaves. It must load as the row default. Test: `CompanionSettingsLoad.NanFloatsLoadAsDefaults` (Task 4).
4. **A 32-byte `node_name` with no NUL** in the file: today only `main.cpp:469` patches it. It must load NUL-terminated. Test: `PrefsCodec.NameAlwaysTerminated` (Task 1).
5. **CLI values with trailing whitespace or CR/LF** from USB serial or an app (`"20\r\n"`, `"on "`): they must parse as the trimmed value. Test: `SettingsText.TrailingWhitespaceTrimmed` (Task 5).

---

### Task 1: `/new_prefs` byte codec

**Files:**
- Create: `examples/companion_radio/PrefsCodec.h`, `examples/companion_radio/PrefsCodec.cpp`
- Create: `test/test_companion_settings/test_prefs_codec.cpp`
- Modify: `platformio.ini` `[env:native]` `build_src_filter` (around line 183): add `+<../examples/companion_radio/PrefsCodec.cpp>`

**Interfaces:**
- Produces:
  - `static const size_t PREFS_FILE_SIZE = 141;` and `static const size_t PREFS_FILE_MAX = 192;`
  - `struct PrefsDecodeResult { bool needs_resave; };`. It is true only for the 13-byte legacy meshcomod tail.
  - `PrefsDecodeResult decodePrefs(const uint8_t* buf, size_t len, NodePrefs& p, double& lat, double& lon, uint8_t rx_boost_default);`
  - `size_t encodePrefs(const NodePrefs& p, double lat, double lon, uint8_t* out, size_t cap);`. It returns 141, or 0 when `cap < 141`.

**Decode semantics.** The decoder reproduces `DataStore::loadPrefsInt` (DataStore.cpp:261-309) exactly. Use one cursor with a `take(dst, n)` helper that copies `min(n, remaining)` bytes and advances, which mirrors `File::read` on a short file.

1. **Header:** read the fields at 0-77 in today's order. The 4 pad bytes at 36 are skipped.
2. **Tail:** read the tail with `take` into a 15-byte buffer. Call the number of bytes actually read `n`, then:
   - `n == 13`: legacy layout, the same field mapping as `load_prefs_tail_meshcomod_legacy`. `rx_boosted_gain` gets `rx_boost_default`, and `needs_resave` is true.
   - `n >= 14`: upstream layout, as `load_prefs_tail_upstream`. When `n == 14`, `rx_boosted_gain` gets `rx_boost_default`; when `n == 15`, it is `tail[14] ? 1 : 0`.
   - `n < 13`: leave the tail fields untouched.
3. **Scope:** zero `default_scope_name` and `default_scope_key`, then take 31 and 16 bytes for them.
4. **CAD:** set `cad_enabled = 1`, then if one more byte is available, store `byte ? 1 : 0`.
5. **Name:** set `node_name[31] = 0`.
6. Ignore any bytes after offset 140.

**Encode layout** (offset: field). All values are little-endian, raw `memcpy` as today.

| offset | field |
|---|---|
| 0 | airtime_factor (f32) |
| 4 | node_name[32] |
| 36 | 4 zero bytes |
| 40 | lat (f64) |
| 48 | lon (f64) |
| 56 | freq (f32) |
| 60 | sf |
| 61 | cr |
| 62 | client_repeat |
| 63 | manual_add_contacts |
| 64 | bw (f32) |
| 68 | tx_power_dbm |
| 69 | telemetry_mode_base |
| 70 | telemetry_mode_loc |
| 71 | telemetry_mode_env |
| 72 | rx_delay_base (f32) |
| 76 | advert_loc_policy |
| 77 | multi_acks |
| 78 | path_hash_mode |
| 79 | 0 |
| 80 | ble_pin (u32) |
| 84 | buzzer_quiet |
| 85 | gps_enabled |
| 86 | gps_interval (u32) |
| 90 | autoadd_config |
| 91 | autoadd_max_hops |
| 92 | rx_boosted_gain |
| 93 | default_scope_name[31] |
| 124 | default_scope_key[16] |
| 140 | cad_enabled |

- [ ] **Step 1: Write the failing tests** in `test/test_companion_settings/test_prefs_codec.cpp`.

  The test file contains `legacySave()` and `legacyLoad()`. They are verbatim copies of today's `DataStore::savePrefs` / `loadPrefsInt` / `load_prefs_tail_*`, rewritten over a `std::vector<uint8_t>` and a read cursor in place of `File`. They act as the reference implementation. Use a helper `NodePrefs sample()` that fills every field with distinct non-zero values, with `node_name = "Alpha"`, `freq = 869.525f`, `ble_pin = 123456` and `cad_enabled = 0`.

  ```cpp
  TEST(PrefsCodec, EncodeMatchesLegacySaveByteForByte)
    // encodePrefs(sample(), 52.1, 4.3, buf, sizeof buf) == 141 and memcmp == legacySave(...)
  TEST(PrefsCodec, EncodeOffsets)
    // buf[60] == sample().sf; memcmp(&buf[80], &pin, 4) == 0; buf[79] == 0; buf[140] == 0
  TEST(PrefsCodec, EncodeRejectsSmallBuffer)        // encodePrefs(..., buf, 140) == 0
  TEST(PrefsCodec, RoundTripFull141)                // decode(encode(x)) == x field by field, needs_resave false
  TEST(PrefsCodec, Legacy91ByteFileMigrates)        // 78 + 13-byte tail: needs_resave true, rx_boosted_gain == rx_boost_default,
                                                    // scope zeroed, cad_enabled == 1, autoadd_max_hops == tail[1]
  TEST(PrefsCodec, Upstream92ByteFileDefaultsRxBoost) // 78 + 14: rx_boosted_gain == rx_boost_default, autoadd_max_hops == tail[13]
  TEST(PrefsCodec, FileWithoutCadByteDefaultsOn)    // first 140 bytes: cad_enabled == 1
  TEST(PrefsCodec, ExtraTrailingBytesIgnored)       // 141 bytes + {0x01, 0xFF}: same result as the 141-byte file
  TEST(PrefsCodec, Decode137ByteFileMatchesLegacyLoader) // legacySave without gps_interval: decodePrefs == legacyLoad, field by field
  TEST(PrefsCodec, EmptyBufferLeavesPrefsUntouched) // len 0: every field keeps its pre-call value, needs_resave false
  TEST(PrefsCodec, NameAlwaysTerminated)            // 32 'A' bytes at offset 4: node_name[31] == 0
  ```

- [ ] **Step 2: Run the tests to verify they fail.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: build fails, because `PrefsCodec.h` is not found.

- [ ] **Step 3: Implement `decodePrefs` and `encodePrefs`** in `PrefsCodec.cpp`, with the semantics and table above. Add the native `build_src_filter` line.

- [ ] **Step 4: Run the tests to verify they pass.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: 11 tests PASS.

- [ ] **Step 5: Commit**
  ```bash
  git add examples/companion_radio/PrefsCodec.* test/test_companion_settings/test_prefs_codec.cpp platformio.ini
  git commit -m "feat(companion): add a byte codec for the /new_prefs layout"
  ```

### Task 2: DataStore reads and writes through the codec

**Files:**
- Modify: `examples/companion_radio/DataStore.cpp:216-354` (`default_rx_boosted_gain_pref`, the `load_prefs_tail_*` helpers, `loadPrefsInt`, `savePrefs`)

**Interfaces:**
- Consumes: `decodePrefs`, `encodePrefs`, `PREFS_FILE_MAX` (Task 1).
- Produces: unchanged public `DataStore::loadPrefs(NodePrefs&, double&, double&)` and `DataStore::savePrefs(const NodePrefs&, double, double)`.

- [ ] **Step 1: Rewrite `loadPrefsInt` and `savePrefs`.**
  - `loadPrefsInt` reads the file with one `file.read(buf, PREFS_FILE_MAX)` into `uint8_t buf[PREFS_FILE_MAX]`, then calls `decodePrefs(buf, n, ...)` with the value from `default_rx_boosted_gain_pref`. It calls `savePrefs` when `needs_resave` is set.
  - `savePrefs` calls `encodePrefs` into a 141-byte buffer and writes it with one `file.write`.
  - Delete the two `load_prefs_tail_*` helpers, which now live in the codec. Keep `default_rx_boosted_gain_pref` in DataStore, because it depends on radio macros.

- [ ] **Step 2: Build an ESP32 and an nRF52 env.**
  Run: `FIRMWARE_VERSION=v0.0.0.0-probe DISABLE_DEBUG=1 sh build.sh build-firmware heltec_v4_companion_radio_usb_tcp`, then the same for `WioTrackerL1_companion_radio_ble`.
  Expected: both report `SUCCESS`.

- [ ] **Step 3: Run the native tests.**
  Run: `pio test -e native`
  Expected: all suites PASS.

- [ ] **Step 4: Commit**
  ```bash
  git add examples/companion_radio/DataStore.cpp
  git commit -m "refactor(companion): read and write prefs through PrefsCodec"
  ```

### Task 3: The settings table

**Files:**
- Create: `examples/companion_radio/SettingsTable.h`, `examples/companion_radio/SettingsTable.cpp`
- Create: `test/test_companion_settings/test_settings_table.cpp`
- Modify: `platformio.ini` `[env:native]`: add `+<../examples/companion_radio/SettingsTable.cpp>` and `+<../src/helpers/TxtDataHelpers.cpp>`. If it fails to compile natively because `itoa`/`ltoa`/`dtostrf` are missing, add minimal versions to `test/mocks/Arduino.h` and do not change `TxtDataHelpers.cpp`.

**Interfaces:**
- Produces:
  ```cpp
  enum class SettingId : uint8_t { NodeName, Freq, Bw, Sf, Cr, TxPower, AirtimeFactor, RxDelayBase,
    MultiAcks, ManualAddContacts, TelemetryBase, TelemetryLoc, TelemetryEnv, AdvertLocPolicy,
    ClientRepeat, PathHashMode, BlePin, BuzzerQuiet, GpsEnabled, GpsInterval, AutoaddConfig,
    AutoaddMaxHops, RxBoostedGain, CadEnabled, DefaultScope, Lat, Lon, Count };
  enum class SettingType : uint8_t { Bool, U8, I8, U32, Float, Double, Text, Pin, Scope };
  enum class ApplyKind : uint8_t { None, Radio, TxPower, RxBoost, Cad, Gps, Ui, Reboot };
  struct SettingInfo {
    SettingId id; const char* key; const char* label; SettingType type;
    int16_t offset;          // offsetof(NodePrefs, field); -1 for Lat/Lon
    float def, min, max;     // numeric rows; see "build" rows below
    bool clamp;              // true: out-of-range input is clamped (result Clamped); false: rejected
    ApplyKind apply; bool cli; bool sanitize_on_load;
    const char* ok_fmt;      // nullptr = "OK"
    const char* err_fmt;     // nullptr = "Error, must be %s to %s"
    const char* get_fmt;     // nullptr = default "> %s" formatting by type
  };
  struct SettingsBuild {     // build-time values MyMesh fills from its macros; tests fill by hand
    float freq, bw; uint8_t sf, cr; int8_t tx_power, max_tx_power; uint8_t rx_boost_default;
    bool (*name_ok)(const char*);             // AdvertDataParser::isValidName in firmware
    bool (*repeat_freq_ok)(uint32_t freq_khz);   // MyMesh::isValidClientRepeatFreq in firmware
  };
  const SettingInfo& settingInfo(SettingId id);
  const SettingInfo* findSettingByKey(const char* key);   // nullptr when unknown or cli == false
  float settingDefault(const SettingInfo& s, const SettingsBuild& b);
  float settingMax(const SettingInfo& s, const SettingsBuild& b);   // TxPower: b.max_tx_power
  ```

**Rows** (key, type, default, range, clamp, apply, cli, sanitize_on_load, texts). `build.x` means the value comes from `SettingsBuild`. Every other row default is the constructor default at MyMesh.cpp:2498-2516.

| id | key | type | default | range | clamp | apply | cli | load | ok / err / get |
|---|---|---|---|---|---|---|---|---|---|
| NodeName | name | Text | "NONAME" | 1-31 chars, `name_ok` | no | None | yes | no | OK / "Error, bad chars" / "> %s" |
| Freq | freq | Float | build.freq | 150-2500 | no | Radio | yes | yes | get "> " + `StrHelper::ftoa` |
| Bw | bw | Float | build.bw | 7.8-500 | no | Radio | yes | yes | |
| Sf | sf | U8 | build.sf | 5-12 | no | Radio | yes | yes | |
| Cr | cr | U8 | build.cr | 5-8 | no | Radio | yes | yes | |
| TxPower | tx | I8 | build.tx_power | -9..build.max_tx_power | no | TxPower | yes | yes | err "Error, must be -9 to %d" |
| AirtimeFactor | af | Float | 1.0 | 0-9 | no | None | yes | yes | err "ERROR: af must be 0-9", get ftoa |
| RxDelayBase | rxdelay | Float | 0 | 0-20 | no | None | yes | yes | err "Error, must be 0-20", get ftoa |
| MultiAcks | multi.acks | U8 | 0 | 0-1 | no | None | yes | no | get "> %d" |
| ManualAddContacts | manual.add | U8 | 0 | 0-1 | no | None | no | no | |
| TelemetryBase / Loc / Env | telemetry.base / .loc / .env | U8 | 0 | 0-2 | no | None | no | no | |
| AdvertLocPolicy | advert.loc | U8 | 0 | 0-1 | no | None | no | no | |
| ClientRepeat | client.repeat | U8 | 0 | 0-1, and when 1, `repeat_freq_ok(freq kHz)` | no | None | no | no | |
| PathHashMode | path.hash.mode | U8 | 0 | 0-2 | no | None | yes | yes | err "Error, must be 0,1, or 2", get "> %d" |
| BlePin | pin | Pin | 0 | 0 or 100000-999999 | no | Reboot | yes | no | ok "> pin is now %06d" / err "Error, pin must be 0 or 100000-999999" |
| BuzzerQuiet | buzzer.quiet | Bool | 0 | | | Ui | no | no | |
| GpsEnabled | gps | Bool | 0 | 0-1 | no | Gps | no | yes | |
| GpsInterval | gps.interval | U32 | 0 | 0-86400 | yes | Gps | no | yes | |
| AutoaddConfig | autoadd.config | U8 | 0 | 0-255 | no | None | no | no | |
| AutoaddMaxHops | autoadd.max.hops | U8 | 0 | 0-64 | yes | None | no | yes | |
| RxBoostedGain | radio.rxgain | Bool | build.rx_boost_default | | | RxBoost | yes | yes (force 0/1) | get "> on" / "> off" |
| CadEnabled | cad | Bool | 1 | | | Cad | yes | yes (force 0/1) | get "> on" / "> off" |
| DefaultScope | default.scope | Scope | zeros | name 0-30 chars | no | None | no | no | |
| Lat | lat | Double | 0 | -90..90 | no | None | no | no | |
| Lon | lon | Double | 0 | -180..180 | no | None | no | no | |

Bool text input: `on` or `1` means true, `off` or `0` means false, anything else is `BadValue`, reply "Error, must be on or off".

- [ ] **Step 1: Write the failing tests** in `test/test_companion_settings/test_settings_table.cpp`.
  ```cpp
  TEST(SettingsTable, EveryIdHasExactlyOneRow)      // for i < Count: settingInfo(i).id == i
  TEST(SettingsTable, KeysAreUnique)                // no two rows share a key
  TEST(SettingsTable, FindByKeyOnlyCliRows)         // findSettingByKey("cad") != nullptr; ("manual.add") == nullptr; ("nope") == nullptr
  TEST(SettingsTable, BuildDefaults)                // with b.freq = 869.525f: settingDefault(Freq) == 869.525f; settingMax(TxPower) == b.max_tx_power
  TEST(SettingsTable, OffsetsMatchNodePrefs)        // settingInfo(CadEnabled).offset == offsetof(NodePrefs, cad_enabled); Lat offset == -1
  ```

- [ ] **Step 2: Run the tests to verify they fail.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: build fails, because `SettingsTable.h` is not found.

- [ ] **Step 3: Implement the table** as one `static const SettingInfo` array indexed by `SettingId`, with the values above. Add `TxtDataHelpers.cpp` to the native filter.

- [ ] **Step 4: Run the tests to verify they pass.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: all PASS.

- [ ] **Step 5: Commit**
  ```bash
  git add examples/companion_radio/SettingsTable.* test/test_companion_settings/test_settings_table.cpp platformio.ini test/mocks/Arduino.h
  git commit -m "feat(companion): add the companion settings table"
  ```

### Task 4: `CompanionSettings` core (ports, load, set, Edit, save, applyAll)

**Files:**
- Create: `examples/companion_radio/SettingsPorts.h`, `examples/companion_radio/CompanionSettings.h`, `examples/companion_radio/CompanionSettings.cpp`
- Create: `test/test_companion_settings/test_companion_settings.cpp` (with `FakeApplier` and `FakeStore` in the file)
- Modify: `platformio.ini` `[env:native]`: add `+<../examples/companion_radio/CompanionSettings.cpp>`

**Interfaces:**
- Consumes: Task 3's table types; `PrefsDecodeResult` is not used here, since the store hides it.
- Produces (`SettingsPorts.h`):
  ```cpp
  struct SettingsApplier {
    enum Kind : uint8_t { Radio, TxPower, RxBoost, Cad, Gps };
    virtual bool supports(Kind) const = 0;
    virtual bool applyRadio(float freq, float bw, uint8_t sf, uint8_t cr) = 0;
    virtual bool applyTxPower(int8_t dbm) = 0;
    virtual bool applyRxBoost(bool on) = 0;
    virtual bool applyCad(bool on) = 0;
    virtual bool applyGps(bool on, uint32_t interval_s) = 0;
    virtual void onChanged(SettingId id) = 0;
    virtual ~SettingsApplier() {}
  };
  struct PrefsStore {
    virtual bool load(NodePrefs& p, double& lat, double& lon) = 0;
    virtual bool save(const NodePrefs& p, double lat, double lon) = 0;
    virtual ~PrefsStore() {}
  };
  ```
- Produces (`CompanionSettings.h`):
  ```cpp
  struct SetResult { enum Code : uint8_t { Ok, OkReboot, Clamped, BadValue, Unsupported } code; };
  enum class Origin : uint8_t { Local, Remote };
  class CompanionSettings {
  public:
    explicit CompanionSettings(const SettingsBuild& build);
    void begin(PrefsStore& store, SettingsApplier& applier, double* lat, double* lon);
    const NodePrefs& prefs() const;
    NodePrefs& editable();          // PR 1 and 2 only; removed in PR 3 when getNodePrefs() turns const
    void loadDefaults();
    void load();                    // store.load, then sanitise rows with sanitize_on_load
    bool save();                    // store.save(prefs, *lat, *lon)
    void applyAll();                // applyRadio, applyTxPower, applyRxBoost (if supported), applyCad
    SetResult setInt(SettingId, int32_t, Origin = Origin::Local);
    SetResult setFloat(SettingId, float, Origin = Origin::Local);
    SetResult setDouble(SettingId, double, Origin = Origin::Local);
    SetResult setText(SettingId, const char*, Origin = Origin::Local);
    SetResult setScope(const char* name, const uint8_t key[16]);
    class Edit {
    public:
      Edit& setInt(SettingId, int32_t); Edit& setFloat(SettingId, float);
      Edit& setDouble(SettingId, double); Edit& setText(SettingId, const char*);
      SetResult commit(Origin = Origin::Local);
    };
    Edit edit();
    const SettingInfo& describe(SettingId) const;
  };
  ```

**Behaviour** (from the spec's "Behaviour rules", "Load-time sanitise" and "`Edit`"):
- **Single sets:** `setX` is a one-field `Edit`.
- **`commit()` order:**
  1. Validate every staged field: range or clamp, the validator, and `ClientRepeat` checked against the staged freq.
  2. Check `supports(kind)` for the changed fields only.
  3. If anything fails: return `BadValue` or `Unsupported`, and store, apply and save nothing.
  4. Copy the staged values in.
  5. Apply only the kinds whose fields changed, in this order: Radio (one `applyRadio` with all four values), TxPower, RxBoost, Cad, Gps.
  6. Call `onChanged(id)` for each changed field.
  7. Save once.
- **No change:** if no field changed, return `Ok` with no apply and no save.
- **Result codes:**
  - `Origin::Remote` skips `applyRadio` and returns `OkReboot`. The other kinds still apply.
  - `ApplyKind::Reboot` returns `OkReboot`.
  - A clamp returns `Clamped`.
  - **Several results in one edit:** the first failure wins (`BadValue` or `Unsupported`, in field order). Otherwise `Clamped` beats `OkReboot`, which beats `Ok`.
- **Load sanitise:** for each row with `sanitize_on_load`:
  - Bool rows are forced to 0 or 1.
  - Numeric rows are clamped to the row range.
  - A NaN or infinite float becomes `settingDefault`.
  - Rows without the flag are left as stored.

- [ ] **Step 1: Write the failing tests.** Every test builds a `SettingsBuild` with `freq 869.525f, bw 250, sf 11, cr 5, tx_power 20, max_tx_power 22, rx_boost_default 1`. `name_ok` rejects `[`, and `repeat_freq_ok` accepts only 433000-434000.
  ```cpp
  TEST(CompanionSettingsLoad, DefaultsFromTableAndBuild)   // freq 869.525, airtime 1.0, cad 1, rx boost 1, name "NONAME"
  TEST(CompanionSettingsLoad, SanitiseClampsTodayRanges)   // stored sf 20 -> 12, freq 100 -> 150, freq 300 stays 300, max_hops 99 -> 64
  TEST(CompanionSettingsLoad, StricterRulesNotAppliedOnLoad) // stored ble_pin 1234, multi_acks 5, name "a[b" load unchanged
  TEST(CompanionSettingsLoad, NanFloatsLoadAsDefaults)     // stored freq NaN -> 869.525; bw +inf -> 250
  TEST(CompanionSettingsSet, InRangeAppliesAndSavesOnce)   // setInt(TxPower, 10): Ok, applyTxPower(10) once, store.saves == 1
  TEST(CompanionSettingsSet, OutOfRangeRejected)           // setInt(TxPower, 30): BadValue, no apply, saves == 0, value unchanged
  TEST(CompanionSettingsSet, ClampRowsClamp)               // setInt(AutoaddMaxHops, 99): Clamped, value 64, saves == 1
  TEST(CompanionSettingsSet, SameValueNoSave)              // setInt(TxPower, current): Ok, no apply, saves == 0
  TEST(CompanionSettingsSet, UnsupportedNotSaved)          // fake supports(RxBoost) false; setInt(RxBoostedGain, 0): Unsupported, saves == 0
  TEST(CompanionSettingsSet, PinIsRebootOnly)              // setInt(BlePin, 123456): OkReboot; setInt(BlePin, 1234): BadValue
  TEST(CompanionSettingsSet, NameValidated)                // setText(NodeName, "a[b"): BadValue; 32 chars: BadValue; "Bob": Ok
  TEST(CompanionSettingsSet, ClientRepeatChecksFreq)       // freq 869.525: setInt(ClientRepeat, 1) BadValue; edit freq 433.5 + repeat 1: Ok
  TEST(CompanionSettingsEdit, RadioAppliedOnceWithAllFour) // edit Freq, Bw, Sf, Cr: one applyRadio(433.5, 125, 9, 6), saves == 1
  TEST(CompanionSettingsEdit, OneBadFieldStoresNothing)    // edit Freq 433.5 + Sf 20: BadValue, freq unchanged, no apply, saves == 0
  TEST(CompanionSettingsEdit, UnchangedUnsupportedFieldDoesNotBlock) // RxBoost unsupported, edit MultiAcks 1 + RxBoostedGain(current): Ok
  TEST(CompanionSettingsEdit, ApplyOrder)                  // edit Cad, TxPower, Freq: fake records Radio, TxPower, Cad in that order
  TEST(CompanionSettingsEdit, RemoteRadioSavedNotApplied)  // commit(Origin::Remote) with Freq + TxPower: OkReboot, no applyRadio, applyTxPower once, saves == 1
  TEST(CompanionSettingsEdit, OnChangedPerChangedField)    // edit BuzzerQuiet 1 + MultiAcks(current): onChanged(BuzzerQuiet) only
  TEST(CompanionSettings, ApplyAllAtBoot)                  // applyAll: applyRadio, applyTxPower, applyRxBoost, applyCad once each; no applyGps
  TEST(CompanionSettings, ScopeWritesNameAndKey)           // setScope("#nl", key): name "#nl" and key stored, saves == 1
  TEST(CompanionSettings, LatLonThroughPointers)           // setDouble(Lat, 52.1) writes *lat; setDouble(Lat, 91) BadValue
  ```

- [ ] **Step 2: Run the tests to verify they fail.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: build fails, because `CompanionSettings.h` is not found.

- [ ] **Step 3: Implement `SettingsPorts.h` and `CompanionSettings`.** Implement exactly the interfaces and behaviour above. `Edit` stages into a copy of `NodePrefs` plus a lat/lon pair and a changed-field bitmask.

- [ ] **Step 4: Run the tests to verify they pass.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: all PASS.

- [ ] **Step 5: Commit**
  ```bash
  git add examples/companion_radio/SettingsPorts.h examples/companion_radio/CompanionSettings.* test/test_companion_settings/test_companion_settings.cpp
  git commit -m "feat(companion): add CompanionSettings with apply and store ports"
  ```

### Task 5: Text front end (`setByKey` / `getByKey`)

**Files:**
- Modify: `examples/companion_radio/CompanionSettings.h/.cpp`
- Create: `test/test_companion_settings/test_settings_text.cpp`

**Interfaces:**
- Consumes: Task 4's `CompanionSettings`, `Origin`, `SetResult`; Task 3's `findSettingByKey` and row texts.
- Produces:
  - `bool setByKey(const char* key, const char* value, Origin origin, char* reply, size_t reply_len);`. It returns false only when the key is unknown or not CLI-visible, and in that case leaves `reply` untouched.
  - `bool getByKey(const char* key, char* reply, size_t reply_len);`

**Reply rules:**
- **Value parsing:** trim leading and trailing spaces, tabs, CR and LF before parsing.
- **Success texts:**
  - `Ok` gives `ok_fmt` (default "OK").
  - `OkReboot` gives `ok_fmt` if the row has one, else "OK - reboot to apply".
  - `Clamped` gives "OK - clamped to %s".
- **Error texts:**
  - `BadValue` gives `err_fmt`, filled with min and max formatted like the get texts. `TxPower` takes `max_tx_power`. Bool rows say "Error, must be on or off".
  - `Unsupported` gives "Error: unsupported".
- **Get texts:** floats use `StrHelper::ftoa`, integers "> %d", bool rows "> on" / "> off", text "> %s".

- [ ] **Step 1: Write the failing tests** (golden strings).
  ```cpp
  TEST(SettingsText, SetTxOk)              // setByKey("tx", "10") -> "OK"
  TEST(SettingsText, SetTxTooHigh)         // setByKey("tx", "30") -> "Error, must be -9 to 22"
  TEST(SettingsText, SetAfTooHigh)         // "af", "10" -> "ERROR: af must be 0-9"
  TEST(SettingsText, SetRxDelayBad)        // "rxdelay", "21" -> "Error, must be 0-20"
  TEST(SettingsText, SetPathHashBad)       // "path.hash.mode", "3" -> "Error, must be 0,1, or 2"
  TEST(SettingsText, SetPin)               // "pin", "123456" -> "> pin is now 123456"; "pin", "12" -> "Error, pin must be 0 or 100000-999999"
  TEST(SettingsText, SetNameBadChars)      // "name", "a[b" -> "Error, bad chars"
  TEST(SettingsText, CadOnOff)             // "cad", "off" -> "OK"; getByKey("cad") -> "> off"; "cad", "maybe" -> "Error, must be on or off"
  TEST(SettingsText, GetFreqUsesFtoa)      // getByKey("freq") == "> " + StrHelper::ftoa(869.525f)
  TEST(SettingsText, RemoteFreqRebootText) // setByKey("freq", "433.5", Origin::Remote) -> "OK - reboot to apply", no applyRadio
  TEST(SettingsText, UnknownKeyFalse)      // setByKey("manual.add", "1") returns false, reply unchanged
  TEST(SettingsText, TrailingWhitespaceTrimmed) // "tx", "20\r\n" -> "OK" and tx == 20; "cad", "on " -> "OK"
  ```

- [ ] **Step 2: Run the tests to verify they fail.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: build fails with `setByKey` not a member.

- [ ] **Step 3: Implement `setByKey` and `getByKey`** with the reply rules above.

- [ ] **Step 4: Run the tests to verify they pass.**
  Run: `pio test -e native -f test_companion_settings`
  Expected: all PASS.

- [ ] **Step 5: Commit**
  ```bash
  git add examples/companion_radio/CompanionSettings.* test/test_companion_settings/test_settings_text.cpp
  git commit -m "feat(companion): add get and set by key to CompanionSettings"
  ```

### Task 6: MyMesh uses the module for defaults, load, save and boot apply

**Files:**
- Create: `examples/companion_radio/DataStorePrefsStore.h`. This header-only adapter is the one settings file that includes `DataStore.h`.
- Modify:
  - `examples/companion_radio/MyMesh.h`: class `MyMesh` gains `public SettingsApplier`; member `NodePrefs _prefs;` (line 758) becomes `CompanionSettings _settings; NodePrefs& _prefs;`; `savePrefs()` (line 565).
  - `examples/companion_radio/MyMesh.cpp`:
    - constructor init list (2473-2475) and the defaults block (2497-2516);
    - `applyRadioFromPrefs` (2519-2527);
    - `begin` load and sanitise (2612-2628).
- Modify: `changelog_ev.md` (Unreleased, `### Changed`)

**Interfaces:**
- Consumes: everything from Tasks 1-5.
- Produces:
  - `class DataStorePrefsStore : public PrefsStore`. Its constructor takes `DataStore&`; `load` calls `DataStore::loadPrefs` and returns true; `save` calls `DataStore::savePrefs` and returns true.
  - `CompanionSettings& MyMesh::settings();` (public accessor, used by PR 2 and 3).

**Steps:**

1. **Wire up MyMesh.**
   - **Member order:** declare `_settings` before `_prefs`. Initialise them as `_settings(makeSettingsBuild())` and `_prefs(_settings.editable())`.
   - **`makeSettingsBuild()`:** a file-static function in MyMesh.cpp. It fills `SettingsBuild` from these macros:
     - `LORA_FREQ`, `LORA_BW`, `LORA_SF`, `LORA_CR`, `LORA_TX_POWER`, `MAX_LORA_TX_POWER`;
     - the `rx_boosted_gain` constructor logic (2510-2516), unchanged;
     - `name_ok = AdvertDataParser::isValidName`;
     - `repeat_freq_ok`: a static trampoline over the repeat-frequency table (it is a const member today, so move the range check into a static helper both use).
   - **Constructor:** the defaults block becomes `_settings.loadDefaults();`.
   - **`begin`:**
     - Call `_settings.begin(_prefs_store, *this, &sensors.node_lat, &sensors.node_lon)` before the name default (2602-2609).
     - Replace the `_store->loadPrefs` + sanitise block with `_settings.load();`.
     - Keep the `node_name` default lines before it, as today.
     - `_prefs_store` is a `DataStorePrefsStore` member constructed from `store`.
   - **`savePrefs()`:** becomes `_settings.save();`.
   - **`applyRadioFromPrefs()`:** becomes `_settings.applyAll();`. It keeps its name, because the touch UI calls it.

2. **Implement the `SettingsApplier` overrides on MyMesh.**
   - `supports`:
     - `Radio`, `TxPower` and `Cad`: true.
     - `RxBoost`: true under `USE_SX1262 || USE_SX1268 || SX126X_RX_BOOSTED_GAIN`.
     - `Gps`: true under `ENV_INCLUDE_GPS == 1`.
   - `applyRadio`: `radio_driver.setParams(...)`.
   - `applyTxPower`: `radio_driver.setTxPower`.
   - `applyRxBoost`: `radio_driver.setRxBoostedGainMode` under the same guard, else false. This keeps today's `MESH_DEBUG_PRINTLN`.
   - `applyCad`: `_radio->setCADEnabled`.
   - `applyGps`: `sensors.setSettingValue("gps", on ? "1" : "0")`, then `sensors.setSettingValue("gps_interval", buf)` where `buf` holds the interval formatted `"%lu"`. It returns the first call's result.
   - `onChanged`: empty in PR 1.

3. **Build the PR check envs, plus one per UI.**
   Run, one at a time, with `FIRMWARE_VERSION=v0.0.0.0-probe DISABLE_DEBUG=1 sh build.sh build-firmware <env>` for:
   - `heltec_v4_companion_radio_usb_tcp`
   - `Heltec_v3_companion_radio_usb_tcp`
   - `WioTrackerL1_companion_radio_ble`
   - `Heltec_v2_companion_radio_ble`
   - `LilyGo_T-Echo_Card_companion_radio_ble`

   The touch env builds in CI.
   Expected: every env reports `SUCCESS`. Note the `Flash:` line of V4, Wio L1 and T-Echo Card next to the same envs built on `origin/main`. The delta must be at most 3 KB.

4. **Run the native tests.**
   Run: `pio test -e native`
   Expected: all suites PASS.

5. **Add the changelog entry** under Unreleased `### Changed`: "Companion settings module (#NN)". It says:
   - what it is;
   - that this PR only moves defaults, load, save and boot apply;
   - the load-time behaviour change: freq 150-2500, and NaN floats load as defaults;
   - the flash delta;
   - "Builds, not tested on hardware".

6. **Commit and open the PR.**
   ```bash
   git add examples/companion_radio/MyMesh.h examples/companion_radio/MyMesh.cpp examples/companion_radio/DataStorePrefsStore.h changelog_ev.md
   git commit -m "feat(companion): load, save and apply prefs through CompanionSettings"
   git push -u origin Feat/companion-settings-module
   gh pr create --repo Elektr0Vodka/meshcomod --base main --title "feat(companion): add the CompanionSettings module (PR 1 of 3)" --body-file <file>
   ```
   The PR body:
   - links the spec;
   - lists the one behaviour change;
   - gives the flash deltas and the build list;
   - asks for a hardware check on V4 or V3: settings survive a reboot, and an older build loads the file after a downgrade.
