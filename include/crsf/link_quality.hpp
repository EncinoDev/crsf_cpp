// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#pragma once

#include <cstdint>

namespace crsf {

// Exponentially weighted moving average, alpha = 1 / 2^Shift. The accumulator holds the value
// scaled by 2^Shift, so the update needs no division and no float - some targets have no FPU.
template <int Shift, typename T = std::uint32_t>
class Ewma {
  static_assert(Shift > 0, "alpha = 1/2^Shift; Shift 0 would mean no filtering at all");
  static_assert(Shift < 16, "the accumulator scales by 2^Shift; keep it inside T");

public:
  constexpr void reset(T value) noexcept { accumulator_ = static_cast<T>(value << Shift); }

  constexpr T update(T sample) noexcept
  {
    accumulator_ = static_cast<T>(accumulator_ + sample - (accumulator_ >> Shift));
    return value();
  }

  // Decay stops subtracting below 2^Shift, so the accumulator floors there rather than at zero.
  // value() shifts that floor back to 0; a caller reading the accumulator itself inherits it.
  [[nodiscard]] constexpr T value() const noexcept { return static_cast<T>(accumulator_ >> Shift); }

private:
  T accumulator_ = 0;
};

inline constexpr std::uint8_t kLinkQualityMax = 100;

// Link quality in percent, starting at 0 rather than assuming a good link. The caller must drive
// on_missed() from its own timer, or a link that stops cleanly holds its last value forever.
template <int Shift = 4>
class LinkQuality {
public:
  constexpr void on_frame(bool crc_ok) noexcept { filter_.update(crc_ok ? kLinkQualityMax : 0U); }

  constexpr void on_missed() noexcept { filter_.update(0U); }

  constexpr void reset(std::uint8_t percent) noexcept { filter_.reset(percent); }

  [[nodiscard]] constexpr std::uint8_t percent() const noexcept { return static_cast<std::uint8_t>(filter_.value()); }

private:
  Ewma<Shift, std::uint32_t> filter_{};
};

}  // namespace crsf
