#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace crsf {

inline constexpr std::uint8_t kCrc8PolyDvbS2 = 0xD5;

namespace detail {

constexpr std::uint8_t crc8_shift(std::uint8_t crc, std::uint8_t poly) noexcept
{
  return ((crc & 0x80) != 0) ? static_cast<std::uint8_t>((crc << 1) ^ poly) : static_cast<std::uint8_t>(crc << 1);
}

template <std::uint8_t Poly>
constexpr std::array<std::uint8_t, 256> make_crc8_table() noexcept
{
  std::array<std::uint8_t, 256> table{};
  for (int value = 0; value < 256; ++value) {
    auto crc = static_cast<std::uint8_t>(value);
    for (int bit = 0; bit < 8; ++bit) {
      crc = crc8_shift(crc, Poly);
    }
    table[static_cast<std::size_t>(value)] = crc;
  }
  return table;
}

}  // namespace detail

// Bit-by-bit policy: no table footprint, smaller code, slower per byte.
template <std::uint8_t Poly>
struct Crc8BitShiftPolicy {
  static constexpr std::uint8_t update(std::uint8_t crc, std::uint8_t byte) noexcept
  {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      crc = detail::crc8_shift(crc, Poly);
    }
    return crc;
  }
};

// inline variable template: one table per Poly, no per-TU duplication in this header-only lib.
template <std::uint8_t Poly>
inline constexpr std::array<std::uint8_t, 256> kCrc8Table = detail::make_crc8_table<Poly>();

template <std::uint8_t Poly>
struct Crc8LutPolicy {
  static constexpr std::uint8_t update(std::uint8_t crc, std::uint8_t byte) noexcept
  {
    return kCrc8Table<Poly>[static_cast<std::uint8_t>(crc ^ byte)];
  }
};

template <typename Policy>
constexpr std::uint8_t crc8(std::span<const std::uint8_t> data, std::uint8_t init = 0) noexcept
{
  std::uint8_t crc = init;
  for (std::uint8_t byte : data) {
    crc = Policy::update(crc, byte);
  }
  return crc;
}

using Crc8Dvb = Crc8LutPolicy<kCrc8PolyDvbS2>;
using Crc8DvbBitShift = Crc8BitShiftPolicy<kCrc8PolyDvbS2>;

}  // namespace crsf
