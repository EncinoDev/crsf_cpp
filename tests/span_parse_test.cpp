// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "crsf/crsf.hpp"

namespace {

std::vector<std::uint8_t> centered_frame()
{
  std::array<std::uint8_t, crsf::kMaxFrameSize> buffer{};
  const std::size_t size = crsf::build_rc_channels_frame(crsf::kSyncByte, crsf::make_centered_channels(), buffer);
  return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(size)};
}

}  // namespace

TEST(SpanParse, ParsesFrameAndConsumesExactlyItsLength)
{
  const auto frame = centered_frame();
  const auto result = crsf::parse(frame);

  EXPECT_EQ(result.status, crsf::ParseStatus::kFrameReady);
  EXPECT_EQ(result.consumed, frame.size());
  EXPECT_EQ(result.frame.address, crsf::kSyncByte);
  EXPECT_EQ(result.frame.type, static_cast<std::uint8_t>(crsf::FrameType::kRcChannelsPacked));
  EXPECT_EQ(result.frame.payload.size(), crsf::kRcChannelsPayloadSize);
}

// The point of the span API: the view aliases the caller's memory rather than a copy.
TEST(SpanParse, FrameViewPointsIntoCallerBuffer)
{
  const auto frame = centered_frame();
  const auto result = crsf::parse(frame);

  ASSERT_EQ(result.status, crsf::ParseStatus::kFrameReady);
  const std::uint8_t* payload_start = frame.data() + crsf::kAddressFieldSize + crsf::kLengthFieldSize + crsf::kTypeFieldSize;
  EXPECT_EQ(result.frame.payload.data(), payload_start);
}

TEST(SpanParse, IncompleteConsumesNothing)
{
  const auto frame = centered_frame();
  for (std::size_t prefix = 0; prefix < frame.size(); ++prefix) {
    const auto result = crsf::parse(std::span<const std::uint8_t>{frame.data(), prefix});
    EXPECT_EQ(result.status, crsf::ParseStatus::kIncomplete) << "prefix " << prefix;
    EXPECT_EQ(result.consumed, 0U) << "prefix " << prefix;
  }
}

TEST(SpanParse, BadLengthConsumesOneByte)
{
  auto frame = centered_frame();
  frame[1] = 0xFF;

  const auto result = crsf::parse(frame);
  EXPECT_EQ(result.status, crsf::ParseStatus::kInvalidLength);
  EXPECT_EQ(result.consumed, 1U);
}

TEST(SpanParse, BadCrcConsumesOneByte)
{
  auto frame = centered_frame();
  frame.back() ^= 0xFF;

  const auto result = crsf::parse(frame);
  EXPECT_EQ(result.status, crsf::ParseStatus::kCrcMismatch);
  EXPECT_EQ(result.consumed, 1U);
}

// Sliding a byte at a time is what locks onto a frame that starts part-way through garbage.
TEST(SpanParse, SlidesPastLeadingGarbageAndFindsTheFrame)
{
  const auto frame = centered_frame();
  std::vector<std::uint8_t> stream{0xC8, 0x18, 0x00, 0xFF, 0x42};
  stream.insert(stream.end(), frame.begin(), frame.end());

  std::span<const std::uint8_t> remaining{stream};
  bool found = false;
  while (!remaining.empty()) {
    const auto result = crsf::parse(remaining);
    if (result.status == crsf::ParseStatus::kIncomplete) {
      break;
    }
    if (result.status == crsf::ParseStatus::kFrameReady) {
      found = true;
      EXPECT_EQ(result.frame.payload.size(), crsf::kRcChannelsPayloadSize);
      break;
    }
    remaining = remaining.subspan(result.consumed);
  }
  EXPECT_TRUE(found);
}

TEST(SpanParse, ParsesBackToBackFramesFromOneSpan)
{
  const auto frame = centered_frame();
  std::vector<std::uint8_t> stream;
  for (int i = 0; i < 3; ++i) {
    stream.insert(stream.end(), frame.begin(), frame.end());
  }

  std::span<const std::uint8_t> remaining{stream};
  int frames = 0;
  while (!remaining.empty()) {
    const auto result = crsf::parse(remaining);
    if (result.status == crsf::ParseStatus::kIncomplete) {
      break;
    }
    if (result.status == crsf::ParseStatus::kFrameReady) {
      ++frames;
    }
    remaining = remaining.subspan(result.consumed);
  }
  EXPECT_EQ(frames, 3);
}

TEST(SpanParse, AgreesWithFeedOnTheSameFrame)
{
  const auto frame = centered_frame();

  crsf::Parser<> parser;
  auto status = crsf::ParseStatus::kIncomplete;
  for (const std::uint8_t byte : frame) {
    status = parser.feed(byte);
  }
  ASSERT_EQ(status, crsf::ParseStatus::kFrameReady);

  const auto result = crsf::parse(frame);
  ASSERT_EQ(result.status, crsf::ParseStatus::kFrameReady);

  EXPECT_EQ(result.frame.address, parser.frame().address);
  EXPECT_EQ(result.frame.type, parser.frame().type);
  ASSERT_EQ(result.frame.payload.size(), parser.frame().payload.size());
  for (std::size_t i = 0; i < result.frame.payload.size(); ++i) {
    EXPECT_EQ(result.frame.payload[i], parser.frame().payload[i]) << "byte " << i;
  }
}

TEST(SpanParse, WorksWithBitShiftCrcPolicy)
{
  const auto frame = centered_frame();
  const auto result = crsf::parse<crsf::Crc8DvbBitShift>(frame);
  EXPECT_EQ(result.status, crsf::ParseStatus::kFrameReady);
}

TEST(SpanParse, IsUsableInConstexprContext)
{
  static constexpr std::array<std::uint8_t, 26> kFrame{0xC8, 0x18, 0x16, 0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0, 0x81, 0x0F,
                                                       0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C, 0xAD};
  static_assert(crsf::parse(kFrame).status == crsf::ParseStatus::kFrameReady);
  static_assert(crsf::parse(kFrame).consumed == kFrame.size());
  SUCCEED();
}
