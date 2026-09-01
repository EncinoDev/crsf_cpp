#pragma once

#include <cstddef>
#include <cstdint>

#include "crsf/protocol.hpp"

namespace crsf {

// 1 start + 8 data + 1 stop, no parity.
inline constexpr std::uint32_t kSerialBitsPerByte = 10;

// Microseconds `bytes` occupy on the wire at `baud`, rounded up. 0 baud yields 0.
constexpr std::uint32_t serial_time_us(std::size_t bytes, std::uint32_t baud) noexcept
{
  if (baud == 0) {
    return 0;
  }
  const std::uint64_t bits = static_cast<std::uint64_t>(bytes) * kSerialBitsPerByte;
  return static_cast<std::uint32_t>((bits * 1'000'000ULL + baud - 1) / baud);
}

inline constexpr std::uint32_t kFrameGapMarginPercent = 15;

// Idle time after which an arriving byte must start a new frame. Parser holds no clock: a caller
// wanting gap-based resync times this itself and calls Parser::reset().
constexpr std::uint32_t byte_gap_timeout_us(std::uint32_t baud) noexcept
{
  const std::uint32_t worst_case = serial_time_us(kMaxFrameSize, baud);
  return worst_case + (worst_case * kFrameGapMarginPercent) / 100;
}

}  // namespace crsf
