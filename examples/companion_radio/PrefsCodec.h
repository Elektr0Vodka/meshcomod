#pragma once

#include <stddef.h>
#include <stdint.h>

#include "NodePrefs.h"

// Byte layout of the companion prefs file (/new_prefs). Pure, so it runs in the native
// tests; DataStore does the file I/O around it. All values are little-endian.
//
//   0 airtime_factor f32     60 sf              72 rx_delay_base f32   86 gps_interval u32
//   4 node_name[32]          61 cr              76 advert_loc_policy   90 autoadd_config
//  36 pad[4]                 62 client_repeat   77 multi_acks          91 autoadd_max_hops
//  40 lat f64                63 manual_add      78 path_hash_mode      92 rx_boosted_gain
//  48 lon f64                64 bw f32          79 pad                 93 default_scope_name[31]
//  56 freq f32               68 tx_power_dbm    80 ble_pin u32        124 default_scope_key[16]
//                            69..71 telemetry   84 buzzer_quiet       140 cad_enabled
//                                               85 gps_enabled

static const size_t PREFS_FILE_SIZE = 141;
static const size_t PREFS_FILE_MAX = 192;   // read buffer; room for fields appended later

struct PrefsDecodeResult {
  bool needs_resave;   // the file used the 13-byte legacy meshcomod tail
};

// Decodes a prefs file of len bytes into p, lat and lon, the way the original loader read
// it field by field: a short file fills what it has and leaves the rest as it was. Older
// layouts: a 13-byte tail (legacy meshcomod) or a 14-byte tail (upstream, no rx boost) take
// rx_boost_default; a file without the trailing CAD byte loads with CAD on.
PrefsDecodeResult decodePrefs(const uint8_t* buf, size_t len, NodePrefs& p, double& lat, double& lon,
                              uint8_t rx_boost_default);

// Encodes p, lat and lon into out. Returns PREFS_FILE_SIZE, or 0 when cap is too small.
size_t encodePrefs(const NodePrefs& p, double lat, double lon, uint8_t* out, size_t cap);
