// Compiled only for the bare-metal target. Odr-uses every public entry point so a header-only
// library actually gets compiled for the MCU and check_symbols.cmake has an object to inspect.
#include <array>
#include <cstddef>
#include <cstdint>

#include "crsf/crsf.hpp"

namespace {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): mutable state is the point; it
// forces real .bss and real code for the target, which is what the symbol gate inspects.
crsf::Parser<crsf::Crc8Dvb> g_lut_parser;
crsf::Parser<crsf::Crc8DvbBitShift> g_shift_parser;
crsf::RcChannels g_channels{};
std::array<std::uint8_t, crsf::kMaxFrameSize> g_frame{};
crsf::LinkQuality<4> g_quality{};
std::uint8_t g_ext_origin = 0;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

extern "C" std::size_t crsf_guardrail_ingest(std::uint8_t byte)
{
  if (g_lut_parser.feed(byte) != crsf::ParseStatus::kFrameReady) {
    return 0;
  }
  if (!crsf::decode_rc_channels(g_lut_parser.frame().payload, g_channels)) {
    return 0;
  }
  return crsf::build_rc_channels_frame(crsf::kSyncByte, g_channels, g_frame);
}

extern "C" std::size_t crsf_guardrail_inject(void)
{
  g_channels = crsf::make_failsafe_channels(crsf::make_aetr_failsafe_config(), g_channels);
  return crsf::build_rc_channels_frame<crsf::Crc8DvbBitShift>(crsf::kAddressFlightController, g_channels, g_frame);
}

extern "C" std::uint8_t crsf_guardrail_shift_parse(std::uint8_t byte)
{
  return static_cast<std::uint8_t>(g_shift_parser.feed(byte));
}

// The stateless span entry point: nothing here holds the bytes but the caller.
extern "C" std::size_t crsf_guardrail_span_parse(const std::uint8_t* data, std::size_t size)
{
  const auto result = crsf::parse(std::span<const std::uint8_t>{data, size});
  if (result.status != crsf::ParseStatus::kFrameReady) {
    return 0;
  }
  return crsf::decode_rc_channels(result.frame.payload, g_channels) ? result.consumed : 0;
}

// scan() takes a callable: proves the lambda path allocates nothing on the target either.
extern "C" std::size_t crsf_guardrail_scan(const std::uint8_t* data, std::size_t size)
{
  std::size_t frames = 0;
  const std::size_t consumed = crsf::scan(std::span<const std::uint8_t>{data, size}, [&frames](const crsf::FrameView& frame) {
    g_ext_origin = frame.ext_origin();
    frames += crsf::decode_rc_channels(frame.payload, g_channels) ? 1U : 0U;
  });
  return consumed + frames;
}

extern "C" std::uint32_t crsf_guardrail_gap_timeout(std::uint32_t baud)
{
  return crsf::byte_gap_timeout_us(baud);
}

extern "C" std::uint8_t crsf_guardrail_link_quality(int received, int missed)
{
  for (int i = 0; i < received; ++i) {
    g_quality.on_frame(true);
  }
  for (int i = 0; i < missed; ++i) {
    g_quality.on_missed();
  }
  return g_quality.percent();
}

extern "C" void crsf_guardrail_reset(void)
{
  g_lut_parser.reset();
  g_shift_parser.reset();
}

// Bit primitives are exercised directly too: RC channels alone never hit the non-11-bit widths.
extern "C" std::uint32_t crsf_guardrail_bits(const std::uint8_t* data, std::size_t size, int width)
{
  crsf::BitReader reader{std::span<const std::uint8_t>{data, size}};
  const std::uint32_t value = reader.read(width);

  crsf::BitWriter writer{g_frame};
  writer.write(value, width);
  return static_cast<std::uint32_t>(writer.finish()) + (reader.exhausted() ? 0U : 1U);
}

// The CRC and packing paths must also work in constant expressions on the target.
static_assert(crsf::Crc8Dvb::update(0, 0) == crsf::Crc8DvbBitShift::update(0, 0));
static_assert(crsf::make_centered_channels()[0] == crsf::kRcChannelMid);
static_assert(crsf::make_failsafe_channels(crsf::make_aetr_failsafe_config(), crsf::make_centered_channels())[crsf::kAetrThrottleIndex] ==
              crsf::kRcChannelMin);

// Wire-format conformance, re-checked by whichever target's compiler builds this file.
namespace {

constexpr std::array<std::uint8_t, 26> kCenteredFrame{0xC8, 0x18, 0x16, 0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0, 0x81, 0x0F,
                                                      0x7C, 0xE0, 0x03, 0x1F, 0xF8, 0xC0, 0x07, 0x3E, 0xF0, 0x81, 0x0F, 0x7C, 0xAD};

constexpr bool builds_expected_frame()
{
  std::array<std::uint8_t, crsf::kMaxFrameSize> out{};
  if (crsf::build_rc_channels_frame(crsf::kSyncByte, crsf::make_centered_channels(), out) != kCenteredFrame.size()) {
    return false;
  }
  for (std::size_t i = 0; i < kCenteredFrame.size(); ++i) {
    if (out[i] != kCenteredFrame[i]) {
      return false;
    }
  }
  return true;
}

constexpr bool scans_expected_frame()
{
  std::size_t frames = 0;
  const std::size_t consumed = crsf::scan(kCenteredFrame, [&frames](const crsf::FrameView&) { ++frames; });
  return frames == 1 && consumed == kCenteredFrame.size();
}

constexpr bool decodes_expected_frame()
{
  crsf::Parser<> parser;
  auto status = crsf::ParseStatus::kIncomplete;
  for (const std::uint8_t byte : kCenteredFrame) {
    status = parser.feed(byte);
  }
  if (status != crsf::ParseStatus::kFrameReady) {
    return false;
  }
  crsf::RcChannels channels{};
  if (!crsf::decode_rc_channels(parser.frame().payload, channels)) {
    return false;
  }
  for (const std::uint16_t channel : channels) {
    if (channel != crsf::kRcChannelMid) {
      return false;
    }
  }
  return true;
}

}  // namespace

static_assert(crsf::parse(kCenteredFrame).status == crsf::ParseStatus::kFrameReady, "span parse differs on this target");
static_assert(crsf::parse(kCenteredFrame).consumed == kCenteredFrame.size());

static_assert(scans_expected_frame(), "scan differs on this target");
static_assert(!crsf::parse(kCenteredFrame).frame.extended(), "rc frames carry no routing addresses");
static_assert(crsf::byte_gap_timeout_us(420000) > crsf::serial_time_us(crsf::kMaxFrameSize, 420000));

static_assert(builds_expected_frame(), "wire format differs on this target");
static_assert(decodes_expected_frame(), "decode differs on this target");
