#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "crsf/crsf.hpp"

namespace {

crsf::RcChannels distinct_channels()
{
  crsf::RcChannels channels{};
  for (std::size_t i = 0; i < crsf::kRcChannelCount; ++i) {
    channels[i] = static_cast<std::uint16_t>(crsf::kRcChannelMin + i * 100);
  }
  return channels;
}

}  // namespace

TEST(Failsafe, AetrDefaultDropsThrottleAndCentresEverythingElse)
{
  const auto channels = crsf::make_failsafe_channels(crsf::make_aetr_failsafe_config(), distinct_channels());

  EXPECT_EQ(channels[crsf::kAetrThrottleIndex], crsf::kRcChannelMin);
  for (std::size_t i = 0; i < crsf::kRcChannelCount; ++i) {
    if (i != crsf::kAetrThrottleIndex) {
      EXPECT_EQ(channels[i], crsf::kRcChannelMid) << "channel " << i;
    }
  }
}

// The trap this module exists to prevent: a centred frame leaves the throttle at half power, so it
// is not a substitute for a failsafe frame.
TEST(Failsafe, CentredChannelsAreNotAFailsafeFrame)
{
  const auto centered = crsf::make_centered_channels();
  const auto failsafe = crsf::make_failsafe_channels(crsf::make_aetr_failsafe_config(), centered);

  EXPECT_NE(failsafe[crsf::kAetrThrottleIndex], centered[crsf::kAetrThrottleIndex]);
  EXPECT_EQ(centered[crsf::kAetrThrottleIndex], crsf::kRcChannelMid);
}

TEST(Failsafe, EachModeResolvesToItsValue)
{
  const crsf::RcChannels last = distinct_channels();
  crsf::FailsafeConfig config{};
  config[0].mode = crsf::FailsafeMode::kCenter;
  config[1].mode = crsf::FailsafeMode::kMin;
  config[2].mode = crsf::FailsafeMode::kMax;
  config[3].mode = crsf::FailsafeMode::kHold;
  config[4].mode = crsf::FailsafeMode::kValue;
  config[4].value = 1234;

  const auto channels = crsf::make_failsafe_channels(config, last);

  EXPECT_EQ(channels[0], crsf::kRcChannelMid);
  EXPECT_EQ(channels[1], crsf::kRcChannelMin);
  EXPECT_EQ(channels[2], crsf::kRcChannelMax);
  EXPECT_EQ(channels[3], last[3]);
  EXPECT_EQ(channels[4], 1234);
}

TEST(Failsafe, HoldIsTheOnlyModeThatReadsLastValid)
{
  crsf::FailsafeConfig config{};
  config[0].mode = crsf::FailsafeMode::kHold;

  const auto first = crsf::make_failsafe_channels(config, distinct_channels());
  const auto second = crsf::make_failsafe_channels(config, crsf::make_centered_channels());

  EXPECT_NE(first[0], second[0]);
  EXPECT_EQ(first[1], second[1]);
}

TEST(Failsafe, DefaultConstructedConfigCentresEveryChannel)
{
  const crsf::FailsafeConfig config{};
  const auto channels = crsf::make_failsafe_channels(config, distinct_channels());

  for (const std::uint16_t channel : channels) {
    EXPECT_EQ(channel, crsf::kRcChannelMid);
  }
}

TEST(Failsafe, FrameSurvivesBuildParseDecodeCycle)
{
  const auto expected = crsf::make_failsafe_channels(crsf::make_aetr_failsafe_config(), distinct_channels());

  std::array<std::uint8_t, crsf::kMaxFrameSize> frame{};
  const std::size_t size = crsf::build_rc_channels_frame(crsf::kSyncByte, expected, frame);
  ASSERT_GT(size, 0U);

  crsf::Parser<> parser;
  auto status = crsf::ParseStatus::kIncomplete;
  for (std::size_t i = 0; i < size; ++i) {
    status = parser.feed(frame[i]);
  }
  ASSERT_EQ(status, crsf::ParseStatus::kFrameReady);

  crsf::RcChannels decoded{};
  ASSERT_TRUE(crsf::decode_rc_channels(parser.frame().payload, decoded));
  EXPECT_EQ(decoded, expected);
}

TEST(Failsafe, IsUsableInConstexprContext)
{
  static_assert(crsf::make_aetr_failsafe_config()[crsf::kAetrThrottleIndex].mode == crsf::FailsafeMode::kMin);
  static_assert(crsf::make_failsafe_channels(crsf::make_aetr_failsafe_config(), crsf::make_centered_channels())[crsf::kAetrThrottleIndex] ==
                crsf::kRcChannelMin);
  SUCCEED();
}
