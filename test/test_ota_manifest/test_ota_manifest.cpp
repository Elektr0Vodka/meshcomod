#include <gtest/gtest.h>

#include <cstring>
#include <string>

// Manifest pull OTA decisions (version compare, partition signature, URL rules).
// Pure string logic, so it runs on `native`; the network and flash side lives in
// ESP32Board.
#include "helpers/OtaManifest.h"

static const char* BASE = "https://ota.dutchmeshcore.nl/meshcomod/v";

// --- version parsing --------------------------------------------------------

TEST(OtaVersion, ParsesBuildScriptString) {
  OtaVersion v;
  ASSERT_TRUE(ota_version_parse("v1.18.0.1-ab12cd3", &v));
  EXPECT_EQ(v.count, 4);
  EXPECT_EQ(v.part[0], 1);
  EXPECT_EQ(v.part[1], 18);
  EXPECT_EQ(v.part[2], 0);
  EXPECT_EQ(v.part[3], 1);
  EXPECT_STREQ(v.hash, "ab12cd3");
}

TEST(OtaVersion, PlainVersionHasNoHash) {
  OtaVersion v;
  ASSERT_TRUE(ota_version_parse("v1.18.0.1", &v));
  EXPECT_EQ(v.count, 4);
  EXPECT_STREQ(v.hash, "");
}

TEST(OtaVersion, HashIsTheLastDashToken) {
  OtaVersion v;
  ASSERT_TRUE(ota_version_parse("v1.17.0.4-DMC-EV-abc1234", &v));
  EXPECT_EQ(v.part[3], 4);
  EXPECT_STREQ(v.hash, "abc1234");
}

TEST(OtaVersion, RejectsNonVersionPrefix) {
  OtaVersion v;
  EXPECT_FALSE(ota_version_parse("meshcomod-r1.14.1.0-repeater-tcp-abc1234", &v));
  EXPECT_FALSE(ota_version_parse("", &v));
  EXPECT_FALSE(ota_version_parse(nullptr, &v));
  EXPECT_FALSE(ota_version_parse("v1..2", &v));
}

// --- compare ---------------------------------------------------------------

TEST(OtaCompare, NewerBuildNumber) {
  EXPECT_EQ(ota_compare("v1.18.0.1-aaaaaaa", "v1.18.0.2", "bbbbbbb"), OTA_VERDICT_NEWER);
}

TEST(OtaCompare, NewerBaseVersion) {
  EXPECT_EQ(ota_compare("v1.18.0.9-aaaaaaa", "v1.19.0.0", "bbbbbbb"), OTA_VERDICT_NEWER);
}

TEST(OtaCompare, OlderIsNotAnUpdate) {
  EXPECT_EQ(ota_compare("v1.18.0.3-aaaaaaa", "v1.18.0.2", "bbbbbbb"), OTA_VERDICT_OLDER);
  EXPECT_EQ(ota_compare("v1.19.0.0-aaaaaaa", "v1.18.9.9", "bbbbbbb"), OTA_VERDICT_OLDER);
}

TEST(OtaCompare, SameVersionSameHashIsCurrent) {
  EXPECT_EQ(ota_compare("v1.18.0.1-ab12cd3", "v1.18.0.1", "ab12cd3"), OTA_VERDICT_CURRENT);
}

TEST(OtaCompare, HashPrefixAbsorbsAbbreviationLength) {
  EXPECT_EQ(ota_compare("v1.18.0.1-ab12cd3", "v1.18.0.1", "ab12cd34"), OTA_VERDICT_CURRENT);
}

TEST(OtaCompare, SameVersionOtherHashIsRebuild) {
  EXPECT_EQ(ota_compare("v1.18.0.1-ab12cd3", "v1.18.0.1", "ffffff0"), OTA_VERDICT_REBUILD);
}

TEST(OtaCompare, SameVersionWithoutRunningHashIsCurrent) {
  // A local build without build.sh embeds the bare MyMesh.h version.
  EXPECT_EQ(ota_compare("v1.18.0.1", "v1.18.0.1", "ab12cd3"), OTA_VERDICT_CURRENT);
}

TEST(OtaCompare, MissingFourthPartCountsAsZero) {
  EXPECT_EQ(ota_compare("v1.18.0-aaaaaaa", "v1.18.0.0", "aaaaaaa"), OTA_VERDICT_CURRENT);
  EXPECT_EQ(ota_compare("v1.18.0-aaaaaaa", "v1.18.0.1", "bbbbbbb"), OTA_VERDICT_NEWER);
}

TEST(OtaCompare, UnparseableFallsBackToHash) {
  EXPECT_EQ(ota_compare("custom-ab12cd3", "", "ab12cd3"), OTA_VERDICT_CURRENT);
  EXPECT_EQ(ota_compare("custom-ab12cd3", "", "ffffff0"), OTA_VERDICT_REBUILD);
}

// --- partition signature ---------------------------------------------------

TEST(OtaPartSig, MatchesScriptFormatSortedByOffset) {
  // Same definition as scripts/partition_signature.py: "%x:%x:%x:%x" joined by ','.
  OtaPartEntry e[] = {
    {0x00, 0x10, 0x10000, 0x330000},
    {0x01, 0x02, 0x9000, 0x5000},
    {0x01, 0x00, 0xe000, 0x2000},
  };
  char out[128];
  ota_partsig_format(e, 3, out, sizeof(out));
  EXPECT_STREQ(out, "1:2:9000:5000,1:0:e000:2000,0:10:10000:330000");
}

TEST(OtaPartSig, TruncatesWithoutOverflow) {
  OtaPartEntry e[] = {{0x01, 0x02, 0x9000, 0x5000}, {0x00, 0x10, 0x10000, 0x330000}};
  char out[8];
  ota_partsig_format(e, 2, out, sizeof(out));
  EXPECT_LT(strlen(out), sizeof(out));
}

// --- URLs -------------------------------------------------------------------

TEST(OtaUrl, ManifestCheckUsesPlainHttp) {
  char url[160];
  ASSERT_TRUE(ota_manifest_url(BASE, "heltec_v4_companion_radio_usb_tcp", url, sizeof(url)));
  EXPECT_STREQ(url, "http://ota.dutchmeshcore.nl/meshcomod/v/heltec_v4_companion_radio_usb_tcp.json");
}

TEST(OtaUrl, ManifestUrlTooLongFails) {
  char url[16];
  EXPECT_FALSE(ota_manifest_url(BASE, "heltec_v4_companion_radio_usb_tcp", url, sizeof(url)));
}

TEST(OtaUrl, FirmwareUnderSiblingFwPathIsAllowed) {
  EXPECT_TRUE(ota_firmware_url_allowed(
      "https://ota.dutchmeshcore.nl/meshcomod/fw/companion-v1.18.0.2/heltec_v4_companion_radio_usb_tcp-v1.18.0.2-abc1234.bin",
      BASE));
  EXPECT_TRUE(ota_firmware_url_allowed(
      "http://ota.dutchmeshcore.nl/meshcomod/fw/companion-v1.18.0.2/x-v1.18.0.2-abc1234.bin", BASE));
}

TEST(OtaUrl, FirmwareElsewhereIsRefused) {
  EXPECT_FALSE(ota_firmware_url_allowed("https://evil.example/meshcomod/fw/a.bin", BASE));
  EXPECT_FALSE(ota_firmware_url_allowed("https://ota.dutchmeshcore.nl/mqtt/fw/a/b.bin", BASE));
  EXPECT_FALSE(ota_firmware_url_allowed("https://ota.dutchmeshcore.nl/meshcomod/fw/../../x.bin", BASE));
  EXPECT_FALSE(ota_firmware_url_allowed("https://ota.dutchmeshcore.nl/meshcomod/fwx/a.bin", BASE));
  EXPECT_FALSE(ota_firmware_url_allowed("ftp://ota.dutchmeshcore.nl/meshcomod/fw/a.bin", BASE));
  EXPECT_FALSE(ota_firmware_url_allowed(nullptr, BASE));
}

TEST(OtaUrl, BaseWithoutTrailingVAllowsNothing) {
  EXPECT_FALSE(ota_firmware_url_allowed("https://ota.dutchmeshcore.nl/meshcomod/fw/a.bin",
                                        "https://ota.dutchmeshcore.nl/meshcomod"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
