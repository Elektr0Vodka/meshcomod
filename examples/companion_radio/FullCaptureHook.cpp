// Full capture: hand the app the frames this companion normally drops (see
// FullCapture.h for the wire format). Formatting lives in FullCapture.cpp so it
// is tested on `native`; this file only decides when to push. Nothing here
// transmits on RF.

#include "MyMesh.h"

// Same values as MyMesh.cpp, which keeps the protocol codes to itself.
#define RESP_CODE_OK          0
#define ERR_CODE_ILLEGAL_ARG  6

// Drop notices need the ESP32 receive queue; other boards do not count drops.
#if defined(ESP32)
static const uint8_t FC_BOARD_FLAGS = FC_SUPPORTED;
#else
static const uint8_t FC_BOARD_FLAGS = FC_FLAG_LONG | FC_FLAG_BAD;
#endif

static const unsigned long FC_NOTICE_INTERVAL_MS = 1000;

void MyMesh::fcCommand(uint8_t flags) {
  if (!fcFlagsValid(flags)) {
    writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    return;
  }
  flags &= FC_BOARD_FLAGS;
  if ((flags & FC_FLAG_DROPS) && !(_fc_flags & FC_FLAG_DROPS)) {
    // Only report frames lost from now on.
    _fc_last_drops = _radio->getRxQueueDrops();
    _fc_last_notice_ms = _ms->getMillis();
  }
  _fc_flags = flags;
  uint8_t reply[2] = { RESP_CODE_OK, FC_BOARD_FLAGS };
  _serial->writeFrame(reply, 2);
}

void MyMesh::fcSendParts(uint8_t status, float snr, float rssi, const uint8_t raw[], int len) {
  uint8_t out[2][FC_FRAME_MAX];
  int out_len[2] = { 0, 0 };
  int parts = fcEncodeParts(_fc_tag++, status, (int8_t)(snr * 4), (int8_t)rssi, raw, len, out, out_len);
  for (int i = 0; i < parts; i++) {
    _serial->writeFrameToAll(out[i], out_len[i]);
  }
}

void MyMesh::fcLogRxLong(float snr, float rssi, const uint8_t raw[], int len) {
  if ((_fc_flags & FC_FLAG_LONG) && fcNeedsLongPush(len) && _serial->isConnected()) {
    fcSendParts(FC_STATUS_OK, snr, rssi, raw, len);
  }
}

void MyMesh::logRxBad(float snr, float rssi, int16_t err, const uint8_t raw[], int len) {
  if ((_fc_flags & FC_FLAG_BAD) && _serial->isConnected()) {
    fcSendParts(fcStatusForRadioError(err), snr, rssi, raw, len);
  }
}

void MyMesh::fcLoop() {
  if (_fc_flags == 0) return;
  if (!_serial->isConnected()) {
    _fc_flags = 0;   // the next app sends its own setting
    return;
  }
  if (!(_fc_flags & FC_FLAG_DROPS)) return;
  unsigned long now = _ms->getMillis();
  if (now - _fc_last_notice_ms < FC_NOTICE_INTERVAL_MS) return;
  _fc_last_notice_ms = now;
  uint32_t total = _radio->getRxQueueDrops();
  if (total == _fc_last_drops) return;
  uint8_t out[FC_FRAME_MAX];
  int n = fcEncodeDropNotice(_fc_tag++, total - _fc_last_drops, 0, 0, out);
  _serial->writeFrameToAll(out, n);
  _fc_last_drops = total;
}
