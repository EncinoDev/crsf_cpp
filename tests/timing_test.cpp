// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

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
  EXPECT_EQ(crsf::frame_assembly_timeout_us(0), 0U);
  EXPECT_EQ(crsf::idle_gap_timeout_us(0), 0U);
}

// One character time, what a USART IDLE line interrupt reports.
TEST(Timing, IdleGapIsOneCharacterTime)
{
  EXPECT_EQ(crsf::idle_gap_timeout_us(420000), crsf::serial_time_us(1, 420000));
  EXPECT_EQ(crsf::idle_gap_timeout_us(420000), 24U);
}

// The two names once meant the same function; a caller resyncing on the idle gap must not wait a
// whole frame.
TEST(Timing, IdleGapIsFarShorterThanFrameAssembly)
{
  for (const std::uint32_t baud : {115200U, 400000U, 420000U, 921600U}) {
    EXPECT_GT(crsf::frame_assembly_timeout_us(baud), crsf::idle_gap_timeout_us(baud) * 50) << "baud " << baud;
  }
}

// Betaflight uses 1748 us here; deriving from frame size and baud must land on the same value.
TEST(Timing, FrameAssemblyTimeoutAgreesWithTheReferenceAtItsNominalBaud)
{
  const std::uint32_t gap = crsf::frame_assembly_timeout_us(420000);
  EXPECT_GE(gap, 1700U);
  EXPECT_LE(gap, 1800U);
}

// Why this takes a baud argument at all: the auto-detect scan sweeps rates.
TEST(Timing, FrameAssemblyTimeoutScalesInverselyWithBaud)
{
  EXPECT_GT(crsf::frame_assembly_timeout_us(115200), crsf::frame_assembly_timeout_us(420000));
  EXPECT_GT(crsf::frame_assembly_timeout_us(420000), crsf::frame_assembly_timeout_us(921600));
}

TEST(Timing, FrameAssemblyTimeoutExceedsAFullFrame)
{
  for (const std::uint32_t baud : {115200U, 400000U, 420000U, 921600U}) {
    EXPECT_GT(crsf::frame_assembly_timeout_us(baud), crsf::serial_time_us(crsf::kMaxFrameSize, baud)) << "baud " << baud;
  }
}

static_assert(crsf::frame_assembly_timeout_us(420000) > 0, "usable in a constant expression");
static_assert(crsf::idle_gap_timeout_us(420000) > 0, "usable in a constant expression");
