#include <gtest/gtest.h>

#include <cstring>
#include <vector>

// Contacts beyond MAX_CONTACTS: a build with a smaller contact table (the Heltec
// V3 _mqtt image keeps 150) must not drop the rest of /contacts3 when it saves.
// DataStore copies the unread records aside on load and appends them back after
// each save, skipping any contact the firmware now holds itself.
#include "../../examples/companion_radio/ContactTail.h"

namespace {

struct MemReader {
  const std::vector<uint8_t>& data;
  size_t pos = 0;
  explicit MemReader(const std::vector<uint8_t>& d) : data(d) {}
  size_t read(uint8_t* buf, size_t n) {
    size_t left = data.size() - pos;
    if (n > left) n = left;
    if (n) memcpy(buf, data.data() + pos, n);
    pos += n;
    return n;
  }
};

struct MemWriter {
  std::vector<uint8_t> data;
  size_t write(const uint8_t* buf, size_t n) {
    data.insert(data.end(), buf, buf + n);
    return n;
  }
};

// One 152-byte record whose public key starts with `id` and whose name byte is `name`.
std::vector<uint8_t> record(uint8_t id, uint8_t name = 'x') {
  std::vector<uint8_t> r(ContactTail::kRecordSize, 0);
  r[0] = id;
  r[32] = name;
  return r;
}

void append(std::vector<uint8_t>& to, const std::vector<uint8_t>& r) { to.insert(to.end(), r.begin(), r.end()); }

}  // namespace

TEST(ContactTail, RecordSizeMatchesTheContactsFileLayout) {
  // pub_key 32, name 32, type, flags, unused, sync_since 4, out_path_len,
  // last_advert 4, out_path 64, lastmod 4, gps_lat 4, gps_lon 4.
  EXPECT_EQ(152u, ContactTail::kRecordSize);
}

TEST(ContactTail, CopiesEveryWholeRecord) {
  std::vector<uint8_t> src;
  append(src, record(1));
  append(src, record(2));
  append(src, record(3));
  MemReader in(src);
  MemWriter out;
  EXPECT_EQ(3u, ContactTail::copyRecords(in, out));
  EXPECT_EQ(src, out.data);
}

TEST(ContactTail, DropsATrailingPartialRecord) {
  std::vector<uint8_t> src;
  append(src, record(1));
  src.resize(src.size() + 40, 0xAA);   // half-written record at the end
  MemReader in(src);
  MemWriter out;
  EXPECT_EQ(1u, ContactTail::copyRecords(in, out));
  EXPECT_EQ(ContactTail::kRecordSize, out.data.size());
}

TEST(ContactTail, AppendsTailRecordsTheFirmwareDoesNotHold) {
  std::vector<uint8_t> tail;
  append(tail, record(7));
  append(tail, record(8));
  MemReader in(tail);
  MemWriter out;
  size_t n = ContactTail::appendTail(in, out, [](const uint8_t*) { return false; });
  EXPECT_EQ(2u, n);
  EXPECT_EQ(tail, out.data);
}

TEST(ContactTail, SkipsTailRecordsTheFirmwareNowHolds) {
  // Contact 8 was heard again and is in the table now: the file must not list it twice.
  std::vector<uint8_t> tail;
  append(tail, record(7));
  append(tail, record(8, 'o'));
  append(tail, record(9));
  MemReader in(tail);
  MemWriter out;
  size_t n = ContactTail::appendTail(in, out, [](const uint8_t* pub_key) { return pub_key[0] == 8; });
  EXPECT_EQ(2u, n);
  ASSERT_EQ(2 * ContactTail::kRecordSize, out.data.size());
  EXPECT_EQ(7, out.data[0]);
  EXPECT_EQ(9, out.data[ContactTail::kRecordSize]);
}

TEST(ContactTail, EmptyTailWritesNothing) {
  std::vector<uint8_t> empty;
  MemReader in(empty);
  MemWriter out;
  EXPECT_EQ(0u, ContactTail::appendTail(in, out, [](const uint8_t*) { return false; }));
  EXPECT_TRUE(out.data.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
