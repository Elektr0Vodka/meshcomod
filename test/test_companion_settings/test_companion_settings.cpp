#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>

#include "settings_fakes.h"

TEST(CompanionSettingsLoad, DefaultsFromTableAndBuild) {
  SettingsFixture f;
  const NodePrefs& p = f.settings.prefs();
  EXPECT_EQ(869.525f, p.freq);
  EXPECT_EQ(250.0f, p.bw);
  EXPECT_EQ(11, p.sf);
  EXPECT_EQ(20, p.tx_power_dbm);
  EXPECT_EQ(1.0f, p.airtime_factor);
  EXPECT_EQ(1, p.cad_enabled);
  EXPECT_EQ(1, p.rx_boosted_gain);
  EXPECT_STREQ("NONAME", p.node_name);
  EXPECT_EQ(0u, p.ble_pin);
}

TEST(CompanionSettingsLoad, SanitiseClampsTodayRanges) {
  SettingsFixture f;
  NodePrefs file = f.defaults();
  file.sf = 20;
  file.freq = 100;
  file.autoadd_max_hops = 99;
  file.gps_enabled = 5;
  file.cad_enabled = 7;
  f.store.stored = file;
  f.store.has_file = true;
  f.settings.load();
  EXPECT_EQ(12, f.settings.prefs().sf);
  EXPECT_EQ(150.0f, f.settings.prefs().freq);
  EXPECT_EQ(64, f.settings.prefs().autoadd_max_hops);
  EXPECT_EQ(1, f.settings.prefs().gps_enabled);
  EXPECT_EQ(1, f.settings.prefs().cad_enabled);

  SettingsFixture g;
  NodePrefs file2 = g.defaults();
  file2.freq = 300;
  g.store.stored = file2;
  g.store.has_file = true;
  g.settings.load();
  EXPECT_EQ(300.0f, g.settings.prefs().freq);   // was raised to 400 before
}

TEST(CompanionSettingsLoad, StricterRulesNotAppliedOnLoad) {
  SettingsFixture f;
  NodePrefs file = f.defaults();
  file.ble_pin = 1234;
  file.multi_acks = 5;
  strcpy(file.node_name, "a[b");
  f.store.stored = file;
  f.store.has_file = true;
  f.settings.load();
  EXPECT_EQ(1234u, f.settings.prefs().ble_pin);
  EXPECT_EQ(5, f.settings.prefs().multi_acks);
  EXPECT_STREQ("a[b", f.settings.prefs().node_name);
  EXPECT_EQ(0, f.store.saves);
}

TEST(CompanionSettingsLoad, NanFloatsLoadAsDefaults) {
  SettingsFixture f;
  NodePrefs file = f.defaults();
  file.freq = std::numeric_limits<float>::quiet_NaN();
  file.bw = std::numeric_limits<float>::infinity();
  file.airtime_factor = std::numeric_limits<float>::quiet_NaN();
  f.store.stored = file;
  f.store.has_file = true;
  f.settings.load();
  EXPECT_EQ(869.525f, f.settings.prefs().freq);
  EXPECT_EQ(250.0f, f.settings.prefs().bw);
  EXPECT_EQ(1.0f, f.settings.prefs().airtime_factor);
}

TEST(CompanionSettingsSet, InRangeAppliesAndSavesOnce) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::Ok, f.settings.setInt(SettingId::TxPower, 10).code);
  EXPECT_EQ(10, f.settings.prefs().tx_power_dbm);
  EXPECT_EQ(1, f.applier.count("tx 10"));
  EXPECT_EQ(1, f.store.saves);
}

TEST(CompanionSettingsSet, OutOfRangeRejected) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::BadValue, f.settings.setInt(SettingId::TxPower, 30).code);
  EXPECT_EQ(20, f.settings.prefs().tx_power_dbm);
  EXPECT_TRUE(f.applier.calls.empty());
  EXPECT_EQ(0, f.store.saves);
}

TEST(CompanionSettingsSet, ClampRowsClamp) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::Clamped, f.settings.setInt(SettingId::AutoaddMaxHops, 99).code);
  EXPECT_EQ(64, f.settings.prefs().autoadd_max_hops);
  EXPECT_EQ(1, f.store.saves);
}

TEST(CompanionSettingsSet, SameValueNoSave) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::Ok, f.settings.setInt(SettingId::TxPower, 20).code);
  EXPECT_TRUE(f.applier.calls.empty());
  EXPECT_EQ(0, f.store.saves);
}

TEST(CompanionSettingsSet, UnsupportedNotSaved) {
  SettingsFixture f;
  f.applier.support[SettingsApplier::RxBoost] = false;
  EXPECT_EQ(SetResult::Unsupported, f.settings.setInt(SettingId::RxBoostedGain, 0).code);
  EXPECT_EQ(1, f.settings.prefs().rx_boosted_gain);
  EXPECT_EQ(0, f.store.saves);
}

TEST(CompanionSettingsSet, PinIsRebootOnly) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::OkReboot, f.settings.setInt(SettingId::BlePin, 123456).code);
  EXPECT_EQ(123456u, f.settings.prefs().ble_pin);
  EXPECT_EQ(SetResult::BadValue, f.settings.setInt(SettingId::BlePin, 1234).code);
  EXPECT_EQ(SetResult::OkReboot, f.settings.setInt(SettingId::BlePin, 0).code);
}

TEST(CompanionSettingsSet, NameValidated) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::BadValue, f.settings.setText(SettingId::NodeName, "a[b").code);
  EXPECT_EQ(SetResult::BadValue, f.settings.setText(SettingId::NodeName, "0123456789012345678901234567890X").code);
  EXPECT_EQ(SetResult::BadValue, f.settings.setText(SettingId::NodeName, "").code);
  EXPECT_EQ(SetResult::Ok, f.settings.setText(SettingId::NodeName, "Bob").code);
  EXPECT_STREQ("Bob", f.settings.prefs().node_name);
}

TEST(CompanionSettingsSet, ClientRepeatChecksFreq) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::BadValue, f.settings.setInt(SettingId::ClientRepeat, 1).code);
  SetResult r = f.settings.edit().setFloat(SettingId::Freq, 433.5f).setInt(SettingId::ClientRepeat, 1).commit();
  EXPECT_EQ(SetResult::Ok, r.code);
  EXPECT_EQ(1, f.settings.prefs().client_repeat);
}

TEST(CompanionSettingsEdit, RadioAppliedOnceWithAllFour) {
  SettingsFixture f;
  SetResult r = f.settings.edit()
                    .setFloat(SettingId::Freq, 433.5f)
                    .setFloat(SettingId::Bw, 125)
                    .setInt(SettingId::Sf, 9)
                    .setInt(SettingId::Cr, 6)
                    .commit();
  EXPECT_EQ(SetResult::Ok, r.code);
  ASSERT_EQ(1u, f.applier.calls.size());
  EXPECT_EQ("radio 433.5 125 9 6", f.applier.calls[0]);
  EXPECT_EQ(1, f.store.saves);
}

TEST(CompanionSettingsEdit, OneBadFieldStoresNothing) {
  SettingsFixture f;
  SetResult r = f.settings.edit().setFloat(SettingId::Freq, 433.5f).setInt(SettingId::Sf, 20).commit();
  EXPECT_EQ(SetResult::BadValue, r.code);
  EXPECT_EQ(869.525f, f.settings.prefs().freq);
  EXPECT_TRUE(f.applier.calls.empty());
  EXPECT_EQ(0, f.store.saves);
}

TEST(CompanionSettingsEdit, UnchangedUnsupportedFieldDoesNotBlock) {
  SettingsFixture f;
  f.applier.support[SettingsApplier::RxBoost] = false;
  SetResult r = f.settings.edit().setInt(SettingId::MultiAcks, 1).setInt(SettingId::RxBoostedGain, 1).commit();
  EXPECT_EQ(SetResult::Ok, r.code);
  EXPECT_EQ(1, f.settings.prefs().multi_acks);
  EXPECT_EQ(1, f.store.saves);
}

TEST(CompanionSettingsEdit, ApplyOrder) {
  SettingsFixture f;
  f.settings.edit().setInt(SettingId::CadEnabled, 0).setInt(SettingId::TxPower, 15).setFloat(SettingId::Freq, 868).commit();
  ASSERT_EQ(3u, f.applier.calls.size());
  EXPECT_EQ(0u, f.applier.calls[0].find("radio "));
  EXPECT_EQ("tx 15", f.applier.calls[1]);
  EXPECT_EQ("cad off", f.applier.calls[2]);
}

TEST(CompanionSettingsEdit, RemoteRadioSavedNotApplied) {
  SettingsFixture f;
  SetResult r = f.settings.edit().setFloat(SettingId::Freq, 868).setInt(SettingId::TxPower, 15).commit(Origin::Remote);
  EXPECT_EQ(SetResult::OkReboot, r.code);
  EXPECT_EQ(0, f.applier.count("radio"));
  EXPECT_EQ(1, f.applier.count("tx 15"));
  EXPECT_EQ(868.0f, f.settings.prefs().freq);
  EXPECT_EQ(1, f.store.saves);
}

TEST(CompanionSettingsEdit, OnChangedPerChangedField) {
  SettingsFixture f;
  f.settings.edit().setInt(SettingId::BuzzerQuiet, 1).setInt(SettingId::MultiAcks, 0).commit();
  ASSERT_EQ(1u, f.applier.changed.size());
  EXPECT_EQ(SettingId::BuzzerQuiet, f.applier.changed[0]);
}

TEST(CompanionSettings, ApplyAllAtBoot) {
  SettingsFixture f;
  f.settings.applyAll();
  EXPECT_EQ(1, f.applier.count("radio 869.525 250 11 5"));
  EXPECT_EQ(1, f.applier.count("tx 20"));
  EXPECT_EQ(1, f.applier.count("rxboost on"));
  EXPECT_EQ(1, f.applier.count("cad on"));
  EXPECT_EQ(0, f.applier.count("gps"));
}

TEST(CompanionSettings, ScopeWritesNameAndKey) {
  SettingsFixture f;
  uint8_t key[16];
  for (int i = 0; i < 16; i++) key[i] = (uint8_t)(i + 1);
  EXPECT_EQ(SetResult::Ok, f.settings.setScope("#nl", key).code);
  EXPECT_STREQ("#nl", f.settings.prefs().default_scope_name);
  EXPECT_EQ(0, memcmp(key, f.settings.prefs().default_scope_key, 16));
  EXPECT_EQ(1, f.store.saves);
}

TEST(CompanionSettings, LatLonThroughPointers) {
  SettingsFixture f;
  EXPECT_EQ(SetResult::Ok, f.settings.setDouble(SettingId::Lat, 52.1).code);
  EXPECT_EQ(52.1, f.lat);
  EXPECT_EQ(52.1, f.store.stored_lat);
  EXPECT_EQ(SetResult::BadValue, f.settings.setDouble(SettingId::Lat, 91).code);
  EXPECT_EQ(52.1, f.lat);
}
