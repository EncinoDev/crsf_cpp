#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "crsf/protocol.hpp"
#include "crsf/rc_channels.hpp"

namespace crsf {

// What one channel holds once the link is declared lost. The policy is per-channel because "safe"
// is not the same value on every axis: a centred throttle is half power, while a centred steering
// axis is exactly right.
enum class FailsafeMode : std::uint8_t {
  kCenter,
  kMin,
  kMax,
  kHold,
  kValue,
};

struct FailsafeChannel {
  FailsafeMode mode = FailsafeMode::kCenter;
  std::uint16_t value = kRcChannelMid;  // read only when mode == kValue
};

using FailsafeConfig = std::array<FailsafeChannel, kRcChannelCount>;

constexpr std::uint16_t failsafe_value(const FailsafeChannel& channel, std::uint16_t last_valid) noexcept
{
  switch (channel.mode) {
    case FailsafeMode::kCenter:
      return kRcChannelMid;
    case FailsafeMode::kMin:
      return kRcChannelMin;
    case FailsafeMode::kMax:
      return kRcChannelMax;
    case FailsafeMode::kHold:
      return last_valid;
    case FailsafeMode::kValue:
      return channel.value;
  }
  return kRcChannelMid;
}

// `last_valid` is only read by kHold channels, but is required rather than defaulted so a config
// that holds cannot silently substitute a centre value. Initialise it to a safe frame at boot:
// before the first valid frame there is no "last valid" to hold.
constexpr RcChannels make_failsafe_channels(const FailsafeConfig& config, const RcChannels& last_valid) noexcept
{
  RcChannels out{};
  for (std::size_t i = 0; i < kRcChannelCount; ++i) {
    out[i] = failsafe_value(config[i], last_valid[i]);
  }
  return out;
}

// Index of the throttle channel under AETR, the order ELRS ships by default: 1 roll, 2 pitch,
// 3 throttle, 4 yaw. A transmitter set to TAER or a custom map puts it elsewhere, which is why
// the config is data rather than a hard-coded rule.
inline constexpr std::size_t kAetrThrottleIndex = 2;

// Centres everything except throttle, which drops to minimum. Safe default for an AETR source;
// deployments that differ override the affected entries.
constexpr FailsafeConfig make_aetr_failsafe_config() noexcept
{
  FailsafeConfig config{};
  config[kAetrThrottleIndex].mode = FailsafeMode::kMin;
  return config;
}

}  // namespace crsf
