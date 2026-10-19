#include "PrefsCodec.h"

#include <stddef.h>
#include <string.h>

namespace {

// One entry per field in file order: where it sits in NodePrefs and how many bytes it has.
// PAD writes zeros and skips on read; LAT and LON are the position, kept outside NodePrefs.
enum : uint16_t { PAD = 0xFFFF, LAT = 0xFFFE, LON = 0xFFFD };
struct Field {
  uint16_t at;
  uint8_t size;
};
#define F(field) { (uint16_t)offsetof(NodePrefs, field), (uint8_t)sizeof(((NodePrefs*)0)->field) }

// Offsets 0-77, the same in every layout.
static const Field HEAD[] = {
  F(airtime_factor), F(node_name), {PAD, 4}, {LAT, 8}, {LON, 8}, F(freq), F(sf), F(cr),
  F(client_repeat), F(manual_add_contacts), F(bw), F(tx_power_dbm), F(telemetry_mode_base),
  F(telemetry_mode_loc), F(telemetry_mode_env), F(rx_delay_base), F(advert_loc_policy), F(multi_acks),
};
// Offsets 78-140 as written today (the upstream tail plus scope and CAD).
static const Field TAIL[] = {
  F(path_hash_mode), {PAD, 1}, F(ble_pin), F(buzzer_quiet), F(gps_enabled), F(gps_interval),
  F(autoadd_config), F(autoadd_max_hops), F(rx_boosted_gain), F(default_scope_name),
  F(default_scope_key), F(cad_enabled),
};
#undef F

uint8_t* fieldPtr(NodePrefs& p, double& lat, double& lon, uint16_t at) {
  if (at == LAT) return (uint8_t*)&lat;
  if (at == LON) return (uint8_t*)&lon;
  return (uint8_t*)&p + at;
}

// Sequential reader with File::read semantics: copies what is left and returns the count.
struct Cursor {
  const uint8_t* buf;
  size_t len;
  size_t pos;
  size_t take(void* dst, size_t n) {
    size_t k = pos >= len ? 0 : (len - pos < n ? len - pos : n);
    if (k) memcpy(dst, buf + pos, k);
    pos += k;
    return k;
  }
};

}  // namespace

PrefsDecodeResult decodePrefs(const uint8_t* buf, size_t len, NodePrefs& p, double& lat, double& lon,
                              uint8_t rx_boost_default) {
  PrefsDecodeResult r = {false};
  Cursor c = {buf, buf ? len : 0, 0};
  uint8_t pad[8];

  for (const Field& f : HEAD) {
    c.take(f.at == PAD ? pad : fieldPtr(p, lat, lon, f.at), f.size);
  }

  // From offset 78 the tail has three layouts, told apart by how many bytes are left.
  uint8_t tail[15];
  size_t n = c.take(tail, sizeof(tail));
  if (n == 13) {   // legacy meshcomod: no reserved pad, max_hops second, no rx boost
    p.path_hash_mode = tail[0];
    p.autoadd_max_hops = tail[1];
    memcpy(&p.ble_pin, tail + 2, 4);
    p.buzzer_quiet = tail[6];
    p.gps_enabled = tail[7];
    memcpy(&p.gps_interval, tail + 8, 4);
    p.autoadd_config = tail[12];
    p.rx_boosted_gain = rx_boost_default;
    r.needs_resave = true;
  } else if (n >= 14) {   // upstream: tail[1] reserved, rx boost optional
    p.path_hash_mode = tail[0];
    memcpy(&p.ble_pin, tail + 2, 4);
    p.buzzer_quiet = tail[6];
    p.gps_enabled = tail[7];
    memcpy(&p.gps_interval, tail + 8, 4);
    p.autoadd_config = tail[12];
    p.autoadd_max_hops = tail[13];
    p.rx_boosted_gain = n >= 15 ? (tail[14] ? 1 : 0) : rx_boost_default;
  }

  memset(p.default_scope_name, 0, sizeof(p.default_scope_name));
  memset(p.default_scope_key, 0, sizeof(p.default_scope_key));
  c.take(p.default_scope_name, sizeof(p.default_scope_name));
  c.take(p.default_scope_key, sizeof(p.default_scope_key));

  p.cad_enabled = 1;   // files written before the CAD toggle lack the byte
  uint8_t cad;
  if (c.take(&cad, 1) == 1) p.cad_enabled = cad ? 1 : 0;

  p.node_name[sizeof(p.node_name) - 1] = 0;
  return r;
}

static size_t putFields(const Field* table, size_t count, NodePrefs& p, double& lat, double& lon, uint8_t* out) {
  size_t pos = 0;
  for (size_t i = 0; i < count; i++) {
    const Field& f = table[i];
    if (f.at == PAD) memset(out + pos, 0, f.size);
    else memcpy(out + pos, fieldPtr(p, lat, lon, f.at), f.size);
    pos += f.size;
  }
  return pos;
}

size_t encodePrefs(const NodePrefs& p, double lat, double lon, uint8_t* out, size_t cap) {
  if (out == nullptr || cap < PREFS_FILE_SIZE) return 0;
  NodePrefs& q = const_cast<NodePrefs&>(p);   // fieldPtr is shared with decode; only read here
  size_t pos = putFields(HEAD, sizeof(HEAD) / sizeof(HEAD[0]), q, lat, lon, out);
  pos += putFields(TAIL, sizeof(TAIL) / sizeof(TAIL[0]), q, lat, lon, out + pos);
  return pos;
}
