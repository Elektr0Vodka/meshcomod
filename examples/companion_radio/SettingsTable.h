#pragma once

#include <stddef.h>
#include <stdint.h>

#include "NodePrefs.h"

// One row per companion setting: its CLI key, type, where it lives, default, valid range,
// how a change is applied, and its reply texts. CompanionSettings is the only user; it
// validates, applies and saves from these rows. Pure data, so it runs in the native tests.

enum class SettingId : uint8_t {
  NodeName, Freq, Bw, Sf, Cr, TxPower, AirtimeFactor, RxDelayBase,
  MultiAcks, ManualAddContacts, TelemetryBase, TelemetryLoc, TelemetryEnv, AdvertLocPolicy,
  ClientRepeat, PathHashMode, BlePin, BuzzerQuiet, GpsEnabled, GpsInterval, AutoaddConfig,
  AutoaddMaxHops, RxBoostedGain, CadEnabled, DefaultScope, Lat, Lon,
  Count
};

enum class SettingType : uint8_t { Bool, U8, I8, U32, Float, Double, Text, Pin, Scope };

// What a change needs besides being stored. Reboot: stored now, used after a restart.
enum class ApplyKind : uint8_t { None, Radio, TxPower, RxBoost, Cad, Gps, Ui, Reboot };

struct SettingInfo {
  SettingId id;
  const char* key;       // CLI name
  const char* label;     // short UI label
  SettingType type;
  int16_t offset;        // offsetof(NodePrefs, field); -1 for Lat/Lon (kept in sensors)
  float def, min, max;   // Text/Scope: min/max are lengths. Some come from SettingsBuild.
  bool clamp;            // out of range: true clamps (result Clamped), false rejects
  ApplyKind apply;
  bool cli;              // reachable by get/set <key>
  bool sanitize_on_load; // range enforced at boot (only where boot enforced it before)
  const char* ok_fmt;    // nullptr: "OK"
  const char* err_fmt;   // nullptr: "Error, must be %s to %s"
  const char* get_fmt;   // nullptr: by type ("> %d", "> on|off", "> %s", floats via ftoa)
};

// Values that depend on the build: radio defaults, the board's TX limit, and checks that
// live elsewhere in the firmware. MyMesh fills it from its macros; tests fill it by hand.
struct SettingsBuild {
  float freq, bw;
  uint8_t sf, cr;
  int8_t tx_power, max_tx_power;
  uint8_t rx_boost_default;
  bool (*name_ok)(const char* name);          // AdvertDataParser::isValidName
  bool (*repeat_freq_ok)(uint32_t freq_khz);  // client repeat allowed on this frequency
};

const SettingInfo& settingInfo(SettingId id);
const SettingInfo* findSettingByKey(const char* key);   // nullptr when unknown or not CLI
float settingDefault(const SettingInfo& s, const SettingsBuild& b);
float settingMax(const SettingInfo& s, const SettingsBuild& b);
