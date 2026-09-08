// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "crsf/crsf.hpp"

namespace {

// [ext_dest][ext_origin][field_idx][chunk], the shape a PARAMETER_READ carries.
std::vector<std::uint8_t> extended_frame(std::uint8_t type, std::uint8_t dest, std::uint8_t origin)
{
  const std::array<std::uint8_t, 4> payload{dest, origin, 0x0A, 0x00};
  std::array<std::uint8_t, crsf::kMaxFrameSize> buffer{};
  const std::size_t size = crsf::build_frame(dest, type, payload, buffer);
  return {buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(size)};
}

}  // namespace

TEST(ExtendedFrame, TypeRangeMatchesTheProtocol)
{
  EXPECT_FALSE(crsf::is_extended_frame_type(crsf::FrameType::kRcChannelsPacked));
  EXPECT_FALSE(crsf::is_extended_frame_type(crsf::FrameType::kLinkStatistics));
  EXPECT_TRUE(crsf::is_extended_frame_type(crsf::FrameType::kDevicePing));
  EXPECT_TRUE(crsf::is_extended_frame_type(crsf::FrameType::kParameterWrite));

  EXPECT_FALSE(crsf::is_extended_frame_type(static_cast<std::uint8_t>(crsf::kExtendedFrameTypeMin - 1)));
  EXPECT_TRUE(crsf::is_extended_frame_type(crsf::kExtendedFrameTypeMin));
  EXPECT_TRUE(crsf::is_extended_frame_type(crsf::kExtendedFrameTypeMax));
  EXPECT_FALSE(crsf::is_extended_frame_type(static_cast<std::uint8_t>(crsf::kExtendedFrameTypeMax + 1)));
}

TEST(ExtendedFrame, ParseExposesRoutingAddresses)
{
  const auto frame =
    extended_frame(static_cast<std::uint8_t>(crsf::FrameType::kParameterRead), crsf::kAddressReserved2, crsf::kAddressRadioTransmitter);
  const auto result = crsf::parse(frame);

  ASSERT_EQ(result.status, crsf::ParseStatus::kFrameReady);
  EXPECT_TRUE(result.frame.extended());
  EXPECT_EQ(result.frame.ext_destination(), crsf::kAddressReserved2);
  EXPECT_EQ(result.frame.ext_origin(), crsf::kAddressRadioTransmitter);
}

// payload stays whole so an ext-unaware caller sees the frame unchanged.
TEST(ExtendedFrame, PayloadKeepsTheRoutingBytesAndExtPayloadDropsThem)
{
  const auto frame =
    extended_frame(static_cast<std::uint8_t>(crsf::FrameType::kParameterRead), crsf::kAddressReserved2, crsf::kAddressRadioTransmitter);
  const auto result = crsf::parse(frame);

  ASSERT_EQ(result.status, crsf::ParseStatus::kFrameReady);
  EXPECT_EQ(result.frame.payload.size(), 4U);
  ASSERT_EQ(result.frame.ext_payload().size(), 2U);
  EXPECT_EQ(result.frame.ext_payload()[0], 0x0A);
  EXPECT_EQ(result.frame.ext_payload().data(), result.frame.payload.data() + crsf::kExtendedAddressFieldsSize);
}

TEST(ExtendedFrame, NonExtendedFrameReportsNoRouting)
{
  std::array<std::uint8_t, crsf::kMaxFrameSize> buffer{};
  const std::size_t size = crsf::build_rc_channels_frame(crsf::kSyncByte, crsf::make_centered_channels(), buffer);
  const auto result = crsf::parse(std::span<const std::uint8_t>{buffer.data(), size});

  ASSERT_EQ(result.status, crsf::ParseStatus::kFrameReady);
  EXPECT_FALSE(result.frame.extended());
  EXPECT_EQ(result.frame.ext_destination(), 0);
  EXPECT_TRUE(result.frame.ext_payload().empty());
}

// Type claims extended addressing, payload too short to hold it: report nothing, never over-read.
TEST(ExtendedFrame, TruncatedRoutingFieldsAreRefusedNotRead)
{
  const std::array<std::uint8_t, 1> payload{0xCA};
  std::array<std::uint8_t, crsf::kMaxFrameSize> buffer{};
  const std::size_t size =
    crsf::build_frame(crsf::kAddressReserved2, static_cast<std::uint8_t>(crsf::FrameType::kDevicePing), payload, buffer);
  const auto result = crsf::parse(std::span<const std::uint8_t>{buffer.data(), size});

  ASSERT_EQ(result.status, crsf::ParseStatus::kFrameReady);
  EXPECT_FALSE(result.frame.extended());
  EXPECT_EQ(result.frame.ext_destination(), 0);
  EXPECT_EQ(result.frame.ext_origin(), 0);
  EXPECT_TRUE(result.frame.ext_payload().empty());
}
