#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "crsf/bit_packing.hpp"
#include "crsf/protocol.hpp"

namespace crsf {

using RcChannels = std::array<std::uint16_t, kRcChannelCount>;

constexpr bool decode_rc_channels(std::span<const std::uint8_t> payload, RcChannels& out) noexcept
{
  if (payload.size() < kRcChannelsPayloadSize) {
    return false;
  }
  BitReader reader{payload};
  for (std::uint16_t& channel : out) {
    channel = static_cast<std::uint16_t>(reader.read(kRcChannelBits));
  }
  return true;
}

// Channel values wider than 11 bits are masked by the writer, matching what a CRSF source puts on
// the wire; clamp beforehand if out-of-range input should be an error instead.
constexpr bool encode_rc_channels(const RcChannels& channels, std::span<std::uint8_t> out) noexcept
{
  if (out.size() < kRcChannelsPayloadSize) {
    return false;
  }
  BitWriter writer{out.first(kRcChannelsPayloadSize)};
  for (const std::uint16_t channel : channels) {
    if (!writer.write(channel, kRcChannelBits)) {
      return false;
    }
  }
  return writer.finish() == kRcChannelsPayloadSize;
}

constexpr RcChannels make_centered_channels() noexcept
{
  RcChannels channels{};
  for (std::uint16_t& channel : channels) {
    channel = kRcChannelMid;
  }
  return channels;
}

}  // namespace crsf
