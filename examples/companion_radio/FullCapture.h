#pragma once

#include <stdint.h>

// Full capture: on request (CMD 0xE0) the companion also hands the app the
// frames it normally drops, so an app such as RTFM-EV can act as a complete
// observer:
//
//   - over-long frames (174-255 bytes) that do not fit the 0x88 RX log push
//   - frames the radio received with a CRC or header error
//   - counts of frames lost to a full receive queue
//
// They go out as PUSH 0xC0, split in two parts when they do not fit one
// frame (BLE and the other transports carry at most 176 bytes):
//
//   [0xC0][tag][index << 4 | count][status][snr_x4 i8][rssi i8][total_len][chunk]
//
// The setting lives in RAM only and is cleared on reboot and when no app is
// connected, so the app sends it again on every connect. Nothing here touches
// the radio: it only formats bytes, so it runs on `native`.

constexpr uint8_t FC_CMD = 0xE0;   // [0xE0][flags]; reply [RESP_CODE_OK][supported flags]
constexpr uint8_t FC_PUSH = 0xC0;

constexpr uint8_t FC_FLAG_LONG = 0x01;   // over-long valid frames
constexpr uint8_t FC_FLAG_BAD = 0x02;    // CRC / header-error frames
constexpr uint8_t FC_FLAG_DROPS = 0x04;  // drop notices
constexpr uint8_t FC_SUPPORTED = FC_FLAG_LONG | FC_FLAG_BAD | FC_FLAG_DROPS;

constexpr uint8_t FC_STATUS_OK = 0;        // valid, only too long for 0x88
constexpr uint8_t FC_STATUS_CRC = 1;       // RadioLib -7 (CRC mismatch)
constexpr uint8_t FC_STATUS_HEADER = 2;    // RadioLib -24 (header damaged)
constexpr uint8_t FC_STATUS_READ_ERR = 3;  // any other read error
constexpr uint8_t FC_STATUS_DROPS = 4;     // drop notice: total_len = frames lost

constexpr int FC_HEADER_LEN = 7;
constexpr int FC_FRAME_MAX = 176;  // MAX_FRAME_SIZE of the companion transports
constexpr int FC_CHUNK_MAX = FC_FRAME_MAX - FC_HEADER_LEN;  // 169
constexpr int FC_RAW_MAX_IN_0x88 = FC_FRAME_MAX - 3;       // 173
constexpr int FC_RAW_MAX = 255;

// Encode one captured frame as one or two pushes. Returns the part count, or 0
// when the frame cannot be encoded (no bytes, longer than 255 bytes).
int fcEncodeParts(uint8_t tag, uint8_t status, int8_t snr_x4, int8_t rssi, const uint8_t* raw,
                  int len, uint8_t out[2][FC_FRAME_MAX], int out_len[2]);

// A drop notice: `dropped` frames lost since the previous notice (capped at 255).
// Returns the frame length (7).
int fcEncodeDropNotice(uint8_t tag, uint32_t dropped, int8_t snr_x4, int8_t rssi,
                       uint8_t out[FC_FRAME_MAX]);

uint8_t fcStatusForRadioError(int16_t err);

// True when a valid frame is too long for the 0x88 push and needs 0xC0.
bool fcNeedsLongPush(int len);

// True when `flags` only uses bits this firmware knows.
bool fcFlagsValid(uint8_t flags);
