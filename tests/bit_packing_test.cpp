#include <array>
#include <cstdint>
#include <span>

#include <gtest/gtest.h>

#include "crsf/bit_packing.hpp"
#include "crsf/protocol.hpp"

namespace {

// CRSF packs LSB-first: the first 11 bits of {0xFF, 0xFF} are all ones, the next 11 are the
// remaining five ones zero-extended.
TEST(BitReader, FollowsLsbFirstPackingConvention)
{
  static constexpr std::array<std::uint8_t, 4> kData = {0xFF, 0xFF, 0x00, 0x00};
  crsf::BitReader reader{kData};
  EXPECT_EQ(reader.read(11), 0x7FFU);
  EXPECT_EQ(reader.read(11), 0x1FU);
  EXPECT_EQ(reader.read(11), 0U);
}

TEST(BitReader, ZeroPadsPastEndAndReportsExhausted)
{
  static constexpr std::array<std::uint8_t, 1> kData = {0xFF};
  crsf::BitReader reader{kData};
  EXPECT_EQ(reader.read(11), 0xFFU);
  EXPECT_TRUE(reader.exhausted());
  EXPECT_EQ(reader.read(11), 0U);
}

TEST(BitPacking, RoundTripsFullRcChannelPayload)
{
  std::array<std::uint16_t, crsf::kRcChannelCount> channels{};
  for (int i = 0; i < crsf::kRcChannelCount; ++i) {
    channels[static_cast<std::size_t>(i)] = static_cast<std::uint16_t>(172 + i * 109);
  }

  std::array<std::uint8_t, crsf::kRcChannelsPayloadSize> payload{};
  crsf::BitWriter writer{payload};
  for (const std::uint16_t value : channels) {
    ASSERT_TRUE(writer.write(value, crsf::kRcChannelBits));
  }
  EXPECT_EQ(writer.finish(), crsf::kRcChannelsPayloadSize);

  crsf::BitReader reader{payload};
  for (const std::uint16_t expected : channels) {
    EXPECT_EQ(reader.read(crsf::kRcChannelBits), expected);
  }
  EXPECT_FALSE(reader.exhausted());
}

TEST(BitPacking, RoundTripsElevenBitBoundaryValues)
{
  static constexpr std::array<std::uint16_t, 4> kValues = {0, 0x7FF, 172, 1811};
  std::array<std::uint8_t, 8> buffer{};

  crsf::BitWriter writer{buffer};
  for (const std::uint16_t value : kValues) {
    ASSERT_TRUE(writer.write(value, 11));
  }
  writer.finish();

  crsf::BitReader reader{buffer};
  for (const std::uint16_t expected : kValues) {
    EXPECT_EQ(reader.read(11), expected);
  }
}

// The same reader/writer must serve the subset-RC widths later, not just 11-bit.
TEST(BitPacking, RoundTripsSubsetChannelWidths)
{
  for (const int bits : {10, 11, 12, 13}) {
    const std::uint32_t max_value = (1U << bits) - 1U;
    const std::array<std::uint32_t, 5> values = {0, 1, max_value / 2, max_value - 1, max_value};

    std::array<std::uint8_t, 16> buffer{};
    crsf::BitWriter writer{buffer};
    for (const std::uint32_t value : values) {
      ASSERT_TRUE(writer.write(value, bits)) << "bits=" << bits;
    }
    writer.finish();

    crsf::BitReader reader{buffer};
    for (const std::uint32_t expected : values) {
      EXPECT_EQ(reader.read(bits), expected) << "bits=" << bits;
    }
  }
}

TEST(BitWriter, ReportsFailureInsteadOfOverrunning)
{
  std::array<std::uint8_t, 2> buffer{};
  crsf::BitWriter writer{buffer};
  EXPECT_TRUE(writer.write(0x7FF, 11));
  EXPECT_FALSE(writer.write(0x7FF, 11));
  EXPECT_TRUE(writer.overflowed());
  EXPECT_LE(writer.finish(), buffer.size());
}

TEST(BitWriter, MasksValuesWiderThanDeclaredWidth)
{
  std::array<std::uint8_t, 4> buffer{};
  crsf::BitWriter writer{buffer};
  ASSERT_TRUE(writer.write(2077, 11));  // 2077 exceeds the 11-bit range
  writer.finish();

  crsf::BitReader reader{buffer};
  EXPECT_EQ(reader.read(11), 2077U & 0x7FFU);
}

TEST(BitWriter, FinishFlushesPartialTrailingByte)
{
  std::array<std::uint8_t, 2> buffer{};
  crsf::BitWriter writer{buffer};
  ASSERT_TRUE(writer.write(0x7FF, 11));
  EXPECT_EQ(writer.finish(), 2U);

  crsf::BitReader reader{buffer};
  EXPECT_EQ(reader.read(11), 0x7FFU);
}

}  // namespace
