// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace crsf {

// CRSF packs channels as one continuous LSB-first bitstream: bit 0 is the LSB of byte 0, a value
// starts where the previous one ended, and values straddle byte boundaries.
class BitReader {
public:
  explicit constexpr BitReader(std::span<const std::uint8_t> data) noexcept
    : data_(data)
  {
  }

  // Bits past the end of the span read as zero; exhausted() reports whether that happened.
  constexpr std::uint32_t read(int bits) noexcept
  {
    while (pending_bits_ < bits) {
      if (byte_index_ >= data_.size()) {
        exhausted_ = true;
        break;
      }
      pending_ |= static_cast<std::uint64_t>(data_[byte_index_++]) << pending_bits_;
      pending_bits_ += 8;
    }

    const std::uint32_t mask = (bits >= 32) ? 0xFFFFFFFFU : ((1U << bits) - 1U);
    const auto value = static_cast<std::uint32_t>(pending_) & mask;
    const int consumed = (bits < pending_bits_) ? bits : pending_bits_;
    pending_ >>= consumed;
    pending_bits_ -= consumed;
    return value;
  }

  [[nodiscard]] constexpr bool exhausted() const noexcept { return exhausted_; }

private:
  std::span<const std::uint8_t> data_;
  std::uint64_t pending_ = 0;
  int pending_bits_ = 0;
  std::size_t byte_index_ = 0;
  bool exhausted_ = false;
};

class BitWriter {
public:
  explicit constexpr BitWriter(std::span<std::uint8_t> out) noexcept
    : out_(out)
  {
  }

  // Values wider than `bits` are masked. Returns false once the destination cannot hold the
  // stream: overflow is sticky, so a truncated stream can never look successful.
  constexpr bool write(std::uint32_t value, int bits) noexcept
  {
    if (overflowed_) {
      return false;
    }

    const std::uint32_t mask = (bits >= 32) ? 0xFFFFFFFFU : ((1U << bits) - 1U);
    pending_ |= static_cast<std::uint64_t>(value & mask) << pending_bits_;
    pending_bits_ += bits;

    while (pending_bits_ >= 8) {
      if (byte_index_ >= out_.size()) {
        overflowed_ = true;
        return false;
      }
      out_[byte_index_++] = static_cast<std::uint8_t>(pending_ & 0xFFU);
      pending_ >>= 8;
      pending_bits_ -= 8;
    }

    if (pending_bits_ > static_cast<int>((out_.size() - byte_index_) * 8)) {
      overflowed_ = true;
      return false;
    }
    return true;
  }

  // Emits any partial trailing byte zero-padded; returns bytes written.
  constexpr std::size_t finish() noexcept
  {
    if (pending_bits_ > 0) {
      if (byte_index_ >= out_.size()) {
        overflowed_ = true;
        return byte_index_;
      }
      out_[byte_index_++] = static_cast<std::uint8_t>(pending_ & 0xFFU);
      pending_ = 0;
      pending_bits_ = 0;
    }
    return byte_index_;
  }

  [[nodiscard]] constexpr bool overflowed() const noexcept { return overflowed_; }

private:
  std::span<std::uint8_t> out_;
  std::uint64_t pending_ = 0;
  int pending_bits_ = 0;
  std::size_t byte_index_ = 0;
  bool overflowed_ = false;
};

}  // namespace crsf
