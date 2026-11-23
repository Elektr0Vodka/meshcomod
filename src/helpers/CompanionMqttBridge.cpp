// The two calls examples/companion_radio/CompanionMqtt.cpp makes straight into the
// vendored MQTTBridge. Kept in their own file: MQTTBridge.h pulls in CommonCLI.h,
// whose repeater NodePrefs clashes with the companion's NodePrefs in MyMesh.h.
// Lives under src/helpers so only the [mqtt_observer] envs compile it: the library
// finder ignores the #ifdef and would pull PsychicMqttClient into every companion.

#ifdef WITH_MQTT_BRIDGE

#include <helpers/bridges/MQTTBridge.h>

void companionMqttBridgeSetSlotCap(int cap) {
  MQTTBridge::setActiveSlotCap(cap);
}

int companionMqttBridgeSkippedPublishes() {
  return MQTTBridge::getSkippedPublishes();
}

#endif
