#include "SettingsTable.h"

#include <string.h>

#define NP(field) ((int16_t)offsetof(NodePrefs, field))

// Columns: key, default, min, max, offset, id, type, apply, flags. Rows are in SettingId
// order (the table test checks it).
static const SettingInfo ROWS[] = {
  { "name", 0, 1, 31, NP(node_name), SettingId::NodeName, SettingType::Text, ApplyKind::None, S_CLI },
  { "freq", 0, 150, 2500, NP(freq), SettingId::Freq, SettingType::Float, ApplyKind::Radio, S_CLI|S_LOAD },
  { "bw", 0, 7.8f, 500, NP(bw), SettingId::Bw, SettingType::Float, ApplyKind::Radio, S_CLI|S_LOAD },
  { "sf", 0, 5, 12, NP(sf), SettingId::Sf, SettingType::U8, ApplyKind::Radio, S_CLI|S_LOAD },
  { "cr", 0, 5, 8, NP(cr), SettingId::Cr, SettingType::U8, ApplyKind::Radio, S_CLI|S_LOAD },
  { "tx", 0, -9, 0, NP(tx_power_dbm), SettingId::TxPower, SettingType::I8, ApplyKind::TxPower, S_CLI|S_LOAD },
  { "af", 1.0f, 0, 9, NP(airtime_factor), SettingId::AirtimeFactor, SettingType::Float, ApplyKind::None, S_CLI|S_LOAD },
  { "rxdelay", 0, 0, 20, NP(rx_delay_base), SettingId::RxDelayBase, SettingType::Float, ApplyKind::None, S_CLI|S_LOAD },
  { "multi.acks", 0, 0, 1, NP(multi_acks), SettingId::MultiAcks, SettingType::U8, ApplyKind::None, S_CLI },
  { "manual.add", 0, 0, 1, NP(manual_add_contacts), SettingId::ManualAddContacts, SettingType::U8, ApplyKind::None, 0 },
  { "telemetry.base", 0, 0, 2, NP(telemetry_mode_base), SettingId::TelemetryBase, SettingType::U8, ApplyKind::None, 0 },
  { "telemetry.loc", 0, 0, 2, NP(telemetry_mode_loc), SettingId::TelemetryLoc, SettingType::U8, ApplyKind::None, 0 },
  { "telemetry.env", 0, 0, 2, NP(telemetry_mode_env), SettingId::TelemetryEnv, SettingType::U8, ApplyKind::None, 0 },
  { "advert.loc", 0, 0, 1, NP(advert_loc_policy), SettingId::AdvertLocPolicy, SettingType::U8, ApplyKind::None, 0 },
  { "client.repeat", 0, 0, 1, NP(client_repeat), SettingId::ClientRepeat, SettingType::U8, ApplyKind::None, 0 },
  { "path.hash.mode", 0, 0, 2, NP(path_hash_mode), SettingId::PathHashMode, SettingType::U8, ApplyKind::None, S_CLI|S_LOAD },
  { "pin", 0, 100000, 999999, NP(ble_pin), SettingId::BlePin, SettingType::Pin, ApplyKind::Reboot, S_CLI },
  { "buzzer.quiet", 0, 0, 1, NP(buzzer_quiet), SettingId::BuzzerQuiet, SettingType::Bool, ApplyKind::Ui, 0 },
  { "gps", 0, 0, 1, NP(gps_enabled), SettingId::GpsEnabled, SettingType::Bool, ApplyKind::Gps, S_LOAD },
  { "gps.interval", 0, 0, 86400, NP(gps_interval), SettingId::GpsInterval, SettingType::U32, ApplyKind::Gps, S_CLAMP|S_LOAD },
  { "autoadd.config", 0, 0, 255, NP(autoadd_config), SettingId::AutoaddConfig, SettingType::U8, ApplyKind::None, 0 },
  { "autoadd.max.hops", 0, 0, 64, NP(autoadd_max_hops), SettingId::AutoaddMaxHops, SettingType::U8, ApplyKind::None, S_CLAMP|S_LOAD },
  { "radio.rxgain", 0, 0, 1, NP(rx_boosted_gain), SettingId::RxBoostedGain, SettingType::Bool, ApplyKind::RxBoost, S_CLI|S_LOAD },
  { "cad", 1, 0, 1, NP(cad_enabled), SettingId::CadEnabled, SettingType::Bool, ApplyKind::Cad, S_CLI|S_LOAD },
  { "default.scope", 0, 0, 30, NP(default_scope_name), SettingId::DefaultScope, SettingType::Scope, ApplyKind::None, 0 },
  { "lat", 0, -90, 90, -1, SettingId::Lat, SettingType::Double, ApplyKind::None, 0 },
  { "lon", 0, -180, 180, -1, SettingId::Lon, SettingType::Double, ApplyKind::None, 0 },
};


// Reply texts that differ from the defaults ("OK", "Error, must be X to Y"), as today's CLI sends them.
struct SettingTexts {
  SettingId id;
  const char* ok;
  const char* err;
};
static const SettingTexts TEXTS[] = {
  { SettingId::NodeName, nullptr, "Error, bad chars" },
  { SettingId::TxPower, nullptr, "Error, must be -9 to %d" },
  { SettingId::AirtimeFactor, nullptr, "ERROR: af must be 0-9" },
  { SettingId::RxDelayBase, nullptr, "Error, must be 0-20" },
  { SettingId::PathHashMode, nullptr, "Error, must be 0,1, or 2" },
  { SettingId::BlePin, "> pin is now %06d", "Error, pin must be 0 or 100000-999999" },
};

static_assert(sizeof(ROWS) / sizeof(ROWS[0]) == (size_t)SettingId::Count, "one row per SettingId");

const SettingInfo& settingInfo(SettingId id) {
  return ROWS[(size_t)id];
}

const SettingInfo* findSettingByKey(const char* key) {
  if (key == nullptr) return nullptr;
  for (const SettingInfo& s : ROWS) {
    if ((s.flags & S_CLI) && strcmp(s.key, key) == 0) return &s;
  }
  return nullptr;
}

float settingDefault(const SettingInfo& s, const SettingsBuild& b) {
  switch (s.id) {
    case SettingId::Freq: return b.freq;
    case SettingId::Bw: return b.bw;
    case SettingId::Sf: return b.sf;
    case SettingId::Cr: return b.cr;
    case SettingId::TxPower: return b.tx_power;
    case SettingId::RxBoostedGain: return b.rx_boost_default;
    default: return s.def;
  }
}

const char* settingOkText(SettingId id) {
  for (const SettingTexts& t : TEXTS) if (t.id == id) return t.ok;
  return nullptr;
}

const char* settingErrText(SettingId id) {
  for (const SettingTexts& t : TEXTS) if (t.id == id) return t.err;
  return nullptr;
}

float settingMax(const SettingInfo& s, const SettingsBuild& b) {
  return s.id == SettingId::TxPower ? (float)b.max_tx_power : s.max;
}
