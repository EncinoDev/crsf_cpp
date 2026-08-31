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
inline constexpr std::size_t kMaxPayloadSize = kMaxLengthFieldValue - kTypeFieldSize - kCrcFieldSize;

enum class FrameType : std::uint8_t {
  kRcChannelsPacked = 0x16,
};

inline constexpr std::uint8_t kAddressBroadcast = 0x00;
inline constexpr std::uint8_t kAddressFlightController = 0xC8;

inline constexpr int kRcChannelCount = 16;
inline constexpr int kRcChannelBits = 11;
inline constexpr std::size_t kRcChannelsPayloadSize = (kRcChannelCount * kRcChannelBits) / 8;  // 22

// Raw 11-bit tick values a CRSF source emits for the conventional 988/1500/2012 us stick positions.
inline constexpr std::uint16_t kRcChannelMin = 172;
inline constexpr std::uint16_t kRcChannelMid = 992;
inline constexpr std::uint16_t kRcChannelMax = 1811;

}  // namespace crsf
