#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <vector>

// The /new_prefs byte layout of the companion. legacySave / legacyLoad are copies of
// DataStore::savePrefs / loadPrefsInt as they were before PrefsCodec, over a byte
// vector instead of a File, and serve as the reference the codec must match.
#include "../../examples/companion_radio/PrefsCodec.h"

namespace {

typedef std::vector<uint8_t> Bytes;

void put(Bytes& b, const void* p, size_t n) {
  const uint8_t* s = (const uint8_t*)p;
  b.insert(b.end(), s, s + n);
}

Bytes legacySave(const NodePrefs& p, double lat, double lon, bool with_gps_interval = true) {
  Bytes b;
  uint8_t pad[8] = {0};
  put(b, &p.airtime_factor, 4);
  put(b, p.node_name, sizeof(p.node_name));
  put(b, pad, 4);
  put(b, &lat, 8);
  put(b, &lon, 8);
  put(b, &p.freq, 4);
  put(b, &p.sf, 1);
  put(b, &p.cr, 1);
  put(b, &p.client_repeat, 1);
  put(b, &p.manual_add_contacts, 1);
  put(b, &p.bw, 4);
  put(b, &p.tx_power_dbm, 1);
  put(b, &p.telemetry_mode_base, 1);
  put(b, &p.telemetry_mode_loc, 1);
  put(b, &p.telemetry_mode_env, 1);
  put(b, &p.rx_delay_base, 4);
  put(b, &p.advert_loc_policy, 1);
  put(b, &p.multi_acks, 1);
  put(b, &p.path_hash_mode, 1);
  put(b, pad, 1);
  put(b, &p.ble_pin, 4);
  put(b, &p.buzzer_quiet, 1);
  put(b, &p.gps_enabled, 1);
  if (with_gps_interval) put(b, &p.gps_interval, 4);
  put(b, &p.autoadd_config, 1);
  put(b, &p.autoadd_max_hops, 1);
  put(b, &p.rx_boosted_gain, 1);
  put(b, p.default_scope_name, sizeof(p.default_scope_name));
  put(b, p.default_scope_key, sizeof(p.default_scope_key));
  put(b, &p.cad_enabled, 1);
  return b;
}

// File::read semantics: copies what is left, returns the count.
struct Reader {
  const Bytes& b;
  size_t pos;
  size_t read(void* dst, size_t n) {
    size_t k = pos >= b.size() ? 0 : std::min(n, b.size() - pos);
    memcpy(dst, b.data() + pos, k);
    pos += k;
    return k;
  }
};

bool legacyLoad(const Bytes& file, NodePrefs& p, double& lat, double& lon, uint8_t rx_default) {
  Reader f{file, 0};
  uint8_t pad[8];
  f.read(&p.airtime_factor, 4);
  f.read(p.node_name, sizeof(p.node_name));
  f.read(pad, 4);
  f.read(&lat, 8);
  f.read(&lon, 8);
  f.read(&p.freq, 4);
  f.read(&p.sf, 1);
  f.read(&p.cr, 1);
  f.read(&p.client_repeat, 1);
  f.read(&p.manual_add_contacts, 1);
  f.read(&p.bw, 4);
  f.read(&p.tx_power_dbm, 1);
  f.read(&p.telemetry_mode_base, 1);
  f.read(&p.telemetry_mode_loc, 1);
  f.read(&p.telemetry_mode_env, 1);
  f.read(&p.rx_delay_base, 4);
  f.read(&p.advert_loc_policy, 1);
  f.read(&p.multi_acks, 1);
  uint8_t tail[15];
  size_t n = f.read(tail, sizeof(tail));
  bool migrated = false;
  if (n == 13) {
    p.path_hash_mode = tail[0];
    p.autoadd_max_hops = tail[1];
    memcpy(&p.ble_pin, tail + 2, 4);
    p.buzzer_quiet = tail[6];
    p.gps_enabled = tail[7];
    memcpy(&p.gps_interval, tail + 8, 4);
    p.autoadd_config = tail[12];
    p.rx_boosted_gain = rx_default;
    migrated = true;
  } else if (n >= 14) {
    p.path_hash_mode = tail[0];
    memcpy(&p.ble_pin, tail + 2, 4);
    p.buzzer_quiet = tail[6];
    p.gps_enabled = tail[7];
    memcpy(&p.gps_interval, tail + 8, 4);
    p.autoadd_config = tail[12];
    p.autoadd_max_hops = tail[13];
    p.rx_boosted_gain = n >= 15 ? (tail[14] ? 1 : 0) : rx_default;
  }
  memset(p.default_scope_name, 0, sizeof(p.default_scope_name));
  memset(p.default_scope_key, 0, sizeof(p.default_scope_key));
  f.read(p.default_scope_name, sizeof(p.default_scope_name));
  f.read(p.default_scope_key, sizeof(p.default_scope_key));
  p.cad_enabled = 1;
  uint8_t cad;
  if (f.read(&cad, 1) == 1) p.cad_enabled = cad ? 1 : 0;
  return migrated;
}

NodePrefs zeroed() {
  NodePrefs p;
  memset(&p, 0, sizeof(p));
  return p;
}

NodePrefs sample() {
  NodePrefs p = zeroed();
  p.airtime_factor = 2.5f;
  strcpy(p.node_name, "Alpha");
  p.freq = 869.525f;
  p.sf = 11;
  p.cr = 6;
  p.multi_acks = 1;
  p.manual_add_contacts = 1;
  p.bw = 62.5f;
  p.tx_power_dbm = 17;
  p.telemetry_mode_base = 1;
  p.telemetry_mode_loc = 2;
  p.telemetry_mode_env = 1;
  p.rx_delay_base = 3.25f;
  p.ble_pin = 123456;
  p.advert_loc_policy = 1;
  p.buzzer_quiet = 1;
  p.gps_enabled = 1;
  p.gps_interval = 3600;
  p.autoadd_config = 0x5A;
  p.rx_boosted_gain = 1;
  p.client_repeat = 1;
  p.path_hash_mode = 2;
  p.autoadd_max_hops = 7;
  strcpy(p.default_scope_name, "#nl");
  for (int i = 0; i < 16; i++) p.default_scope_key[i] = (uint8_t)(0xA0 + i);
  p.cad_enabled = 0;
  return p;
}

void expectSame(const NodePrefs& a, const NodePrefs& b) {
  EXPECT_STREQ(a.node_name, b.node_name);
  EXPECT_EQ(a.freq, b.freq);
  EXPECT_EQ(a.ble_pin, b.ble_pin);
  EXPECT_EQ(a.autoadd_max_hops, b.autoadd_max_hops);
  EXPECT_EQ(a.rx_boosted_gain, b.rx_boosted_gain);
  EXPECT_EQ(a.cad_enabled, b.cad_enabled);
  EXPECT_EQ(0, memcmp(&a, &b, sizeof(NodePrefs)));
}

}  // namespace

TEST(PrefsCodec, EncodeMatchesLegacySaveByteForByte) {
  uint8_t buf[PREFS_FILE_MAX];
  ASSERT_EQ(PREFS_FILE_SIZE, encodePrefs(sample(), 52.1, 4.3, buf, sizeof(buf)));
  Bytes ref = legacySave(sample(), 52.1, 4.3);
  ASSERT_EQ(PREFS_FILE_SIZE, ref.size());
  EXPECT_EQ(0, memcmp(ref.data(), buf, PREFS_FILE_SIZE));
}

TEST(PrefsCodec, EncodeOffsets) {
  uint8_t buf[PREFS_FILE_MAX];
  memset(buf, 0xEE, sizeof(buf));
  NodePrefs p = sample();
  encodePrefs(p, 52.1, 4.3, buf, sizeof(buf));
  EXPECT_EQ(p.sf, buf[60]);
  EXPECT_EQ(0, memcmp(&buf[80], &p.ble_pin, 4));
  EXPECT_EQ(0, buf[79]);
  EXPECT_EQ(0, buf[140]);
  EXPECT_EQ(0, memcmp(&buf[56], &p.freq, 4));
  EXPECT_EQ(0, memcmp(&buf[124], p.default_scope_key, 16));
}

TEST(PrefsCodec, EncodeRejectsSmallBuffer) {
  uint8_t buf[PREFS_FILE_MAX];
  EXPECT_EQ(0u, encodePrefs(sample(), 0, 0, buf, PREFS_FILE_SIZE - 1));
}

TEST(PrefsCodec, RoundTripFull141) {
  uint8_t buf[PREFS_FILE_MAX];
  size_t n = encodePrefs(sample(), 52.1, 4.3, buf, sizeof(buf));
  NodePrefs got = zeroed();
  double lat = 0, lon = 0;
  PrefsDecodeResult r = decodePrefs(buf, n, got, lat, lon, 0);
  EXPECT_FALSE(r.needs_resave);
  expectSame(sample(), got);
  EXPECT_EQ(52.1, lat);
  EXPECT_EQ(4.3, lon);
}

TEST(PrefsCodec, Legacy91ByteFileMigrates) {
  NodePrefs p = sample();
  Bytes b = legacySave(p, 1, 2);
  b.resize(78);
  uint8_t tail[13] = {2, 9, 0x40, 0xE2, 0x01, 0x00, 1, 1, 0x10, 0x0E, 0, 0, 0x33};
  put(b, tail, sizeof(tail));
  ASSERT_EQ(91u, b.size());
  NodePrefs got = zeroed();
  double lat = 0, lon = 0;
  PrefsDecodeResult r = decodePrefs(b.data(), b.size(), got, lat, lon, 1);
  EXPECT_TRUE(r.needs_resave);
  EXPECT_EQ(9, got.autoadd_max_hops);
  EXPECT_EQ(123456u, got.ble_pin);
  EXPECT_EQ(3600u, got.gps_interval);
  EXPECT_EQ(0x33, got.autoadd_config);
  EXPECT_EQ(1, got.rx_boosted_gain);
  EXPECT_EQ(1, got.cad_enabled);
  EXPECT_EQ(0, got.default_scope_name[0]);
  NodePrefs ref = zeroed();
  double rl = 0, ro = 0;
  EXPECT_TRUE(legacyLoad(b, ref, rl, ro, 1));
  expectSame(ref, got);
}

TEST(PrefsCodec, Upstream92ByteFileDefaultsRxBoost) {
  Bytes b = legacySave(sample(), 1, 2);
  b.resize(92);
  NodePrefs got = zeroed();
  double lat = 0, lon = 0;
  PrefsDecodeResult r = decodePrefs(b.data(), b.size(), got, lat, lon, 1);
  EXPECT_FALSE(r.needs_resave);
  EXPECT_EQ(1, got.rx_boosted_gain);
  EXPECT_EQ(7, got.autoadd_max_hops);
  EXPECT_EQ(1, got.cad_enabled);
}

TEST(PrefsCodec, FileWithoutCadByteDefaultsOn) {
  Bytes b = legacySave(sample(), 1, 2);
  b.resize(140);
  NodePrefs got = zeroed();
  double lat = 0, lon = 0;
  decodePrefs(b.data(), b.size(), got, lat, lon, 0);
  EXPECT_EQ(1, got.cad_enabled);
  EXPECT_STREQ("#nl", got.default_scope_name);
}

TEST(PrefsCodec, ExtraTrailingBytesIgnored) {
  Bytes b = legacySave(sample(), 1, 2);
  NodePrefs base = zeroed();
  double l1 = 0, o1 = 0;
  decodePrefs(b.data(), b.size(), base, l1, o1, 0);
  b.push_back(0x01);
  b.push_back(0xFF);
  NodePrefs got = zeroed();
  double l2 = 0, o2 = 0;
  PrefsDecodeResult r = decodePrefs(b.data(), b.size(), got, l2, o2, 0);
  EXPECT_FALSE(r.needs_resave);
  expectSame(base, got);
}

TEST(PrefsCodec, Decode137ByteFileMatchesLegacyLoader) {
  Bytes b = legacySave(sample(), 52.1, 4.3, false);
  ASSERT_EQ(137u, b.size());
  NodePrefs got = zeroed(), ref = zeroed();
  double gl = 0, go = 0, rl = 0, ro = 0;
  PrefsDecodeResult r = decodePrefs(b.data(), b.size(), got, gl, go, 1);
  bool migrated = legacyLoad(b, ref, rl, ro, 1);
  EXPECT_EQ(migrated, r.needs_resave);
  expectSame(ref, got);
  EXPECT_EQ(rl, gl);
  EXPECT_EQ(ro, go);
}

// An empty file decodes as the old loader did: nothing read, but the scope is zeroed and
// CAD defaults on. Everything else keeps its value.
TEST(PrefsCodec, EmptyBufferLeavesPrefsUntouched) {
  NodePrefs got = sample(), ref = sample();
  double gl = 7, go = 8, rl = 7, ro = 8;
  Bytes empty;
  PrefsDecodeResult r = decodePrefs(nullptr, 0, got, gl, go, 1);
  legacyLoad(empty, ref, rl, ro, 1);
  EXPECT_FALSE(r.needs_resave);
  expectSame(ref, got);
  EXPECT_EQ(7, gl);
  EXPECT_EQ(8, go);
  EXPECT_EQ(2.5f, got.airtime_factor);
}

TEST(PrefsCodec, NameAlwaysTerminated) {
  Bytes b = legacySave(sample(), 1, 2);
  memset(&b[4], 'A', 32);
  NodePrefs got = zeroed();
  double lat = 0, lon = 0;
  decodePrefs(b.data(), b.size(), got, lat, lon, 0);
  EXPECT_EQ(0, got.node_name[31]);
  EXPECT_EQ(31u, strlen(got.node_name));
}
