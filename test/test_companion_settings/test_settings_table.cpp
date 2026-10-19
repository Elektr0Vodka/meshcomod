#include <gtest/gtest.h>

#include <cstddef>
#include <cstring>
#include <set>
#include <string>

#include "../../examples/companion_radio/SettingsTable.h"

namespace {

SettingsBuild build() {
  SettingsBuild b;
  b.freq = 869.525f;
  b.bw = 250;
  b.sf = 11;
  b.cr = 5;
  b.tx_power = 20;
  b.max_tx_power = 22;
  b.rx_boost_default = 1;
  b.name_ok = nullptr;
  b.repeat_freq_ok = nullptr;
  return b;
}

}  // namespace

TEST(SettingsTable, EveryIdHasExactlyOneRow) {
  for (int i = 0; i < (int)SettingId::Count; i++) {
    EXPECT_EQ(i, (int)settingInfo((SettingId)i).id) << "row " << i;
  }
}

TEST(SettingsTable, KeysAreUnique) {
  std::set<std::string> keys;
  for (int i = 0; i < (int)SettingId::Count; i++) {
    const char* k = settingInfo((SettingId)i).key;
    ASSERT_NE(nullptr, k);
    EXPECT_TRUE(keys.insert(k).second) << "duplicate key " << k;
  }
}

TEST(SettingsTable, FindByKeyOnlyCliRows) {
  const SettingInfo* cad = findSettingByKey("cad");
  ASSERT_NE(nullptr, cad);
  EXPECT_EQ(SettingId::CadEnabled, cad->id);
  EXPECT_EQ(nullptr, findSettingByKey("manual.add"));
  EXPECT_EQ(nullptr, findSettingByKey("nope"));
  EXPECT_EQ(nullptr, findSettingByKey(nullptr));
}

TEST(SettingsTable, BuildDefaults) {
  SettingsBuild b = build();
  EXPECT_EQ(869.525f, settingDefault(settingInfo(SettingId::Freq), b));
  EXPECT_EQ(11.0f, settingDefault(settingInfo(SettingId::Sf), b));
  EXPECT_EQ(20.0f, settingDefault(settingInfo(SettingId::TxPower), b));
  EXPECT_EQ(1.0f, settingDefault(settingInfo(SettingId::RxBoostedGain), b));
  EXPECT_EQ(1.0f, settingDefault(settingInfo(SettingId::AirtimeFactor), b));
  EXPECT_EQ(1.0f, settingDefault(settingInfo(SettingId::CadEnabled), b));
  EXPECT_EQ(22.0f, settingMax(settingInfo(SettingId::TxPower), b));
  EXPECT_EQ(2500.0f, settingMax(settingInfo(SettingId::Freq), b));
}

TEST(SettingsTable, OffsetsMatchNodePrefs) {
  EXPECT_EQ((int)offsetof(NodePrefs, cad_enabled), settingInfo(SettingId::CadEnabled).offset);
  EXPECT_EQ((int)offsetof(NodePrefs, freq), settingInfo(SettingId::Freq).offset);
  EXPECT_EQ((int)offsetof(NodePrefs, ble_pin), settingInfo(SettingId::BlePin).offset);
  EXPECT_EQ(-1, settingInfo(SettingId::Lat).offset);
  EXPECT_EQ(-1, settingInfo(SettingId::Lon).offset);
}
