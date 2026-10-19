#include "FullCapture.h"

#include <string.h>

static int fcHeader(uint8_t* out, uint8_t tag, uint8_t index, uint8_t count, uint8_t status,
                    int8_t snr_x4, int8_t rssi, uint8_t total_len) {
  out[0] = FC_PUSH;
  out[1] = tag;
  out[2] = (uint8_t)((index << 4) | count);
  out[3] = status;
  out[4] = (uint8_t)snr_x4;
  out[5] = (uint8_t)rssi;
  out[6] = total_len;
  return FC_HEADER_LEN;
}

int fcEncodeParts(uint8_t tag, uint8_t status, int8_t snr_x4, int8_t rssi, const uint8_t* raw,
                  int len, uint8_t out[2][FC_FRAME_MAX], int out_len[2]) {
  if (raw == nullptr || len <= 0 || len > FC_RAW_MAX) return 0;
  const int count = len <= FC_CHUNK_MAX ? 1 : 2;
  int offset = 0;
  for (int index = 0; index < count; index++) {
    const int chunk = (len - offset) < FC_CHUNK_MAX ? (len - offset) : FC_CHUNK_MAX;
    const int head = fcHeader(out[index], tag, (uint8_t)index, (uint8_t)count, status, snr_x4,
                              rssi, (uint8_t)len);
    memcpy(&out[index][head], raw + offset, chunk);
    out_len[index] = head + chunk;
    offset += chunk;
  }
  return count;
}

int fcEncodeDropNotice(uint8_t tag, uint32_t dropped, int8_t snr_x4, int8_t rssi,
                       uint8_t out[FC_FRAME_MAX]) {
  const uint8_t capped = dropped > 255 ? 255 : (uint8_t)dropped;
  return fcHeader(out, tag, 0, 1, FC_STATUS_DROPS, snr_x4, rssi, capped);
}

uint8_t fcStatusForRadioError(int16_t err) {
  if (err == -7) return FC_STATUS_CRC;      // RADIOLIB_ERR_CRC_MISMATCH
  if (err == -24) return FC_STATUS_HEADER;  // RADIOLIB_ERR_LORA_HEADER_DAMAGED
  return FC_STATUS_READ_ERR;
}

bool fcNeedsLongPush(int len) {
  return len > FC_RAW_MAX_IN_0x88 && len <= FC_RAW_MAX;
}

bool fcFlagsValid(uint8_t flags) {
  return (flags & ~FC_SUPPORTED) == 0;
}
