#pragma once

#include <stddef.h>
#include <stdint.h>

// Contacts that do not fit the contact table. A build with a smaller MAX_CONTACTS
// than the file holds (the Heltec V3 _mqtt image keeps 150, the usb_tcp image 350)
// stops loading early. DataStore copies the unread records to /contacts3.tail and
// appends them back after every save, so reflashing a bigger build brings them all
// back. Header-only over any reader with read(buf, n) and writer with write(buf, n)
// (Arduino File fits), so it runs on `native` (test/test_contact_tail).
namespace ContactTail {

  // One /contacts3 record: pub_key 32, name 32, type, flags, unused, sync_since 4,
  // out_path_len, last_advert 4, out_path 64, lastmod 4, gps_lat 4, gps_lon 4.
  static const size_t kRecordSize = 152;

  // Copy whole records from `in` to `out` until EOF; a trailing partial record is
  // dropped. Returns the number of records copied.
  template <class Reader, class Writer>
  size_t copyRecords(Reader& in, Writer& out) {
    uint8_t rec[kRecordSize];
    size_t n = 0;
    while (in.read(rec, kRecordSize) == kRecordSize) {
      if (out.write(rec, kRecordSize) != kRecordSize) break;
      n++;
    }
    return n;
  }

  // Append the records of `tail` to `out`, skipping any whose public key (the first
  // 32 bytes) `isHeld` reports as already in the table, so a contact heard again is
  // not listed twice. Returns the number of records appended.
  template <class Reader, class Writer, class Held>
  size_t appendTail(Reader& tail, Writer& out, Held isHeld) {
    uint8_t rec[kRecordSize];
    size_t n = 0;
    while (tail.read(rec, kRecordSize) == kRecordSize) {
      if (isHeld(rec)) continue;
      if (out.write(rec, kRecordSize) != kRecordSize) break;
      n++;
    }
    return n;
  }

}  // namespace ContactTail
