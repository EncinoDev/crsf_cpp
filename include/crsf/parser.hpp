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

// Payload views memory the parser does not own; see Parser::feed and parse() for its lifetime.
// `payload` stays whole on extended frames; the ext_* accessors read the routing fields out of it.
struct FrameView {
  std::uint8_t address = 0;
  std::uint8_t type = 0;
  std::span<const std::uint8_t> payload;

  [[nodiscard]] constexpr bool extended() const noexcept
  {
    return is_extended_frame_type(type) && payload.size() >= kExtendedAddressFieldsSize;
  }

  [[nodiscard]] constexpr std::uint8_t ext_destination() const noexcept { return extended() ? payload[0] : 0; }

  [[nodiscard]] constexpr std::uint8_t ext_origin() const noexcept { return extended() ? payload[1] : 0; }

  [[nodiscard]] constexpr std::span<const std::uint8_t> ext_payload() const noexcept
  {
    return extended() ? payload.subspan(kExtendedAddressFieldsSize) : std::span<const std::uint8_t>{};
  }
};

namespace detail {

constexpr bool length_valid(std::uint8_t length) noexcept
{
  return length >= kMinLengthFieldValue && length <= kMaxLengthFieldValue;
}

constexpr std::size_t frame_size_from_length(std::uint8_t length) noexcept
{
  return kAddressFieldSize + kLengthFieldSize + length;
}

// `frame` must be exactly one frame: address, length, type, payload, crc.
template <typename Crc8Policy>
constexpr bool crc_ok(std::span<const std::uint8_t> frame) noexcept
{
  const std::size_t crc_index = frame.size() - kCrcFieldSize;
  const std::size_t scope_offset = kAddressFieldSize + kLengthFieldSize;
  return crc8<Crc8Policy>(frame.subspan(scope_offset, crc_index - scope_offset)) == frame[crc_index];
}

constexpr FrameView make_frame_view(std::span<const std::uint8_t> frame) noexcept
{
  const std::size_t scope_offset = kAddressFieldSize + kLengthFieldSize;
  const std::size_t payload_offset = scope_offset + kTypeFieldSize;
  const std::size_t crc_index = frame.size() - kCrcFieldSize;
  return FrameView{frame[0], frame[scope_offset], frame.subspan(payload_offset, crc_index - payload_offset)};
}

}  // namespace detail

struct ParseResult {
  ParseStatus status = ParseStatus::kIncomplete;
  std::size_t consumed = 0;
  FrameView frame;
};

// Stateless parse over caller-owned bytes; `frame.payload` aliases `data`. `consumed` is 0 while
// more bytes are needed and 1 after a bad frame, so the caller slides a byte and retries.
// Choosing between this and Parser::feed: see README.
template <typename Crc8Policy = Crc8Dvb>
constexpr ParseResult parse(std::span<const std::uint8_t> data) noexcept
{
  if (data.size() < kAddressFieldSize + kLengthFieldSize) {
    return ParseResult{ParseStatus::kIncomplete, 0, FrameView{}};
  }

  const std::uint8_t length = data[1];
  if (!detail::length_valid(length)) {
    return ParseResult{ParseStatus::kInvalidLength, 1, FrameView{}};
  }

  const std::size_t frame_size = detail::frame_size_from_length(length);
  if (data.size() < frame_size) {
    return ParseResult{ParseStatus::kIncomplete, 0, FrameView{}};
  }

  const std::span<const std::uint8_t> frame = data.first(frame_size);
  if (!detail::crc_ok<Crc8Policy>(frame)) {
    return ParseResult{ParseStatus::kCrcMismatch, 1, FrameView{}};
  }
  return ParseResult{ParseStatus::kFrameReady, frame_size, detail::make_frame_view(frame)};
}

// Calls on_frame for each valid frame and on_crc_error for each byte rejected by its checksum;
// returns how many leading bytes may be discarded, leaving a trailing partial frame. Pure garbage
// holds bytes back, so the caller must cap its accumulator.
//
// A rejection is worth reporting because its absence is evidence too: a stream aligned to the right
// rate and polarity resyncs at frame boundaries and rejects nothing, while one that is not produces
// rejections continuously and passes a checksum only by chance. A caller that only counts the
// passes cannot tell those apart.
template <typename Crc8Policy = Crc8Dvb, typename OnFrame, typename OnCrcError>
constexpr std::size_t scan(std::span<const std::uint8_t> data, OnFrame&& on_frame, OnCrcError&& on_crc_error)
{
  std::size_t offset = 0;
  while (offset < data.size()) {
    const ParseResult result = parse<Crc8Policy>(data.subspan(offset));
    if (result.status == ParseStatus::kIncomplete) {
      break;
    }
    if (result.status == ParseStatus::kFrameReady) {
      on_frame(result.frame);
    }
    else if (result.status == ParseStatus::kCrcMismatch) {
      on_crc_error();
    }
    offset += result.consumed;
  }
  return offset;
}

template <typename Crc8Policy = Crc8Dvb, typename OnFrame>
constexpr std::size_t scan(std::span<const std::uint8_t> data, OnFrame&& on_frame)
{
  return scan<Crc8Policy>(data, on_frame, [] {});
}

// Byte-at-a-time parser for a per-byte-interrupt UART, where no contiguous span exists yet. Holds
// no clock: a caller needing gap-based resync runs its own timer and calls reset().
template <typename Crc8Policy = Crc8Dvb>
class Parser {
public:
  // frame().payload views this parser's buffer and dies on the next feed().
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
      if (!detail::length_valid(buffer_[1])) {
        position_ = 0;
        return ParseStatus::kInvalidLength;
      }
      frame_size_ = detail::frame_size_from_length(buffer_[1]);
    }

    if (position_ < frame_size_) {
      return ParseStatus::kIncomplete;
    }

    position_ = 0;
    const std::span<const std::uint8_t> frame{buffer_.data(), frame_size_};
    if (!detail::crc_ok<Crc8Policy>(frame)) {
      return ParseStatus::kCrcMismatch;
    }
    frame_ = detail::make_frame_view(frame);
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
