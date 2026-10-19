#include "CompanionSettings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <helpers/TxtDataHelpers.h>

namespace {

inline uint32_t bit(SettingId id) { return 1u << (uint32_t)id; }

double readField(const NodePrefs& p, const SettingInfo& s) {
  const uint8_t* f = (const uint8_t*)&p + s.offset;
  switch (s.type) {
    case SettingType::Bool:
    case SettingType::U8: return *f;
    case SettingType::I8: return (int8_t)*f;
    case SettingType::U32:
    case SettingType::Pin: { uint32_t v; memcpy(&v, f, 4); return v; }
    case SettingType::Float: { float v; memcpy(&v, f, 4); return v; }
    default: return 0;
  }
}

void writeField(NodePrefs& p, const SettingInfo& s, double v) {
  uint8_t* f = (uint8_t*)&p + s.offset;
  switch (s.type) {
    case SettingType::Bool: *f = v != 0 ? 1 : 0; break;
    case SettingType::U8: *f = (uint8_t)v; break;
    case SettingType::I8: *f = (uint8_t)(int8_t)v; break;
    case SettingType::U32:
    case SettingType::Pin: { uint32_t x = (uint32_t)v; memcpy(f, &x, 4); break; }
    case SettingType::Float: { float x = (float)v; memcpy(f, &x, 4); break; }
    default: break;
  }
}

bool isNumeric(SettingType t) {
  return t == SettingType::Bool || t == SettingType::U8 || t == SettingType::I8 ||
         t == SettingType::U32 || t == SettingType::Float;
}

bool applierKind(ApplyKind a, SettingsApplier::Kind& k) {
  switch (a) {
    case ApplyKind::Radio: k = SettingsApplier::Radio; return true;
    case ApplyKind::TxPower: k = SettingsApplier::TxPower; return true;
    case ApplyKind::RxBoost: k = SettingsApplier::RxBoost; return true;
    case ApplyKind::Cad: k = SettingsApplier::Cad; return true;
    case ApplyKind::Gps: k = SettingsApplier::Gps; return true;
    default: return false;
  }
}

}  // namespace

CompanionSettings::CompanionSettings(const SettingsBuild& build)
    : _b(build), _store(nullptr), _applier(nullptr), _own_lat(0), _own_lon(0),
      _lat(&_own_lat), _lon(&_own_lon) {
  memset(&_p, 0, sizeof(_p));
}

void CompanionSettings::begin(PrefsStore& store, SettingsApplier& applier, double* lat, double* lon) {
  _store = &store;
  _applier = &applier;
  if (lat) _lat = lat;
  if (lon) _lon = lon;
}

void CompanionSettings::loadDefaults() {
  memset(&_p, 0, sizeof(_p));
  for (int i = 0; i < (int)SettingId::Count; i++) {
    const SettingInfo& s = settingInfo((SettingId)i);
    if (isNumeric(s.type)) writeField(_p, s, settingDefault(s, _b));
  }
  strcpy(_p.node_name, "NONAME");
}

void CompanionSettings::load() {
  if (_store) _store->load(_p, *_lat, *_lon);
  _p.node_name[sizeof(_p.node_name) - 1] = 0;
  // Only the ranges boot enforced before; stricter set-time rules wait for the next set.
  for (int i = 0; i < (int)SettingId::Count; i++) {
    const SettingInfo& s = settingInfo((SettingId)i);
    if (!s.sanitize_on_load || !isNumeric(s.type)) continue;
    double v = readField(_p, s);
    if (s.type == SettingType::Bool) {
      v = v != 0 ? 1 : 0;
    } else if (!isfinite(v)) {
      v = settingDefault(s, _b);
    } else {
      double hi = settingMax(s, _b);
      if (v < s.min) v = s.min;
      if (v > hi) v = hi;
    }
    writeField(_p, s, v);
  }
}

bool CompanionSettings::save() {
  return _store ? _store->save(_p, *_lat, *_lon) : false;
}

void CompanionSettings::applyAll() {
  if (!_applier) return;
  _applier->applyRadio(_p.freq, _p.bw, _p.sf, _p.cr);
  _applier->applyTxPower(_p.tx_power_dbm);
  if (_applier->supports(SettingsApplier::RxBoost)) _applier->applyRxBoost(_p.rx_boosted_gain != 0);
  _applier->applyCad(_p.cad_enabled != 0);
}

SetResult CompanionSettings::setInt(SettingId id, int32_t v, Origin origin) {
  return edit().setInt(id, v).commit(origin);
}
SetResult CompanionSettings::setFloat(SettingId id, float v, Origin origin) {
  return edit().setFloat(id, v).commit(origin);
}
SetResult CompanionSettings::setDouble(SettingId id, double v, Origin origin) {
  return edit().setDouble(id, v).commit(origin);
}
SetResult CompanionSettings::setText(SettingId id, const char* v, Origin origin) {
  return edit().setText(id, v).commit(origin);
}
SetResult CompanionSettings::setScope(const char* name, const uint8_t key[16]) {
  return edit().setScope(name, key).commit();
}

// ---- Edit ----

CompanionSettings::Edit::Edit(CompanionSettings& owner)
    : _owner(&owner), _pending(0), _name_len(0), _scope_len(0), _bad(false) {
  memset(_num, 0, sizeof(_num));
  _name[0] = 0;
  _scope_name[0] = 0;
  memset(_scope_key, 0, sizeof(_scope_key));
}

void CompanionSettings::Edit::stage(SettingId id, double v) {
  SettingType t = settingInfo(id).type;
  if (!isNumeric(t) && t != SettingType::Pin && t != SettingType::Double) {
    _bad = true;
    return;
  }
  _num[(int)id] = v;
  _pending |= bit(id);
}

CompanionSettings::Edit& CompanionSettings::Edit::setInt(SettingId id, int32_t v) { stage(id, v); return *this; }
CompanionSettings::Edit& CompanionSettings::Edit::setFloat(SettingId id, float v) { stage(id, v); return *this; }
CompanionSettings::Edit& CompanionSettings::Edit::setDouble(SettingId id, double v) { stage(id, v); return *this; }

CompanionSettings::Edit& CompanionSettings::Edit::setText(SettingId id, const char* v) {
  if (id != SettingId::NodeName || v == nullptr) {
    _bad = true;
    return *this;
  }
  _name_len = strlen(v);
  size_t n = _name_len < sizeof(_name) - 1 ? _name_len : sizeof(_name) - 1;
  memcpy(_name, v, n);
  _name[n] = 0;
  _pending |= bit(id);
  return *this;
}

CompanionSettings::Edit& CompanionSettings::Edit::setScope(const char* name, const uint8_t key[16]) {
  const char* s = name ? name : "";
  _scope_len = strlen(s);
  size_t n = _scope_len < sizeof(_scope_name) - 1 ? _scope_len : sizeof(_scope_name) - 1;
  memcpy(_scope_name, s, n);
  _scope_name[n] = 0;
  if (key) memcpy(_scope_key, key, sizeof(_scope_key));
  else memset(_scope_key, 0, sizeof(_scope_key));
  _pending |= bit(SettingId::DefaultScope);
  return *this;
}

SetResult CompanionSettings::Edit::commit(Origin origin) {
  CompanionSettings& o = *_owner;
  const SettingsBuild& b = o._b;
  if (_bad) return {SetResult::BadValue};

  NodePrefs next = o._p;
  double lat = *o._lat, lon = *o._lon;
  bool clamped = false;

  // 1. Validate every staged value into a copy.
  for (int i = 0; i < (int)SettingId::Count; i++) {
    SettingId id = (SettingId)i;
    if (!(_pending & bit(id))) continue;
    const SettingInfo& s = settingInfo(id);
    double v = _num[i];
    switch (s.type) {
      case SettingType::Text:
        if (_name_len < (size_t)s.min || _name_len > (size_t)s.max) return {SetResult::BadValue};
        if (b.name_ok && !b.name_ok(_name)) return {SetResult::BadValue};
        memset(next.node_name, 0, sizeof(next.node_name));
        memcpy(next.node_name, _name, _name_len);
        break;
      case SettingType::Scope:
        if (_scope_len > (size_t)s.max) return {SetResult::BadValue};
        memset(next.default_scope_name, 0, sizeof(next.default_scope_name));
        memcpy(next.default_scope_name, _scope_name, _scope_len);
        memcpy(next.default_scope_key, _scope_key, sizeof(next.default_scope_key));
        break;
      case SettingType::Pin:
        if (v != 0 && (v < s.min || v > s.max)) return {SetResult::BadValue};
        writeField(next, s, v);
        break;
      case SettingType::Double:
        if (!isfinite(v) || v < s.min || v > s.max) return {SetResult::BadValue};
        if (id == SettingId::Lat) lat = v; else lon = v;
        break;
      case SettingType::Bool:
        if (v != 0 && v != 1) return {SetResult::BadValue};
        writeField(next, s, v);
        break;
      default: {
        double hi = settingMax(s, b);
        if (!isfinite(v)) return {SetResult::BadValue};
        if (v < s.min || v > hi) {
          if (!s.clamp) return {SetResult::BadValue};
          v = v < s.min ? s.min : hi;
          clamped = true;
        }
        writeField(next, s, v);
        break;
      }
    }
  }

  // Client repeat is only allowed on the frequencies the app allows it on.
  if ((_pending & bit(SettingId::ClientRepeat)) && next.client_repeat && b.repeat_freq_ok &&
      !b.repeat_freq_ok((uint32_t)(next.freq * 1000.0f + 0.5f))) {
    return {SetResult::BadValue};
  }

  // 2. Which settings changed, and can this board apply them?
  uint32_t changed = 0;
  for (int i = 0; i < (int)SettingId::Count; i++) {
    SettingId id = (SettingId)i;
    if (!(_pending & bit(id))) continue;
    const SettingInfo& s = settingInfo(id);
    bool diff;
    switch (s.type) {
      case SettingType::Text: diff = strcmp(next.node_name, o._p.node_name) != 0; break;
      case SettingType::Scope:
        diff = memcmp(next.default_scope_name, o._p.default_scope_name, sizeof(next.default_scope_name)) != 0 ||
               memcmp(next.default_scope_key, o._p.default_scope_key, sizeof(next.default_scope_key)) != 0;
        break;
      case SettingType::Double: diff = (id == SettingId::Lat ? lat != *o._lat : lon != *o._lon); break;
      default: diff = readField(next, s) != readField(o._p, s); break;
    }
    if (!diff) continue;
    SettingsApplier::Kind k;
    if (o._applier && applierKind(s.apply, k) && !o._applier->supports(k)) return {SetResult::Unsupported};
    changed |= bit(id);
  }
  if (!changed) return {clamped ? SetResult::Clamped : SetResult::Ok};

  // 3. Store, apply what changed (fixed order), notify, save once.
  o._p = next;
  *o._lat = lat;
  *o._lon = lon;

  bool reboot = false;
  bool kinds[8] = {false};
  for (int i = 0; i < (int)SettingId::Count; i++) {
    if (!(changed & bit((SettingId)i))) continue;
    ApplyKind a = settingInfo((SettingId)i).apply;
    kinds[(int)a] = true;
    if (a == ApplyKind::Reboot) reboot = true;
  }
  if (o._applier) {
    SettingsApplier& ap = *o._applier;
    if (kinds[(int)ApplyKind::Radio]) {
      if (origin == Origin::Remote) reboot = true;
      else ap.applyRadio(next.freq, next.bw, next.sf, next.cr);
    }
    if (kinds[(int)ApplyKind::TxPower]) ap.applyTxPower(next.tx_power_dbm);
    if (kinds[(int)ApplyKind::RxBoost]) ap.applyRxBoost(next.rx_boosted_gain != 0);
    if (kinds[(int)ApplyKind::Cad]) ap.applyCad(next.cad_enabled != 0);
    if (kinds[(int)ApplyKind::Gps]) ap.applyGps(next.gps_enabled != 0, next.gps_interval);
    for (int i = 0; i < (int)SettingId::Count; i++) {
      if (changed & bit((SettingId)i)) ap.onChanged((SettingId)i);
    }
  } else if (kinds[(int)ApplyKind::Radio] && origin == Origin::Remote) {
    reboot = true;
  }
  o.save();

  if (clamped) return {SetResult::Clamped};
  return {reboot ? SetResult::OkReboot : SetResult::Ok};
}

// ---- Text front end ----

namespace {

// Copies value without surrounding spaces, tabs, CR or LF. False when it does not fit.
bool trimmed(const char* value, char* out, size_t len) {
  const char* a = value ? value : "";
  while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n') a++;
  size_t n = strlen(a);
  while (n && (a[n - 1] == ' ' || a[n - 1] == '\t' || a[n - 1] == '\r' || a[n - 1] == '\n')) n--;
  if (n >= len) return false;
  memcpy(out, a, n);
  out[n] = 0;
  return true;
}

bool parseLong(const char* s, long& v) {
  if (!*s) return false;
  char* end;
  v = strtol(s, &end, 10);
  return *end == 0;
}

bool parseDouble(const char* s, double& v) {
  if (!*s) return false;
  char* end;
  v = strtod(s, &end);
  return *end == 0;
}

void formatNumber(const SettingInfo& s, double v, char* out, size_t len) {
  if (s.type == SettingType::Float || s.type == SettingType::Double) snprintf(out, len, "%s", StrHelper::ftoa((float)v));
  else snprintf(out, len, "%ld", (long)v);
}

}  // namespace

void CompanionSettings::formatValue(const SettingInfo& s, char* out, size_t len) const {
  switch (s.type) {
    case SettingType::Bool: snprintf(out, len, "%s", readField(_p, s) != 0 ? "on" : "off"); break;
    case SettingType::Text: snprintf(out, len, "%s", _p.node_name); break;
    case SettingType::Scope: snprintf(out, len, "%s", _p.default_scope_name); break;
    case SettingType::Double: formatNumber(s, s.id == SettingId::Lat ? *_lat : *_lon, out, len); break;
    default: formatNumber(s, readField(_p, s), out, len); break;
  }
}

bool CompanionSettings::getByKey(const char* key, char* reply, size_t reply_len) const {
  const SettingInfo* s = findSettingByKey(key);
  if (!s) return false;
  char v[40];
  formatValue(*s, v, sizeof(v));
  snprintf(reply, reply_len, "> %s", v);
  return true;
}

bool CompanionSettings::setByKey(const char* key, const char* value, Origin origin, char* reply, size_t reply_len) {
  const SettingInfo* s = findSettingByKey(key);
  if (!s) return false;

  char v[40];
  SetResult r = {SetResult::BadValue};
  bool parsed = trimmed(value, v, sizeof(v));
  if (parsed) {
    long n;
    double d;
    switch (s->type) {
      case SettingType::Bool:
        if (strcmp(v, "on") == 0 || strcmp(v, "1") == 0) r = setInt(s->id, 1, origin);
        else if (strcmp(v, "off") == 0 || strcmp(v, "0") == 0) r = setInt(s->id, 0, origin);
        else {
          snprintf(reply, reply_len, "Error, must be on or off");
          return true;
        }
        break;
      case SettingType::Text: r = setText(s->id, v, origin); break;
      case SettingType::Float:
      case SettingType::Double:
        if (parseDouble(v, d)) r = setDouble(s->id, d, origin);
        break;
      case SettingType::Scope: break;   // not a CLI setting
      default:
        if (parseLong(v, n)) r = setInt(s->id, (int32_t)n, origin);
        break;
    }
  }

  switch (r.code) {
    case SetResult::Ok:
    case SetResult::OkReboot:
      if (s->ok_fmt) snprintf(reply, reply_len, s->ok_fmt, (unsigned)readField(_p, *s));
      else snprintf(reply, reply_len, "%s", r.code == SetResult::OkReboot ? "OK - reboot to apply" : "OK");
      break;
    case SetResult::Clamped: {
      char now[40];
      formatValue(*s, now, sizeof(now));
      snprintf(reply, reply_len, "OK - clamped to %s", now);
      break;
    }
    case SetResult::Unsupported:
      snprintf(reply, reply_len, "Error: unsupported");
      break;
    case SetResult::BadValue:
      if (s->type == SettingType::Bool) {
        snprintf(reply, reply_len, "Error, must be on or off");
      } else if (s->err_fmt && strstr(s->err_fmt, "%d")) {
        snprintf(reply, reply_len, s->err_fmt, (int)settingMax(*s, _b));
      } else if (s->err_fmt) {
        snprintf(reply, reply_len, "%s", s->err_fmt);
      } else {
        char lo[24], hi[24];
        formatNumber(*s, s->min, lo, sizeof(lo));
        formatNumber(*s, settingMax(*s, _b), hi, sizeof(hi));
        snprintf(reply, reply_len, "Error, must be %s to %s", lo, hi);
      }
      break;
  }
  return true;
}
