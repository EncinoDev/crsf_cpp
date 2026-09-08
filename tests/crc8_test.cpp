// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#include <array>
#include <cstdint>
#include <span>

#include <gtest/gtest.h>

#include "crsf/crc8.hpp"

namespace {

// Public CRC-8/DVB-S2 catalog check value (reveng catalog), independent of Betaflight.
TEST(Crc8, MatchesDvbS2CatalogCheckValue)
{
  static constexpr std::array<std::uint8_t, 9> kCheckInput = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(crsf::crc8<crsf::Crc8Dvb>(kCheckInput), 0xBC);
  EXPECT_EQ(crsf::crc8<crsf::Crc8DvbBitShift>(kCheckInput), 0xBC);
}

TEST(Crc8, EmptyInputReturnsInit)
{
  EXPECT_EQ(crsf::crc8<crsf::Crc8Dvb>(std::span<const std::uint8_t>{}, 0x00), 0x00);
  EXPECT_EQ(crsf::crc8<crsf::Crc8Dvb>(std::span<const std::uint8_t>{}, 0x42), 0x42);
}

TEST(Crc8, LutPolicyMatchesBitShiftPolicyForAllByteValues)
{
  for (int crc_in = 0; crc_in <= 0xFF; ++crc_in) {
    for (int byte_in = 0; byte_in <= 0xFF; ++byte_in) {
      const auto crc = static_cast<std::uint8_t>(crc_in);
      const auto byte = static_cast<std::uint8_t>(byte_in);
      ASSERT_EQ(crsf::Crc8LutPolicy<crsf::kCrc8PolyDvbS2>::update(crc, byte),
                crsf::Crc8BitShiftPolicy<crsf::kCrc8PolyDvbS2>::update(crc, byte));
    }
  }
}

TEST(Crc8, MultiByteSequenceAgreesBetweenPolicies)
{
  static constexpr std::array<std::uint8_t, 6> kFrame = {0xC8, 0x18, 0x16, 0x01, 0x02, 0x03};
  EXPECT_EQ(crsf::crc8<crsf::Crc8Dvb>(kFrame), crsf::crc8<crsf::Crc8DvbBitShift>(kFrame));
}

TEST(Crc8, IsUsableInConstexprContext)
{
  static constexpr std::array<std::uint8_t, 3> kData = {0x01, 0x02, 0x03};
  static constexpr std::uint8_t kCrc = crsf::crc8<crsf::Crc8Dvb>(kData);
  static_assert(kCrc == crsf::crc8<crsf::Crc8DvbBitShift>(kData));
  EXPECT_EQ(kCrc, crsf::crc8<crsf::Crc8DvbBitShift>(kData));
}

}  // namespace
