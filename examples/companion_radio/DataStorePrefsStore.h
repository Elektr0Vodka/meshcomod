#pragma once

#include "DataStore.h"
#include "SettingsPorts.h"

// CompanionSettings' store in the firmware: the prefs file through DataStore.
class DataStorePrefsStore : public PrefsStore {
public:
  explicit DataStorePrefsStore(DataStore& store) : _store(store) {}

  bool load(NodePrefs& p, double& lat, double& lon) override {
    _store.loadPrefs(p, lat, lon);
    return true;
  }
  bool save(const NodePrefs& p, double lat, double lon) override {
    _store.savePrefs(p, lat, lon);
    return true;
  }

private:
  DataStore& _store;
};
