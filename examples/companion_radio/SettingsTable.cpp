#include "SettingsTable.h"

#include <string.h>

#define NP(field) ((int16_t)offsetof(NodePrefs, field))

// Columns: id, key, label, type, offset, default, min, max, clamp, apply, cli, sanitize on load,
// ok text, error text, get text. Rows are in SettingId order (the table test checks it).
static const SettingInfo ROWS[] = {
  { SettingId::NodeName, "name", "Name", SettingType::Text, NP(node_name), 0, 1, 31,
    false, ApplyKind::None, true, false, nullptr, "Error, bad chars", "> %s" },
  { SettingId::Freq, "freq", "Frequency", SettingType::Float, NP(freq), 0, 150, 2500,
    false, ApplyKind::Radio, true, true, nullptr, nullptr, nullptr },
  { SettingId::Bw, "bw", "Bandwidth", SettingType::Float, NP(bw), 0, 7.8f, 500,
    false, ApplyKind::Radio, true, true, nullptr, nullptr, nullptr },
  { SettingId::Sf, "sf", "Spreading factor", SettingType::U8, NP(sf), 0, 5, 12,
    false, ApplyKind::Radio, true, true, nullptr, nullptr, nullptr },
  { SettingId::Cr, "cr", "Coding rate", SettingType::U8, NP(cr), 0, 5, 8,
    false, ApplyKind::Radio, true, true, nullptr, nullptr, nullptr },
  { SettingId::TxPower, "tx", "TX power", SettingType::I8, NP(tx_power_dbm), 0, -9, 0,
    false, ApplyKind::TxPower, true, true, nullptr, "Error, must be -9 to %d", "> %d" },
  { SettingId::AirtimeFactor, "af", "Airtime factor", SettingType::Float, NP(airtime_factor), 1.0f, 0, 9,
    false, ApplyKind::None, true, true, nullptr, "ERROR: af must be 0-9", nullptr },
  { SettingId::RxDelayBase, "rxdelay", "RX delay", SettingType::Float, NP(rx_delay_base), 0, 0, 20,
    false, ApplyKind::None, true, true, nullptr, "Error, must be 0-20", nullptr },
  { SettingId::MultiAcks, "multi.acks", "Multi ACKs", SettingType::U8, NP(multi_acks), 0, 0, 1,
    false, ApplyKind::None, true, false, nullptr, nullptr, "> %d" },
  { SettingId::ManualAddContacts, "manual.add", "Manual add", SettingType::U8, NP(manual_add_contacts), 0, 0, 1,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::TelemetryBase, "telemetry.base", "Telemetry", SettingType::U8, NP(telemetry_mode_base), 0, 0, 2,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::TelemetryLoc, "telemetry.loc", "Telemetry location", SettingType::U8, NP(telemetry_mode_loc), 0, 0, 2,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::TelemetryEnv, "telemetry.env", "Telemetry sensors", SettingType::U8, NP(telemetry_mode_env), 0, 0, 2,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::AdvertLocPolicy, "advert.loc", "Share location", SettingType::U8, NP(advert_loc_policy), 0, 0, 1,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::ClientRepeat, "client.repeat", "Client repeat", SettingType::U8, NP(client_repeat), 0, 0, 1,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::PathHashMode, "path.hash.mode", "Path hash", SettingType::U8, NP(path_hash_mode), 0, 0, 2,
    false, ApplyKind::None, true, true, nullptr, "Error, must be 0,1, or 2", "> %d" },
  { SettingId::BlePin, "pin", "BLE PIN", SettingType::Pin, NP(ble_pin), 0, 100000, 999999,
    false, ApplyKind::Reboot, true, false, "> pin is now %06d", "Error, pin must be 0 or 100000-999999", nullptr },
  { SettingId::BuzzerQuiet, "buzzer.quiet", "Buzzer off", SettingType::Bool, NP(buzzer_quiet), 0, 0, 1,
    false, ApplyKind::Ui, false, false, nullptr, nullptr, nullptr },
  { SettingId::GpsEnabled, "gps", "GPS", SettingType::Bool, NP(gps_enabled), 0, 0, 1,
    false, ApplyKind::Gps, false, true, nullptr, nullptr, nullptr },
  { SettingId::GpsInterval, "gps.interval", "GPS interval", SettingType::U32, NP(gps_interval), 0, 0, 86400,
    true, ApplyKind::Gps, false, true, nullptr, nullptr, nullptr },
  { SettingId::AutoaddConfig, "autoadd.config", "Auto-add", SettingType::U8, NP(autoadd_config), 0, 0, 255,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::AutoaddMaxHops, "autoadd.max.hops", "Auto-add hops", SettingType::U8, NP(autoadd_max_hops), 0, 0, 64,
    true, ApplyKind::None, false, true, nullptr, nullptr, nullptr },
  { SettingId::RxBoostedGain, "radio.rxgain", "RX boost", SettingType::Bool, NP(rx_boosted_gain), 0, 0, 1,
    false, ApplyKind::RxBoost, true, true, nullptr, nullptr, nullptr },
  { SettingId::CadEnabled, "cad", "CAD", SettingType::Bool, NP(cad_enabled), 1, 0, 1,
    false, ApplyKind::Cad, true, true, nullptr, nullptr, nullptr },
  { SettingId::DefaultScope, "default.scope", "Region", SettingType::Scope, NP(default_scope_name), 0, 0, 30,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::Lat, "lat", "Latitude", SettingType::Double, -1, 0, -90, 90,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
  { SettingId::Lon, "lon", "Longitude", SettingType::Double, -1, 0, -180, 180,
    false, ApplyKind::None, false, false, nullptr, nullptr, nullptr },
};

static_assert(sizeof(ROWS) / sizeof(ROWS[0]) == (size_t)SettingId::Count, "one row per SettingId");

const SettingInfo& settingInfo(SettingId id) {
  return ROWS[(size_t)id];
}

const SettingInfo* findSettingByKey(const char* key) {
  if (key == nullptr) return nullptr;
  for (const SettingInfo& s : ROWS) {
    if (s.cli && strcmp(s.key, key) == 0) return &s;
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

float settingMax(const SettingInfo& s, const SettingsBuild& b) {
  return s.id == SettingId::TxPower ? (float)b.max_tx_power : s.max;
}
