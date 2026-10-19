#include <gtest/gtest.h>

#include <cstring>

// Full capture push encoder of the companion firmware. Free of radio and
// serial, so it runs on `native`. RTFM-EV decodes the same byte layout
// (app/full_capture.py there).
#include "../../examples/companion_radio/FullCapture.h"

static void fill(uint8_t* raw, int len) {
  for (int i = 0; i < len; i++) raw[i] = (uint8_t)(i % 251);
}

TEST(FullCapture, SplitsFor174Bytes) {
  uint8_t raw[174];
  fill(raw, sizeof(raw));
  uint8_t out[2][FC_FRAME_MAX];
  int out_len[2] = {0, 0};

  ASSERT_EQ(fcEncodeParts(9, FC_STATUS_OK, 16, -60, raw, 174, out, out_len), 2);
  EXPECT_EQ(out_len[0], 176);
  EXPECT_EQ(out_len[1], 12);
  EXPECT_EQ(out[0][0], FC_PUSH);
  EXPECT_EQ(out[0][1], 9);
  EXPECT_EQ(out[0][2], 0x02);  // index 0 of 2
  EXPECT_EQ(out[1][2], 0x12);  // index 1 of 2
  EXPECT_EQ(out[0][6], 174);
  EXPECT_EQ(out[1][6], 174);
  EXPECT_EQ(memcmp(&out[0][FC_HEADER_LEN], raw, 169), 0);
  EXPECT_EQ(memcmp(&out[1][FC_HEADER_LEN], raw + 169, 5), 0);
}

TEST(FullCapture, EncodesTwoPartsFor255Bytes) {
  uint8_t raw[255];
  fill(raw, sizeof(raw));
  uint8_t out[2][FC_FRAME_MAX];
  int out_len[2] = {0, 0};

  ASSERT_EQ(fcEncodeParts(200, FC_STATUS_OK, -10, (int8_t)0xC4, raw, 255, out, out_len), 2);
  EXPECT_EQ(out_len[0], 176);
  EXPECT_EQ(out_len[1], 93);
  for (int p = 0; p < 2; p++) {
    EXPECT_EQ(out[p][1], 200);
    EXPECT_EQ(out[p][3], FC_STATUS_OK);
    EXPECT_EQ((int8_t)out[p][4], -10);
    EXPECT_EQ((int8_t)out[p][5], -60);
    EXPECT_EQ(out[p][6], 255);
  }
}

TEST(FullCapture, EncodesSmallBadFrameInOnePart) {
  uint8_t raw[40];
  fill(raw, sizeof(raw));
  uint8_t out[2][FC_FRAME_MAX];
  int out_len[2] = {0, 0};

  ASSERT_EQ(fcEncodeParts(1, FC_STATUS_CRC, 0, -90, raw, 40, out, out_len), 1);
  EXPECT_EQ(out[0][2], 0x01);
  EXPECT_EQ(out[0][3], FC_STATUS_CRC);
  EXPECT_EQ(out_len[0], 47);
  EXPECT_EQ(memcmp(&out[0][FC_HEADER_LEN], raw, 40), 0);
}

TEST(FullCapture, RejectsOutOfRange) {
  uint8_t raw[256] = {0};
  uint8_t out[2][FC_FRAME_MAX];
  int out_len[2] = {0, 0};

  EXPECT_EQ(fcEncodeParts(1, FC_STATUS_OK, 0, 0, raw, 256, out, out_len), 0);
  EXPECT_EQ(fcEncodeParts(1, FC_STATUS_OK, 0, 0, raw, -1, out, out_len), 0);
  EXPECT_EQ(fcEncodeParts(1, FC_STATUS_OK, 0, 0, raw, 0, out, out_len), 0);
  EXPECT_EQ(fcEncodeParts(1, FC_STATUS_OK, 0, 0, nullptr, 10, out, out_len), 0);
}

TEST(FullCapture, DropNotice) {
  uint8_t out[FC_FRAME_MAX];

  ASSERT_EQ(fcEncodeDropNotice(3, 300, 0, 0, out), 7);
  EXPECT_EQ(out[0], FC_PUSH);
  EXPECT_EQ(out[1], 3);
  EXPECT_EQ(out[2], 0x01);
  EXPECT_EQ(out[3], FC_STATUS_DROPS);
  EXPECT_EQ(out[6], 255);

  ASSERT_EQ(fcEncodeDropNotice(4, 12, 0, 0, out), 7);
  EXPECT_EQ(out[6], 12);
}

TEST(FullCapture, StatusMapping) {
  EXPECT_EQ(fcStatusForRadioError(-7), FC_STATUS_CRC);
  EXPECT_EQ(fcStatusForRadioError(-24), FC_STATUS_HEADER);
  EXPECT_EQ(fcStatusForRadioError(-16), FC_STATUS_READ_ERR);
}

TEST(FullCapture, LongPushBoundary) {
  // 173 bytes still fit the stock 0x88 push; exactly one of the two fires.
  EXPECT_FALSE(fcNeedsLongPush(173));
  EXPECT_TRUE(fcNeedsLongPush(174));
  EXPECT_TRUE(fcNeedsLongPush(255));
  EXPECT_FALSE(fcNeedsLongPush(256));
}

TEST(FullCapture, FlagsValidation) {
  EXPECT_TRUE(fcFlagsValid(0));
  EXPECT_TRUE(fcFlagsValid(FC_SUPPORTED));
  EXPECT_FALSE(fcFlagsValid(0x08));
  EXPECT_FALSE(fcFlagsValid(0xFF));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
