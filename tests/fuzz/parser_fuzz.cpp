#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "crsf/crsf.hpp"

// Drives the parser with arbitrary bytes: no input may cause a crash, an out-of-bounds access, or
// undefined behaviour, and a frame the parser accepts must survive a rebuild/re-parse cycle.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  crsf::Parser<> parser;
  crsf::RcChannels channels{};

  for (std::size_t i = 0; i < size; ++i) {
    if (parser.feed(data[i]) != crsf::ParseStatus::kFrameReady) {
      continue;
    }

    const auto& frame = parser.frame();
    if (frame.type != static_cast<std::uint8_t>(crsf::FrameType::kRcChannelsPacked)) {
      continue;
    }
    if (!crsf::decode_rc_channels(frame.payload, channels)) {
      continue;
    }

    std::array<std::uint8_t, crsf::kMaxFrameSize> rebuilt{};
    const std::size_t rebuilt_size = crsf::build_rc_channels_frame(frame.address, channels, rebuilt);
    if (rebuilt_size == 0) {
      __builtin_trap();
    }

    crsf::Parser<> verifier;
    auto status = crsf::ParseStatus::kIncomplete;
    for (std::size_t j = 0; j < rebuilt_size; ++j) {
      status = verifier.feed(rebuilt[j]);
    }
    if (status != crsf::ParseStatus::kFrameReady) {
      __builtin_trap();
    }

    crsf::RcChannels round_tripped{};
    if (!crsf::decode_rc_channels(verifier.frame().payload, round_tripped) || round_tripped != channels) {
      __builtin_trap();
    }
  }
  // Recovery differs from feed() by design, so the two are not asserted to agree; what must hold is
  // that parse() never walks outside the caller's buffer.
  std::span<const std::uint8_t> remaining{data, size};
  while (!remaining.empty()) {
    const auto result = crsf::parse(remaining);
    if (result.status == crsf::ParseStatus::kIncomplete) {
      break;
    }
    if (result.consumed == 0 || result.consumed > remaining.size()) {
      __builtin_trap();
    }
    if (result.status == crsf::ParseStatus::kFrameReady) {
      const std::uint8_t* payload = result.frame.payload.data();
      if (payload < remaining.data() || payload + result.frame.payload.size() > data + size) {
        __builtin_trap();
      }
      const auto verify = crsf::parse(remaining.first(result.consumed));
      if (verify.status != crsf::ParseStatus::kFrameReady || verify.consumed != result.consumed) {
        __builtin_trap();
      }
    }
    remaining = remaining.subspan(result.consumed);
  }

  // scan() must agree with the hand loop above, and hand out no view outside the caller's buffer.
  const std::size_t expected = size - remaining.size();
  const std::size_t scanned = crsf::scan(std::span<const std::uint8_t>{data, size}, [data, size](const crsf::FrameView& frame) {
    if (frame.payload.data() < data || frame.payload.data() + frame.payload.size() > data + size) {
      __builtin_trap();
    }
    const auto ext = frame.ext_payload();
    if (!ext.empty() && (ext.data() < data || ext.data() + ext.size() > data + size)) {
      __builtin_trap();
    }
  });
  if (scanned != expected) {
    __builtin_trap();
  }

  return 0;
}
