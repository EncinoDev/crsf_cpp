#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "crsf/crc8.hpp"
#include "crsf/protocol.hpp"
#include "crsf/rc_channels.hpp"

namespace crsf {

// Writes <address><length><type><payload><crc> into `out`. Returns bytes written, or 0 if the
// payload exceeds the protocol maximum or `out` is too small.
template <typename Crc8Policy = Crc8Dvb>
constexpr std::size_t build_frame(std::uint8_t address,
                                  std::uint8_t type,
                                  std::span<const std::uint8_t> payload,
                                  std::span<std::uint8_t> out) noexcept
{
  if (payload.size() > kMaxPayloadSize) {
    return 0;
  }
  const std::size_t frame_size = payload.size() + kAddressFieldSize + kLengthFieldSize + kTypeFieldSize + kCrcFieldSize;
  if (out.size() < frame_size) {
    return 0;
  }

  out[0] = address;
  out[1] = static_cast<std::uint8_t>(payload.size() + kTypeFieldSize + kCrcFieldSize);
  out[2] = type;
  for (std::size_t i = 0; i < payload.size(); ++i) {
    out[3 + i] = payload[i];
  }

  const std::size_t crc_scope_offset = kAddressFieldSize + kLengthFieldSize;
  out[frame_size - 1] = crc8<Crc8Policy>(out.subspan(crc_scope_offset, kTypeFieldSize + payload.size()));
  return frame_size;
}

template <typename Crc8Policy = Crc8Dvb>
constexpr std::size_t build_rc_channels_frame(std::uint8_t address, const RcChannels& channels, std::span<std::uint8_t> out) noexcept
{
  std::array<std::uint8_t, kRcChannelsPayloadSize> payload{};
  if (!encode_rc_channels(channels, payload)) {
    return 0;
  }
  return build_frame<Crc8Policy>(address, static_cast<std::uint8_t>(FrameType::kRcChannelsPacked), payload, out);
}

}  // namespace crsf
