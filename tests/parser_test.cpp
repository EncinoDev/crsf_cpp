#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "crsf/crc8.hpp"
#include "crsf/parser.hpp"
#include "crsf/protocol.hpp"

namespace {

std::vector<std::uint8_t> make_frame(std::uint8_t address, std::uint8_t type, std::span<const std::uint8_t> payload)
{
  std::vector<std::uint8_t> frame;
  frame.push_back(address);
  frame.push_back(static_cast<std::uint8_t>(payload.size() + crsf::kTypeFieldSize + crsf::kCrcFieldSize));
  frame.push_back(type);
  frame.insert(frame.end(), payload.begin(), payload.end());

  std::vector<std::uint8_t> crc_scope;
  crc_scope.push_back(type);
  crc_scope.insert(crc_scope.end(), payload.begin(), payload.end());
  frame.push_back(crsf::crc8<crsf::Crc8Dvb>(crc_scope));
  return frame;
}

template <typename ParserT>
crsf::ParseStatus feed_all(ParserT& parser, std::span<const std::uint8_t> bytes)
{
  auto status = crsf::ParseStatus::kIncomplete;
  for (const std::uint8_t byte : bytes) {
    status = parser.feed(byte);
  }
  return status;
}

TEST(Parser, ParsesValidRcChannelsFrame)
{
  std::array<std::uint8_t, crsf::kRcChannelsPayloadSize> payload{};
  for (std::size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<std::uint8_t>(i);
  }
  const auto frame = make_frame(crsf::kAddressFlightController, static_cast<std::uint8_t>(crsf::FrameType::kRcChannelsPacked), payload);
  ASSERT_EQ(frame.size(), 26U);

  crsf::Parser<> parser;
  EXPECT_EQ(feed_all(parser, frame), crsf::ParseStatus::kFrameReady);
  EXPECT_EQ(parser.frame().address, crsf::kAddressFlightController);
  EXPECT_EQ(parser.frame().type, static_cast<std::uint8_t>(crsf::FrameType::kRcChannelsPacked));
  ASSERT_EQ(parser.frame().payload.size(), payload.size());
  EXPECT_TRUE(std::equal(payload.begin(), payload.end(), parser.frame().payload.begin()));
}

TEST(Parser, AcceptsBroadcastAndOtherAddresses)
{
  const std::array<std::uint8_t, 2> payload = {0xAA, 0xBB};
  for (const std::uint8_t address : {crsf::kAddressBroadcast, crsf::kAddressFlightController, std::uint8_t{0xEE}}) {
    const auto frame = make_frame(address, 0x16, payload);
    crsf::Parser<> parser;
    EXPECT_EQ(feed_all(parser, frame), crsf::ParseStatus::kFrameReady);
    EXPECT_EQ(parser.frame().address, address);
  }
}

TEST(Parser, RejectsOutOfRangeLengthAndRecovers)
{
  const std::array<std::uint8_t, 2> payload = {0x01, 0x02};
  const auto good = make_frame(crsf::kAddressFlightController, 0x16, payload);

  for (const std::uint8_t bad_length : {std::uint8_t{0}, std::uint8_t{1}, std::uint8_t{63}, std::uint8_t{255}}) {
    crsf::Parser<> parser;
    EXPECT_EQ(parser.feed(crsf::kAddressFlightController), crsf::ParseStatus::kIncomplete);
    EXPECT_EQ(parser.feed(bad_length), crsf::ParseStatus::kInvalidLength) << "length=" << int{bad_length};
    EXPECT_EQ(feed_all(parser, good), crsf::ParseStatus::kFrameReady) << "length=" << int{bad_length};
  }
}

TEST(Parser, RejectsCrcMismatchAndRecovers)
{
  const std::array<std::uint8_t, 2> payload = {0x01, 0x02};
  auto corrupted = make_frame(crsf::kAddressFlightController, 0x16, payload);
  corrupted.back() ^= 0xFF;

  crsf::Parser<> parser;
  EXPECT_EQ(feed_all(parser, corrupted), crsf::ParseStatus::kCrcMismatch);

  const auto good = make_frame(crsf::kAddressFlightController, 0x16, payload);
  EXPECT_EQ(feed_all(parser, good), crsf::ParseStatus::kFrameReady);
}

TEST(Parser, DetectsCorruptionAnywhereInFrame)
{
  const std::array<std::uint8_t, 4> payload = {0x10, 0x20, 0x30, 0x40};
  const auto good = make_frame(crsf::kAddressFlightController, 0x16, payload);

  for (std::size_t i = crsf::kAddressFieldSize + crsf::kLengthFieldSize; i < good.size(); ++i) {
    auto corrupted = good;
    corrupted[i] ^= 0x01;
    crsf::Parser<> parser;
    EXPECT_EQ(feed_all(parser, corrupted), crsf::ParseStatus::kCrcMismatch) << "corrupted byte " << i;
  }
}

TEST(Parser, AcceptsMinimumLengthFrameWithEmptyPayload)
{
  const auto frame = make_frame(crsf::kAddressFlightController, 0x16, {});
  ASSERT_EQ(frame.size(), 4U);

  crsf::Parser<> parser;
  EXPECT_EQ(feed_all(parser, frame), crsf::ParseStatus::kFrameReady);
  EXPECT_TRUE(parser.frame().payload.empty());
}

TEST(Parser, AcceptsMaximumLengthFrame)
{
  std::vector<std::uint8_t> payload(crsf::kMaxPayloadSize, 0x5A);
  const auto frame = make_frame(crsf::kAddressFlightController, 0x16, payload);
  ASSERT_EQ(frame.size(), crsf::kMaxFrameSize);

  crsf::Parser<> parser;
  EXPECT_EQ(feed_all(parser, frame), crsf::ParseStatus::kFrameReady);
  EXPECT_EQ(parser.frame().payload.size(), crsf::kMaxPayloadSize);
}

TEST(Parser, ParsesBackToBackFramesFromOneStream)
{
  const std::array<std::uint8_t, 2> payload = {0x01, 0x02};
  const auto frame = make_frame(crsf::kAddressFlightController, 0x16, payload);

  crsf::Parser<> parser;
  for (int repeat = 0; repeat < 3; ++repeat) {
    EXPECT_EQ(feed_all(parser, frame), crsf::ParseStatus::kFrameReady) << "repeat " << repeat;
  }
}

TEST(Parser, ResetDiscardsPartialFrame)
{
  const std::array<std::uint8_t, 2> payload = {0x01, 0x02};
  const auto frame = make_frame(crsf::kAddressFlightController, 0x16, payload);

  crsf::Parser<> parser;
  parser.feed(frame[0]);
  parser.feed(frame[1]);
  parser.feed(frame[2]);
  parser.reset();

  EXPECT_EQ(feed_all(parser, frame), crsf::ParseStatus::kFrameReady);
}

TEST(Parser, WorksWithBitShiftCrcPolicy)
{
  const std::array<std::uint8_t, 2> payload = {0x01, 0x02};
  const auto frame = make_frame(crsf::kAddressFlightController, 0x16, payload);

  crsf::Parser<crsf::Crc8DvbBitShift> parser;
  EXPECT_EQ(feed_all(parser, frame), crsf::ParseStatus::kFrameReady);
  EXPECT_EQ(parser.frame().payload.size(), payload.size());
}

// A stalled frame followed by resumed traffic costs one frame, then self-heals on the next.
TEST(Parser, RecoversFromTruncatedFrameWithoutReset)
{
  const std::array<std::uint8_t, 2> payload = {0x01, 0x02};
  const auto frame = make_frame(crsf::kAddressFlightController, 0x16, payload);

  crsf::Parser<> parser;
  parser.feed(frame[0]);
  parser.feed(frame[1]);

  auto status = crsf::ParseStatus::kIncomplete;
  for (int repeat = 0; repeat < 3; ++repeat) {
    status = feed_all(parser, frame);
  }
  EXPECT_EQ(status, crsf::ParseStatus::kFrameReady);
}

}  // namespace
