#include <gtest/gtest.h>

#include <string>

// Decisions the companion makes around the vendored MQTT observer: when to take
// Wi-Fi credentials from /mqtt.json, how many broker slots to allow, and which
// CLI commands belong to the observer. Pure functions, so they run on `native`.
#include "../../examples/companion_radio/CompanionMqttPolicy.h"

using namespace CompanionMqttPolicy;

// ---- shouldAdoptObserverWifi --------------------------------------------------

TEST(AdoptObserverWifi, AdoptsWhenCompanionHasNoSsid) {
  EXPECT_TRUE(shouldAdoptObserverWifi("", "home"));
}

TEST(AdoptObserverWifi, AdoptsWhenCompanionSsidIsNull) {
  EXPECT_TRUE(shouldAdoptObserverWifi(nullptr, "home"));
}

TEST(AdoptObserverWifi, KeepsTheCompanionsOwnSsid) {
  EXPECT_FALSE(shouldAdoptObserverWifi("home", "other"));
}

TEST(AdoptObserverWifi, NothingToAdoptFromAnEmptyObserverSsid) {
  EXPECT_FALSE(shouldAdoptObserverWifi("", ""));
  EXPECT_FALSE(shouldAdoptObserverWifi("", nullptr));
}

TEST(AdoptObserverWifi, RefusesAnSsidTheCompanionStoreCannotHold) {
  const std::string max_fit(31, 'a');
  const std::string too_long(32, 'a');
  EXPECT_TRUE(shouldAdoptObserverWifi("", max_fit.c_str()));
  EXPECT_FALSE(shouldAdoptObserverWifi("", too_long.c_str()));
}

// ---- slotCap ------------------------------------------------------------------

TEST(SlotCap, PsramBoardsKeepTheBridgeDefault) {
  EXPECT_EQ(0, slotCap(true, true));
  EXPECT_EQ(0, slotCap(true, false));
}

TEST(SlotCap, NoPsramWithBleOnAllowsOneSlot) {
  EXPECT_EQ(1, slotCap(false, true));
}

TEST(SlotCap, NoPsramWithBleOffAllowsTwoSlots) {
  EXPECT_EQ(2, slotCap(false, false));
}

// ---- isObserverCommand --------------------------------------------------------

TEST(ObserverCommand, MqttSettingsGoToTheObserver) {
  EXPECT_TRUE(isObserverCommand("set mqtt1.preset analyzer-eu"));
  EXPECT_TRUE(isObserverCommand("get mqtt.status"));
  EXPECT_TRUE(isObserverCommand("get mqtt3.diag"));
  EXPECT_TRUE(isObserverCommand("set mqtt.iata AMS"));
}

TEST(ObserverCommand, TimezoneAndLinkStatusGoToTheObserver) {
  EXPECT_TRUE(isObserverCommand("set timezone Europe/Amsterdam"));
  EXPECT_TRUE(isObserverCommand("get timezone"));
  EXPECT_TRUE(isObserverCommand("get link.status"));
}

TEST(ObserverCommand, WifiOtaAndSnmpStayWithTheCompanion) {
  EXPECT_FALSE(isObserverCommand("set wifi.ssid x"));
  EXPECT_FALSE(isObserverCommand("get wifi.txpower"));
  EXPECT_FALSE(isObserverCommand("ota check"));
  EXPECT_FALSE(isObserverCommand("get snmp"));
}

TEST(ObserverCommand, LookalikeKeysAreNotObserverCommands) {
  EXPECT_FALSE(isObserverCommand("set timezones x"));
  EXPECT_FALSE(isObserverCommand("get link.statusx"));
  EXPECT_FALSE(isObserverCommand("mqtt.status"));
  EXPECT_FALSE(isObserverCommand("set mqt"));
  EXPECT_FALSE(isObserverCommand(""));
  EXPECT_FALSE(isObserverCommand(nullptr));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
