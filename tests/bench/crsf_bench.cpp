// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Bohdan Puhach

// Standalone benchmark: no external dependency, so the library stays extractable as-is.
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "crsf/crsf.hpp"

namespace {

constexpr int kIterations = 200000;

crsf::RcChannels make_channels()
{
  crsf::RcChannels channels{};
  for (std::size_t i = 0; i < channels.size(); ++i) {
    channels[i] = static_cast<std::uint16_t>(crsf::kRcChannelMin + i * 109);
  }
  return channels;
}

template <typename Policy>
void bench_crc(const char* label, std::span<const std::uint8_t> data)
{
  volatile std::uint8_t sink = 0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kIterations; ++i) {
    sink = crsf::crc8<Policy>(data);
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  (void)sink;

  const double seconds = std::chrono::duration<double>(elapsed).count();
  const double bytes = static_cast<double>(kIterations) * static_cast<double>(data.size());
  std::printf("  %-28s %8.2f MB/s  (%6.1f ns/frame)\n", label, bytes / seconds / 1e6, seconds / kIterations * 1e9);
}

template <typename Policy>
void bench_parse(const char* label, const std::vector<std::uint8_t>& frame)
{
  volatile int sink = 0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kIterations; ++i) {
    crsf::Parser<Policy> parser;
    crsf::RcChannels channels{};
    for (const std::uint8_t byte : frame) {
      if (parser.feed(byte) == crsf::ParseStatus::kFrameReady) {
        crsf::decode_rc_channels(parser.frame().payload, channels);
        sink = channels[0];
      }
    }
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  (void)sink;

  const double seconds = std::chrono::duration<double>(elapsed).count();
  std::printf("  %-28s %8.2f k frames/s  (%6.1f ns/frame)\n", label, kIterations / seconds / 1e3, seconds / kIterations * 1e9);
}

}  // namespace

int main()
{
  const auto channels = make_channels();
  std::array<std::uint8_t, crsf::kMaxFrameSize> built{};
  const std::size_t size = crsf::build_rc_channels_frame(crsf::kSyncByte, channels, built);
  const std::vector<std::uint8_t> frame(built.begin(), built.begin() + static_cast<std::ptrdiff_t>(size));

  std::printf("CRC8 over a %zu-byte RC frame body:\n", size - 3);
  const std::span<const std::uint8_t> body{frame.data() + 2, size - 3};
  bench_crc<crsf::Crc8Dvb>("lut policy", body);
  bench_crc<crsf::Crc8DvbBitShift>("bit-shift policy", body);

  std::printf("Full frame parse + channel decode:\n");
  bench_parse<crsf::Crc8Dvb>("lut policy", frame);
  bench_parse<crsf::Crc8DvbBitShift>("bit-shift policy", frame);
  return 0;
}
