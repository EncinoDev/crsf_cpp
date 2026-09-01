#include <gtest/gtest.h>

#include "crsf/crsf.hpp"

TEST(Timing, SerialTimeMatchesTenBitsPerByte)
{
  // 10 bytes at 100000 baud = 100 bits = exactly 1000 us.
  EXPECT_EQ(crsf::serial_time_us(10, 100000), 1000U);
  EXPECT_EQ(crsf::serial_time_us(0, 420000), 0U);
}

TEST(Timing, SerialTimeRoundsUp)
{
  // 1 byte at 420000 baud = 23.8 us; a truncating divide would under-wait.
  EXPECT_EQ(crsf::serial_time_us(1, 420000), 24U);
}

TEST(Timing, ZeroBaudIsNotADivideByZero)
{
  EXPECT_EQ(crsf::serial_time_us(64, 0), 0U);
  EXPECT_EQ(crsf::byte_gap_timeout_us(0), 0U);
}

// Betaflight uses 1748 us here; deriving from frame size and baud must land on the same value.
TEST(Timing, GapTimeoutAgreesWithTheReferenceAtItsNominalBaud)
{
  const std::uint32_t gap = crsf::byte_gap_timeout_us(420000);
  EXPECT_GE(gap, 1700U);
  EXPECT_LE(gap, 1800U);
}

// Why this takes a baud argument at all: the auto-detect scan sweeps rates.
TEST(Timing, GapTimeoutScalesInverselyWithBaud)
{
  EXPECT_GT(crsf::byte_gap_timeout_us(115200), crsf::byte_gap_timeout_us(420000));
  EXPECT_GT(crsf::byte_gap_timeout_us(420000), crsf::byte_gap_timeout_us(921600));
}

TEST(Timing, GapTimeoutExceedsAFullFrame)
{
  for (const std::uint32_t baud : {115200U, 400000U, 420000U, 921600U}) {
    EXPECT_GT(crsf::byte_gap_timeout_us(baud), crsf::serial_time_us(crsf::kMaxFrameSize, baud)) << "baud " << baud;
  }
}

static_assert(crsf::byte_gap_timeout_us(420000) > 0, "usable in a constant expression");
