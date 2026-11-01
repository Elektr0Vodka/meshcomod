#pragma once

#include <stddef.h>
#include <stdint.h>

#include "NodePrefs.h"
#include "SettingsPorts.h"
#include "SettingsTable.h"

struct SetResult {
  enum Code : uint8_t {
    Ok,           // stored (or already that value)
    OkReboot,     // stored; takes effect after a reboot
    Clamped,      // stored a clamped value instead of the one asked for
    BadValue,     // rejected, nothing stored
    Unsupported,  // this board cannot apply it, nothing stored
  } code;
};

// Local: the app, USB serial or the device itself. Remote: the CLI of a contact over the
// mesh, where a radio change is saved but not applied (the reply would go out on the new
// frequency and the admin would lose the node).
enum class Origin : uint8_t { Local, Remote };

// Owns the companion's settings: the NodePrefs values, their defaults, valid ranges,
// persistence and how a change is applied. Every change goes through set*() or an Edit:
// validate all, apply what changed, save once.
class CompanionSettings {
public:
  // Several settings changed together: all are validated first, and if any is rejected
  // nothing is stored, applied or saved.
  class Edit {
  public:
    Edit& setInt(SettingId id, int32_t v);
    Edit& setFloat(SettingId id, float v);
    Edit& setDouble(SettingId id, double v);
    Edit& setText(SettingId id, const char* v);
    Edit& setScope(const char* name, const uint8_t key[16]);
    SetResult commit(Origin origin = Origin::Local);

  private:
    friend class CompanionSettings;
    explicit Edit(CompanionSettings& owner);
    void stage(SettingId id, double v);

    CompanionSettings* _owner;
    uint32_t _pending;                    // bit per SettingId
    double _num[(int)SettingId::Count];
    char _name[33];                       // one more than fits, to catch too-long names
    size_t _name_len;
    char _scope_name[32];
    size_t _scope_len;
    uint8_t _scope_key[16];
    bool _bad;                            // a value that has no meaning for its setting
  };

  // storage: the NodePrefs instance the rest of the firmware reads (MyMesh keeps it).
  CompanionSettings(const SettingsBuild& build, NodePrefs& storage);

  void begin(PrefsStore& store, SettingsApplier& applier, double* lat, double* lon);

  const NodePrefs& prefs() const { return _p; }
  NodePrefs& editable() { return _p; }   // until every writer goes through the module
  const SettingInfo& describe(SettingId id) const { return settingInfo(id); }

  void loadDefaults();
  void load();          // read the file, then enforce the boot ranges
  bool save();
  void applyAll();      // at boot: radio, TX power, RX boost (if supported), CAD

  SetResult setInt(SettingId id, int32_t v, Origin origin = Origin::Local);
  SetResult setFloat(SettingId id, float v, Origin origin = Origin::Local);
  SetResult setDouble(SettingId id, double v, Origin origin = Origin::Local);
  SetResult setText(SettingId id, const char* v, Origin origin = Origin::Local);
  SetResult setScope(const char* name, const uint8_t key[16]);
  Edit edit() { return Edit(*this); }

  // Text front end for the CLIs: "set <key> <value>" and "get <key>". Both return false,
  // leaving reply untouched, when the key is unknown or not a CLI setting. Otherwise reply
  // holds the text to send back ("OK", "> 20", "Error, must be -9 to 20" ...).
  bool setByKey(const char* key, const char* value, Origin origin, char* reply, size_t reply_len);
  bool getByKey(const char* key, char* reply, size_t reply_len) const;

private:
  void formatValue(const SettingInfo& s, char* out, size_t len) const;

  SettingsBuild _b;
  NodePrefs& _p;
  PrefsStore* _store;
  SettingsApplier* _applier;
  double _own_lat, _own_lon;
  double* _lat;
  double* _lon;
};
