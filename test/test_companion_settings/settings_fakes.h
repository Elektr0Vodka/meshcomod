#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../examples/companion_radio/CompanionSettings.h"

// Test stand-ins for the two CompanionSettings ports, shared by the settings test files.

struct FakeApplier : SettingsApplier {
  bool support[5] = {true, true, true, true, true};
  std::vector<std::string> calls;
  std::vector<SettingId> changed;

  bool supports(Kind k) const override { return support[k]; }
  bool applyRadio(float freq, float bw, uint8_t sf, uint8_t cr) override {
    char b[64];
    snprintf(b, sizeof(b), "radio %g %g %u %u", freq, bw, (unsigned)sf, (unsigned)cr);
    calls.push_back(b);
    return true;
  }
  bool applyTxPower(int8_t dbm) override { calls.push_back("tx " + std::to_string(dbm)); return true; }
  bool applyRxBoost(bool on) override { calls.push_back(on ? "rxboost on" : "rxboost off"); return true; }
  bool applyCad(bool on) override { calls.push_back(on ? "cad on" : "cad off"); return true; }
  bool applyGps(bool on, uint32_t interval) override {
    calls.push_back(std::string(on ? "gps on " : "gps off ") + std::to_string(interval));
    return true;
  }
  void onChanged(SettingId id) override { changed.push_back(id); }

  int count(const std::string& prefix) const {
    int n = 0;
    for (const std::string& c : calls) if (c.compare(0, prefix.size(), prefix) == 0) n++;
    return n;
  }
};

struct FakeStore : PrefsStore {
  NodePrefs stored;
  double stored_lat = 0, stored_lon = 0;
  bool has_file = false;
  int saves = 0;

  bool load(NodePrefs& p, double& lat, double& lon) override {
    if (!has_file) return false;
    p = stored;
    lat = stored_lat;
    lon = stored_lon;
    return true;
  }
  bool save(const NodePrefs& p, double lat, double lon) override {
    stored = p;
    stored_lat = lat;
    stored_lon = lon;
    saves++;
    return true;
  }
};

inline bool testNameOk(const char* n) { return strchr(n, '[') == nullptr; }
inline bool testRepeatOk(uint32_t khz) { return khz >= 433000 && khz <= 434000; }

inline SettingsBuild testBuild() {
  SettingsBuild b;
  b.freq = 869.525f;
  b.bw = 250;
  b.sf = 11;
  b.cr = 5;
  b.tx_power = 20;
  b.max_tx_power = 22;
  b.rx_boost_default = 1;
  b.name_ok = testNameOk;
  b.repeat_freq_ok = testRepeatOk;
  return b;
}

// A started module on defaults, with the fakes attached.
struct SettingsFixture {
  FakeApplier applier;
  FakeStore store;
  double lat = 0, lon = 0;
  CompanionSettings settings{testBuild()};

  SettingsFixture() {
    settings.loadDefaults();
    settings.begin(store, applier, &lat, &lon);
  }
  // Store what a file would hold: defaults with edits applied by the caller.
  NodePrefs defaults() const {
    CompanionSettings s{testBuild()};
    s.loadDefaults();
    return s.prefs();
  }
};
