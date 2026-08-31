#include <cstddef>
#include <cstdint>

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
  return 0;
}
