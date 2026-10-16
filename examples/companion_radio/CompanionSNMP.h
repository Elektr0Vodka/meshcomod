#pragma once

#include <stdint.h>

// Optional SNMPv2c agent for the multi-transport (Wi-Fi) companion, built only
// with -D WITH_SNMP=1. Read-only GET / GETNEXT / GETBULK on UDP 161.
//
// Off by default at runtime: until `set snmp on` the agent is never allocated, so
// a WITH_SNMP build that leaves it off costs flash but no RAM. When on, it runs in
// the main loop whenever Wi-Fi holds an address and stops while the link is down.
// Settings live in NVS (WifiRuntimeStore), not NodePrefs, and apply live.
//
// The OID tree matches the observer firmware's agent (Dutch-MeshCore
// dmc-observer-dev, MQTT_SNMP.md) so one poller reads both. See
// docs/companion_snmp.md for the full list.

#ifdef WITH_SNMP

// Main loop. link_up: Wi-Fi associated with an address.
void companionSnmpLoop(bool link_up);

// SNTP has set the clock (.3.4.0).
void companionSnmpSetNtpSynced();

// `get snmp`, `set snmp on|off`, `get snmp.status`, `get|set snmp.community`.
// remote: the request came over the mesh (remote CLI), which never gets the
// community string back. Returns false when the command is not an SNMP one.
bool companionSnmpCommand(const char* command, bool remote, char* reply);

#endif
