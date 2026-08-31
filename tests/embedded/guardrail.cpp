// Compiled only for the bare-metal target. The library is header-only, so without a translation
// unit that instantiates it nothing would actually be compiled for the MCU and ADR 0006's
// guarantees would go unverified. Odr-uses every public entry point; check_symbols.cmake then
// asserts the object pulls in no heap, exception, or RTTI machinery.
#include <array>
#include <cstddef>
#include <cstdint>

#include "crsf/crsf.hpp"

namespace {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables): mutable file-scope state is
// the point here. These force the compiler to emit real .bss and real code for the target, which
// is what the symbol gate then inspects; a const parser could not be fed a byte.
crsf::Parser<crsf::Crc8Dvb> g_lut_parser;
crsf::Parser<crsf::Crc8DvbBitShift> g_shift_parser;
crsf::RcChannels g_channels{};
std::array<std::uint8_t, crsf::kMaxFrameSize> g_frame{};
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
  g_channels = crsf::make_centered_channels();
  return crsf::build_rc_channels_frame<crsf::Crc8DvbBitShift>(crsf::kAddressFlightController, g_channels, g_frame);
}

extern "C" std::uint8_t crsf_guardrail_shift_parse(std::uint8_t byte)
{
  return static_cast<std::uint8_t>(g_shift_parser.feed(byte));
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

// Proves the CRC and packing paths are usable in constant expressions on the target too.
static_assert(crsf::Crc8Dvb::update(0, 0) == crsf::Crc8DvbBitShift::update(0, 0));
static_assert(crsf::make_centered_channels()[0] == crsf::kRcChannelMid);

// Wire-format conformance, evaluated by the compiler for whatever target it is building. The
// codec never type-puns or memcpys a multi-byte integer, so byte order cannot vary; these
// assertions are what turns that claim into something each target's toolchain re-checks.
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

static_assert(builds_expected_frame(), "wire format differs on this target");
static_assert(decodes_expected_frame(), "decode differs on this target");
