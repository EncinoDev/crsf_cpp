// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#pragma once

#include <cstddef>
#include <cstdint>

namespace crsf {

inline constexpr std::uint8_t kSyncByte = 0xC8;

inline constexpr std::size_t kAddressFieldSize = 1;
inline constexpr std::size_t kLengthFieldSize = 1;
inline constexpr std::size_t kTypeFieldSize = 1;
inline constexpr std::size_t kCrcFieldSize = 1;

inline constexpr std::size_t kMaxFrameSize = 64;

// Value the length byte declares: covers type + payload + crc (not address or itself).
inline constexpr std::uint8_t kMinLengthFieldValue = kTypeFieldSize + kCrcFieldSize;
inline constexpr std::uint8_t kMaxLengthFieldValue = static_cast<std::uint8_t>(kMaxFrameSize - kAddressFieldSize - kLengthFieldSize);
// 60: what the framing arithmetic leaves. Betaflight says 58 because it also subtracts the two
// extended-header routing bytes unconditionally; here those belong to the payload of the types
// that carry them, so the deduction is made per type rather than for all of them.
inline constexpr std::size_t kMaxPayloadSize = kMaxLengthFieldValue - kTypeFieldSize - kCrcFieldSize;

enum class FrameType : std::uint8_t {
  kLinkStatistics = 0x14,
  kRcChannelsPacked = 0x16,
  kDevicePing = 0x28,
  kDeviceInfo = 0x29,
  kParameterSettingsEntry = 0x2B,
  kParameterRead = 0x2C,
  kParameterWrite = 0x2D,
};

// Types in this range carry a destination and an origin address ahead of the payload.
inline constexpr std::uint8_t kExtendedFrameTypeMin = 0x28;
inline constexpr std::uint8_t kExtendedFrameTypeMax = 0x96;
inline constexpr std::size_t kExtendedAddressFieldsSize = 2;

constexpr bool is_extended_frame_type(std::uint8_t type) noexcept
{
  return type >= kExtendedFrameTypeMin && type <= kExtendedFrameTypeMax;
}

constexpr bool is_extended_frame_type(FrameType type) noexcept
{
  return is_extended_frame_type(static_cast<std::uint8_t>(type));
}

inline constexpr std::uint8_t kAddressBroadcast = 0x00;
inline constexpr std::uint8_t kAddressFlightController = 0xC8;
inline constexpr std::uint8_t kAddressReserved2 = 0xCA;
inline constexpr std::uint8_t kAddressRadioTransmitter = 0xEA;
inline constexpr std::uint8_t kAddressCrsfReceiver = 0xEC;
inline constexpr std::uint8_t kAddressCrsfTransmitter = 0xEE;

inline constexpr int kRcChannelCount = 16;
inline constexpr int kRcChannelBits = 11;
inline constexpr std::size_t kRcChannelsPayloadSize = (kRcChannelCount * kRcChannelBits) / 8;  // 22

// Raw 11-bit tick values a CRSF source emits for the conventional 988/1500/2012 us stick positions.
inline constexpr std::uint16_t kRcChannelMin = 172;
inline constexpr std::uint16_t kRcChannelMid = 992;
inline constexpr std::uint16_t kRcChannelMax = 1811;

}  // namespace crsf
