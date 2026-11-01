#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <helpers/TxtDataHelpers.h>

#include "settings_fakes.h"

namespace {

std::string set(SettingsFixture& f, const char* key, const char* value, Origin origin = Origin::Local) {
  char reply[96];
  memset(reply, 'X', sizeof(reply));
  reply[sizeof(reply) - 1] = 0;
  EXPECT_TRUE(f.settings.setByKey(key, value, origin, reply, sizeof(reply))) << key;
  return reply;
}

std::string get(SettingsFixture& f, const char* key) {
  char reply[96];
  EXPECT_TRUE(f.settings.getByKey(key, reply, sizeof(reply))) << key;
  return reply;
}

}  // namespace

TEST(SettingsText, SetTxOk) {
  SettingsFixture f;
  EXPECT_EQ("OK", set(f, "tx", "10"));
  EXPECT_EQ(10, f.settings.prefs().tx_power_dbm);
}

TEST(SettingsText, SetTxTooHigh) {
  SettingsFixture f;
  EXPECT_EQ("Error, must be -9 to 22", set(f, "tx", "30"));
}

TEST(SettingsText, SetAfTooHigh) {
  SettingsFixture f;
  EXPECT_EQ("ERROR: af must be 0-9", set(f, "af", "10"));
}

TEST(SettingsText, SetRxDelayBad) {
  SettingsFixture f;
  EXPECT_EQ("Error, must be 0-20", set(f, "rxdelay", "21"));
}

TEST(SettingsText, SetPathHashBad) {
  SettingsFixture f;
  EXPECT_EQ("Error, must be 0,1, or 2", set(f, "path.hash.mode", "3"));
}

TEST(SettingsText, SetPin) {
  SettingsFixture f;
  EXPECT_EQ("> pin is now 123456", set(f, "pin", "123456"));
  EXPECT_EQ("Error, pin must be 0 or 100000-999999", set(f, "pin", "12"));
}

TEST(SettingsText, SetNameBadChars) {
  SettingsFixture f;
  EXPECT_EQ("Error, bad chars", set(f, "name", "a[b"));
  EXPECT_EQ("OK", set(f, "name", "Bob"));
  EXPECT_EQ("> Bob", get(f, "name"));
}

TEST(SettingsText, CadOnOff) {
  SettingsFixture f;
  EXPECT_EQ("OK", set(f, "cad", "off"));
  EXPECT_EQ("> off", get(f, "cad"));
  EXPECT_EQ("Error, must be on or off", set(f, "cad", "maybe"));
  EXPECT_EQ("> off", get(f, "cad"));
}

TEST(SettingsText, GetFreqUsesFtoa) {
  SettingsFixture f;
  EXPECT_EQ(std::string("> ") + StrHelper::ftoa(869.525f), get(f, "freq"));
  EXPECT_EQ("> 20", get(f, "tx"));
  EXPECT_EQ("> 0", get(f, "multi.acks"));
}

TEST(SettingsText, RemoteFreqRebootText) {
  SettingsFixture f;
  EXPECT_EQ("OK - reboot to apply", set(f, "freq", "433.5", Origin::Remote));
  EXPECT_EQ(0, f.applier.count("radio"));
  EXPECT_EQ(433.5f, f.settings.prefs().freq);
}

TEST(SettingsText, UnknownKeyFalse) {
  SettingsFixture f;
  char reply[16] = "untouched";
  EXPECT_FALSE(f.settings.setByKey("manual.add", "1", Origin::Local, reply, sizeof(reply)));
  EXPECT_FALSE(f.settings.getByKey("nope", reply, sizeof(reply)));
  EXPECT_STREQ("untouched", reply);
}

TEST(SettingsText, TrailingWhitespaceTrimmed) {
  SettingsFixture f;
  EXPECT_EQ("OK", set(f, "tx", "20\r\n"));
  EXPECT_EQ("OK", set(f, "tx", " 12 "));
  EXPECT_EQ(12, f.settings.prefs().tx_power_dbm);
  EXPECT_EQ("OK", set(f, "cad", "on "));
  EXPECT_EQ("> on", get(f, "cad"));
}
