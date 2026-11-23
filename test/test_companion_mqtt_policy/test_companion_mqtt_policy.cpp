#include <gtest/gtest.h>

#include <string>

// Decisions the companion makes around the vendored MQTT observer: when to take
// Wi-Fi credentials from /mqtt.json, how many broker slots to allow, and which
// CLI commands belong to the observer. Pure functions, so they run on `native`.
#include "../../examples/companion_radio/CompanionMqttPolicy.h"

using namespace CompanionMqttPolicy;

// ---- shouldAdoptObserverWifi --------------------------------------------------

TEST(AdoptObserverWifi, AdoptsWhenCompanionHasNoSsid) {
  EXPECT_TRUE(shouldAdoptObserverWifi("", "home", true));
}

TEST(AdoptObserverWifi, AdoptsWhenCompanionSsidIsNull) {
  EXPECT_TRUE(shouldAdoptObserverWifi(nullptr, "home", true));
}

TEST(AdoptObserverWifi, KeepsTheCompanionsOwnSsid) {
  EXPECT_FALSE(shouldAdoptObserverWifi("home", "other", true));
}

TEST(AdoptObserverWifi, NothingToAdoptFromAnEmptyObserverSsid) {
  EXPECT_FALSE(shouldAdoptObserverWifi("", "", true));
  EXPECT_FALSE(shouldAdoptObserverWifi("", nullptr, true));
}

TEST(AdoptObserverWifi, RefusesAnSsidTheCompanionStoreCannotHold) {
  const std::string max_fit(31, 'a');
  const std::string too_long(32, 'a');
  EXPECT_TRUE(shouldAdoptObserverWifi("", max_fit.c_str(), true));
  EXPECT_FALSE(shouldAdoptObserverWifi("", too_long.c_str(), true));
}

TEST(AdoptObserverWifi, OnlyOnTheFirstBootOfAnMqttImage) {
  // Later boots never adopt, so a companion whose Wi-Fi was cleared on purpose
  // (wifi.clear, BLE only) does not get the DMC network back.
  EXPECT_FALSE(shouldAdoptObserverWifi("", "home", false));
  EXPECT_FALSE(shouldAdoptObserverWifi(nullptr, "home", false));
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

// ---- liveBleStartAllowed -----------------------------------------------------

TEST(LiveBleStart, PsramBoardsAlwaysAllowIt) {
  EXPECT_TRUE(liveBleStartAllowed(true, 5));
}

TEST(LiveBleStart, NoPsramAllowsItNextToOneBrokerSlot) {
  EXPECT_TRUE(liveBleStartAllowed(false, 0));
  EXPECT_TRUE(liveBleStartAllowed(false, 1));
}

TEST(LiveBleStart, NoPsramRefusesItNextToTwoBrokerSlots) {
  // The slot budget for BLE on is one; two TLS sessions plus NimBLE do not fit.
  EXPECT_FALSE(liveBleStartAllowed(false, 2));
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
