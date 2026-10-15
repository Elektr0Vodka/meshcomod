// Companion receive filter: the MyMesh side. Kept in its own file so MyMesh.cpp
// only carries one-line calls into it.

#include "MyMesh.h"

// Root-aware, so the filter settings follow the rest of the data onto the SD
// card when DataStore::useSdStorage() is active.
bool loadCompanionFilter(CompanionFilter& filter, DataStore& store) {
  if (!store.fileExists(CFILTER_PREFS_FILE)) return true;   // nothing saved yet: defaults (off)

  File file = store.openRead(CFILTER_PREFS_FILE);
  if (!file) return false;
  filter.prefs().reset();
  bool ok = filter.prefs().loadSerial(file);
  file.close();
  filter.prefs().sanitize();
  return ok;
}

bool saveCompanionFilter(CompanionFilter& filter, DataStore& store) {
  File file = store.openWrite(store.getPrimaryFS(), CFILTER_PREFS_FILE);
  if (!file) return false;
  bool ok = filter.prefs().saveSerial(file);
  file.close();
  return ok;
}

// From filterRecvFloodPacket(): true drops the packet before the mesh handles
// it, so it is neither shown nor repeated.
bool MyMesh::cfilterFlood(mesh::Packet* packet) {
  _cfilter_hidden_set = false;
  if (!_cfilter.isEnabled()) return false;

  uint8_t hash[MAX_HASH_SIZE];
  packet->calculatePacketHash(hash);
  // A relayed copy of a packet already judged: the mesh's duplicate check
  // handles it (a dropped one was marked seen below).
  if (_cfilter.alreadyJudged(hash)) return false;

  CFilterVerdict verdict = _cfilter.checkPath(packet);

  // Group text is decrypted here with this node's own channel keys, the same
  // way Mesh::onRecvPacket() is about to. Only channels the user holds can be
  // read, so only those are filtered on content.
  if (verdict == CFilterVerdict::PASS && packet->getPayloadType() == PAYLOAD_TYPE_GRP_TXT &&
      _cfilter.wantsGroupText() && packet->payload_len > PATH_HASH_SIZE + CIPHER_MAC_SIZE) {
    mesh::GroupChannel channels[4];
    int num = searchChannelsByHash(packet->payload, channels, 4);
    for (int i = 0; i < num; i++) {
      uint8_t data[MAX_PACKET_PAYLOAD + 1];
      int len = mesh::Utils::MACThenDecrypt(channels[i].secret, data, &packet->payload[PATH_HASH_SIZE],
                                            packet->payload_len - PATH_HASH_SIZE);
      if (len > 0) {
        verdict = _cfilter.checkGroupText(data, len, getRTCClock()->getCurrentTime(), millis(), (uint8_t)random(100));
        break;
      }
    }
  }

  if (verdict == CFilterVerdict::PASS) return false;

  if (verdict == CFilterVerdict::HIDE && _prefs.client_repeat != 0) {
    // A personal rule: still repeated for everyone else, only not shown here.
    memcpy(_cfilter_hidden, hash, sizeof(_cfilter_hidden));
    _cfilter_hidden_set = true;
    return false;
  }

  getTables()->markSeen(packet);   // later copies then die in the mesh's duplicate check
  return true;
}

// From onChannelMessageRecv(): true when this is the message cfilterFlood()
// let through only so it could be repeated.
bool MyMesh::cfilterHidden(const mesh::Packet* packet) {
  if (!_cfilter_hidden_set) return false;

  uint8_t hash[MAX_HASH_SIZE];
  packet->calculatePacketHash(hash);
  if (memcmp(hash, _cfilter_hidden, sizeof(_cfilter_hidden)) != 0) return false;
  _cfilter_hidden_set = false;
  return true;
}

// From handleCommand(): `filter ...`.
bool MyMesh::cfilterCommand(const char* command, char* reply) {
  if (!CompanionFilter::isCommand(command)) return false;
  if (_cfilter.handleCommand(command, reply, getRTCClock()->getCurrentTime())) {
    saveCompanionFilter(_cfilter, *_store);
  }
  return true;
}
