// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "crsf/crsf.hpp"

namespace {

std::vector<std::uint8_t> rc_frame()
{
  std::array<std::uint8_t, crsf::kMaxFrameSize> buffer{};
  const std::size_t size = crsf::build_rc_channels_frame(crsf::kSyncByte, crsf::make_centered_channels(), buffer);
  return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(size)};
}

std::vector<std::uint8_t> collect(std::span<const std::uint8_t> data, std::size_t& consumed)
{
  std::vector<std::uint8_t> types;
  consumed = crsf::scan(data, [&types](const crsf::FrameView& frame) { types.push_back(frame.type); });
  return types;
}

}  // namespace

TEST(Scan, ReportsEveryFrameInABackToBackRun)
{
  const auto one = rc_frame();
  std::vector<std::uint8_t> stream;
  for (int i = 0; i < 3; ++i) {
    stream.insert(stream.end(), one.begin(), one.end());
  }

  std::size_t consumed = 0;
  const auto types = collect(stream, consumed);

  EXPECT_EQ(types.size(), 3U);
  EXPECT_EQ(consumed, stream.size());
}

// Why scan() returns a count: a frame split across two reads must survive the first.
TEST(Scan, LeavesATrailingPartialFrameUnconsumed)
{
  const auto one = rc_frame();
  std::vector<std::uint8_t> stream = one;
  stream.insert(stream.end(), one.begin(), one.begin() + 5);

  std::size_t consumed = 0;
  const auto types = collect(stream, consumed);

  EXPECT_EQ(types.size(), 1U);
  EXPECT_EQ(consumed, one.size());
}

TEST(Scan, ResynchronisesPastLeadingGarbage)
{
  const auto one = rc_frame();
  std::vector<std::uint8_t> stream{0xC8, 0xFF, 0x01, 0x02};
  stream.insert(stream.end(), one.begin(), one.end());

  std::size_t consumed = 0;
  const auto types = collect(stream, consumed);

  EXPECT_EQ(types.size(), 1U);
  EXPECT_EQ(consumed, stream.size());
}

TEST(Scan, ConsumesGarbageThatCannotStartAFrame)
{
  const std::array<std::uint8_t, 8> stream{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  std::size_t consumed = 0;
  const auto types = collect(stream, consumed);

  EXPECT_TRUE(types.empty());
  // The last byte is retained: with no length byte behind it, it cannot yet be ruled out.
  EXPECT_EQ(consumed, stream.size() - 1);
}

// A plausible length byte holds the tail: it may be a real frame still arriving. The caller caps
// the accumulator, since only it can tell a stalled stream from a slow one.
TEST(Scan, RetainsGarbageWhoseLengthByteIsPlausible)
{
  const std::array<std::uint8_t, 2> stream{0x11, 0x22};

  std::size_t consumed = 0;
  const auto types = collect(stream, consumed);

  EXPECT_TRUE(types.empty());
  EXPECT_EQ(consumed, 0U);
}

TEST(Scan, EmptyInputConsumesNothing)
{
  std::size_t consumed = 0;
  const auto types = collect({}, consumed);

  EXPECT_TRUE(types.empty());
  EXPECT_EQ(consumed, 0U);
}

TEST(Scan, FrameViewsAliasTheCallerBuffer)
{
  const auto stream = rc_frame();
  const std::uint8_t* payload_start = stream.data() + crsf::kAddressFieldSize + crsf::kLengthFieldSize + crsf::kTypeFieldSize;

  const std::uint8_t* seen = nullptr;
  crsf::scan(stream, [&seen](const crsf::FrameView& frame) { seen = frame.payload.data(); });

  EXPECT_EQ(seen, payload_start);
}
