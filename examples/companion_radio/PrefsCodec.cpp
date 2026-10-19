#include "PrefsCodec.h"

#include <string.h>

namespace {

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

struct Writer {
  uint8_t* out;
  size_t pos;
  void put(const void* src, size_t n) {
    memcpy(out + pos, src, n);
    pos += n;
  }
  void zeros(size_t n) {
    memset(out + pos, 0, n);
    pos += n;
  }
};

}  // namespace

PrefsDecodeResult decodePrefs(const uint8_t* buf, size_t len, NodePrefs& p, double& lat, double& lon,
                              uint8_t rx_boost_default) {
  PrefsDecodeResult r = {false};
  Cursor c = {buf, buf ? len : 0, 0};
  uint8_t pad[4];

  c.take(&p.airtime_factor, sizeof(p.airtime_factor));
  c.take(p.node_name, sizeof(p.node_name));
  c.take(pad, sizeof(pad));
  c.take(&lat, sizeof(lat));
  c.take(&lon, sizeof(lon));
  c.take(&p.freq, sizeof(p.freq));
  c.take(&p.sf, 1);
  c.take(&p.cr, 1);
  c.take(&p.client_repeat, 1);
  c.take(&p.manual_add_contacts, 1);
  c.take(&p.bw, sizeof(p.bw));
  c.take(&p.tx_power_dbm, 1);
  c.take(&p.telemetry_mode_base, 1);
  c.take(&p.telemetry_mode_loc, 1);
  c.take(&p.telemetry_mode_env, 1);
  c.take(&p.rx_delay_base, sizeof(p.rx_delay_base));
  c.take(&p.advert_loc_policy, 1);
  c.take(&p.multi_acks, 1);

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

size_t encodePrefs(const NodePrefs& p, double lat, double lon, uint8_t* out, size_t cap) {
  if (out == nullptr || cap < PREFS_FILE_SIZE) return 0;
  Writer w = {out, 0};
  w.put(&p.airtime_factor, sizeof(p.airtime_factor));
  w.put(p.node_name, sizeof(p.node_name));
  w.zeros(4);
  w.put(&lat, sizeof(lat));
  w.put(&lon, sizeof(lon));
  w.put(&p.freq, sizeof(p.freq));
  w.put(&p.sf, 1);
  w.put(&p.cr, 1);
  w.put(&p.client_repeat, 1);
  w.put(&p.manual_add_contacts, 1);
  w.put(&p.bw, sizeof(p.bw));
  w.put(&p.tx_power_dbm, 1);
  w.put(&p.telemetry_mode_base, 1);
  w.put(&p.telemetry_mode_loc, 1);
  w.put(&p.telemetry_mode_env, 1);
  w.put(&p.rx_delay_base, sizeof(p.rx_delay_base));
  w.put(&p.advert_loc_policy, 1);
  w.put(&p.multi_acks, 1);
  w.put(&p.path_hash_mode, 1);
  w.zeros(1);
  w.put(&p.ble_pin, sizeof(p.ble_pin));
  w.put(&p.buzzer_quiet, 1);
  w.put(&p.gps_enabled, 1);
  w.put(&p.gps_interval, sizeof(p.gps_interval));
  w.put(&p.autoadd_config, 1);
  w.put(&p.autoadd_max_hops, 1);
  w.put(&p.rx_boosted_gain, 1);
  w.put(p.default_scope_name, sizeof(p.default_scope_name));
  w.put(p.default_scope_key, sizeof(p.default_scope_key));
  w.put(&p.cad_enabled, 1);
  return w.pos;
}
