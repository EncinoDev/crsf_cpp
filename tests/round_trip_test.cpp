#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "crsf/crsf.hpp"

namespace {

crsf::RcChannels make_test_channels()
{
  crsf::RcChannels channels{};
  for (std::size_t i = 0; i < channels.size(); ++i) {
    channels[i] = static_cast<std::uint16_t>(crsf::kRcChannelMin + i * 109);
  }
  return channels;
}

TEST(RcChannels, RoundTripsThroughEncodeAndDecode)
{
  const auto channels = make_test_channels();
  std::array<std::uint8_t, crsf::kRcChannelsPayloadSize> payload{};
  ASSERT_TRUE(crsf::encode_rc_channels(channels, payload));

  crsf::RcChannels decoded{};
  ASSERT_TRUE(crsf::decode_rc_channels(payload, decoded));
  EXPECT_EQ(decoded, channels);
}

TEST(RcChannels, RejectsUndersizedBuffers)
{
  const auto channels = make_test_channels();
  std::array<std::uint8_t, crsf::kRcChannelsPayloadSize - 1> too_small{};
  EXPECT_FALSE(crsf::encode_rc_channels(channels, too_small));

  crsf::RcChannels decoded{};
  EXPECT_FALSE(crsf::decode_rc_channels(too_small, decoded));
}

TEST(RcChannels, CenteredChannelsCarryMidValue)
{
  const auto channels = crsf::make_centered_channels();
  for (const std::uint16_t channel : channels) {
    EXPECT_EQ(channel, crsf::kRcChannelMid);
  }
}

TEST(FrameBuilder, BuildsFrameThatParserAccepts)
{
  const std::array<std::uint8_t, 3> payload = {0xDE, 0xAD, 0xBE};
  std::array<std::uint8_t, crsf::kMaxFrameSize> frame{};
  const std::size_t size = crsf::build_frame(crsf::kAddressFlightController, 0x16, payload, frame);
  ASSERT_EQ(size, payload.size() + 4);

  crsf::Parser<> parser;
  auto status = crsf::ParseStatus::kIncomplete;
  for (std::size_t i = 0; i < size; ++i) {
    status = parser.feed(frame[i]);
  }

  EXPECT_EQ(status, crsf::ParseStatus::kFrameReady);
  EXPECT_EQ(parser.frame().address, crsf::kAddressFlightController);
  EXPECT_EQ(parser.frame().type, 0x16);
  ASSERT_EQ(parser.frame().payload.size(), payload.size());
  EXPECT_TRUE(std::equal(payload.begin(), payload.end(), parser.frame().payload.begin()));
}

// The full path a failsafe frame takes: channels -> frame -> wire -> parse -> channels.
TEST(FrameBuilder, RcChannelsSurviveFullBuildParseDecodeCycle)
{
  const auto channels = make_test_channels();
  std::array<std::uint8_t, crsf::kMaxFrameSize> frame{};
  const std::size_t size = crsf::build_rc_channels_frame(crsf::kAddressFlightController, channels, frame);
  ASSERT_EQ(size, 26U);

  crsf::Parser<> parser;
  auto status = crsf::ParseStatus::kIncomplete;
  for (std::size_t i = 0; i < size; ++i) {
    status = parser.feed(frame[i]);
  }
  ASSERT_EQ(status, crsf::ParseStatus::kFrameReady);
  EXPECT_EQ(parser.frame().type, static_cast<std::uint8_t>(crsf::FrameType::kRcChannelsPacked));

  crsf::RcChannels decoded{};
  ASSERT_TRUE(crsf::decode_rc_channels(parser.frame().payload, decoded));
  EXPECT_EQ(decoded, channels);
}

TEST(FrameBuilder, RejectsOversizedPayloadAndUndersizedOutput)
{
  const std::array<std::uint8_t, crsf::kMaxPayloadSize + 1> too_big{};
  std::array<std::uint8_t, crsf::kMaxFrameSize> frame{};
  EXPECT_EQ(crsf::build_frame(crsf::kAddressFlightController, 0x16, too_big, frame), 0U);

  const std::array<std::uint8_t, 3> payload = {0x01, 0x02, 0x03};
  std::array<std::uint8_t, 4> too_small{};
  EXPECT_EQ(crsf::build_frame(crsf::kAddressFlightController, 0x16, payload, too_small), 0U);
}

TEST(FrameBuilder, MaximumPayloadProducesMaximumFrame)
{
  const std::array<std::uint8_t, crsf::kMaxPayloadSize> payload{};
  std::array<std::uint8_t, crsf::kMaxFrameSize> frame{};
  const std::size_t size = crsf::build_frame(crsf::kAddressFlightController, 0x16, payload, frame);
  ASSERT_EQ(size, crsf::kMaxFrameSize);

  crsf::Parser<> parser;
  auto status = crsf::ParseStatus::kIncomplete;
  for (std::size_t i = 0; i < size; ++i) {
    status = parser.feed(frame[i]);
  }
  EXPECT_EQ(status, crsf::ParseStatus::kFrameReady);
}

TEST(FrameBuilder, BitShiftAndLutPoliciesProduceIdenticalFrames)
{
  const auto channels = make_test_channels();
  std::array<std::uint8_t, crsf::kMaxFrameSize> lut_frame{};
  std::array<std::uint8_t, crsf::kMaxFrameSize> shift_frame{};

  const std::size_t lut_size = crsf::build_rc_channels_frame<crsf::Crc8Dvb>(crsf::kAddressFlightController, channels, lut_frame);
  const std::size_t shift_size =
    crsf::build_rc_channels_frame<crsf::Crc8DvbBitShift>(crsf::kAddressFlightController, channels, shift_frame);

  ASSERT_EQ(lut_size, shift_size);
  EXPECT_EQ(lut_frame, shift_frame);
}

}  // namespace
