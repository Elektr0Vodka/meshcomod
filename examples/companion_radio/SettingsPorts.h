#pragma once

#include <stdint.h>

#include "NodePrefs.h"
#include "SettingsTable.h"

// The two seams of CompanionSettings. In the firmware MyMesh is the applier and DataStore
// (through DataStorePrefsStore) the store; the native tests use fakes.

// Puts a changed setting into effect on the hardware.
struct SettingsApplier {
  enum Kind : uint8_t { Radio, TxPower, RxBoost, Cad, Gps };
  virtual bool supports(Kind kind) const = 0;   // checked before a change is accepted
  virtual bool applyRadio(float freq, float bw, uint8_t sf, uint8_t cr) = 0;
  virtual bool applyTxPower(int8_t dbm) = 0;
  virtual bool applyRxBoost(bool on) = 0;
  virtual bool applyCad(bool on) = 0;
  virtual bool applyGps(bool on, uint32_t interval_s) = 0;
  virtual void onChanged(SettingId id) = 0;     // after a setting changed, for the UI
  virtual ~SettingsApplier() {}
};

// Reads and writes the prefs file (NodePrefs plus the advert position).
struct PrefsStore {
  virtual bool load(NodePrefs& p, double& lat, double& lon) = 0;
  virtual bool save(const NodePrefs& p, double lat, double lon) = 0;
  virtual ~PrefsStore() {}
};
