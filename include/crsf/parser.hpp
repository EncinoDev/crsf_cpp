#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "crsf/crc8.hpp"
#include "crsf/protocol.hpp"

namespace crsf {

enum class ParseStatus : std::uint8_t {
  kIncomplete,
  kFrameReady,
  kInvalidLength,
  kCrcMismatch,
};

// Payload views into the parser's buffer and stays valid only until the next feed().
struct FrameView {
  std::uint8_t address = 0;
  std::uint8_t type = 0;
  std::span<const std::uint8_t> payload;
};

// Byte-at-a-time CRSF frame parser. Holds no clock: a caller that needs gap-based resync runs its
// own timer and calls reset(), which keeps this usable on any platform.
template <typename Crc8Policy = Crc8Dvb>
class Parser {
public:
  constexpr ParseStatus feed(std::uint8_t byte) noexcept
  {
    if (position_ >= buffer_.size()) {
      position_ = 0;
    }
    buffer_[position_++] = byte;

    if (position_ < kAddressFieldSize + kLengthFieldSize) {
      return ParseStatus::kIncomplete;
    }

    if (position_ == kAddressFieldSize + kLengthFieldSize) {
      const std::uint8_t length = buffer_[1];
      if (length < kMinLengthFieldValue || length > kMaxLengthFieldValue) {
        position_ = 0;
        return ParseStatus::kInvalidLength;
      }
      frame_size_ = kAddressFieldSize + kLengthFieldSize + length;
    }

    if (position_ < frame_size_) {
      return ParseStatus::kIncomplete;
    }

    position_ = 0;
    const std::size_t crc_index = frame_size_ - kCrcFieldSize;
    const std::size_t crc_scope_offset = kAddressFieldSize + kLengthFieldSize;
    const std::span<const std::uint8_t> crc_scope{buffer_.data() + crc_scope_offset, crc_index - crc_scope_offset};

    if (crc8<Crc8Policy>(crc_scope) != buffer_[crc_index]) {
      return ParseStatus::kCrcMismatch;
    }

    const std::size_t payload_offset = crc_scope_offset + kTypeFieldSize;
    frame_.address = buffer_[0];
    frame_.type = buffer_[crc_scope_offset];
    frame_.payload = std::span<const std::uint8_t>{buffer_.data() + payload_offset, crc_index - payload_offset};
    return ParseStatus::kFrameReady;
  }

  [[nodiscard]] constexpr const FrameView& frame() const noexcept { return frame_; }

  constexpr void reset() noexcept
  {
    position_ = 0;
    frame_size_ = 0;
  }

private:
  std::array<std::uint8_t, kMaxFrameSize> buffer_{};
  std::size_t position_ = 0;
  std::size_t frame_size_ = 0;
  FrameView frame_{};
};

}  // namespace crsf
