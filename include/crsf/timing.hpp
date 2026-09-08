// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

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

// Idle time that ends a frame: one character time, what a USART IDLE line interrupt reports.
// 24 us at 420000 baud.
constexpr std::uint32_t idle_gap_timeout_us(std::uint32_t baud) noexcept
{
  return serial_time_us(1, baud);
}

// Time to allow a whole frame to arrive before abandoning it, a maximal frame plus 15%: 1752 us at
// 420000 baud. Seventy times `idle_gap_timeout_us` — the two measure different events. Parser holds
// no clock: a caller wanting timeout-based resync times this itself and calls Parser::reset().
constexpr std::uint32_t frame_assembly_timeout_us(std::uint32_t baud) noexcept
{
  const std::uint32_t worst_case = serial_time_us(kMaxFrameSize, baud);
  return worst_case + (worst_case * kFrameGapMarginPercent) / 100;
}

}  // namespace crsf
