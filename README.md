# crsf_cpp

Header-only C++20 implementation of the CRSF (Crossfire) serial protocol: frame parsing, frame
construction, CRC8, and RC channel packing.

Hardware-independent by design — no HAL, no OS, no heap, no exceptions, no RTTI, no global state,
and no clock dependency. It is a pure protocol library: bytes in, bytes out. The same headers build
unmodified for any bare-metal MCU (STM32, ESP32, nRF, RP2040, …), a Linux SBC, and host unit tests,
on both byte orders.

---

## Design guarantees

These are contractual; the test suite and CI enforce them.

| Guarantee | Detail |
| --- | --- |
| No dynamic allocation | No `new`/`malloc` on any path. A build step runs `nm` over the compiled bare-metal object and fails on any heap, exception, or RTTI symbol. |
| No exceptions, no RTTI | Builds with `-fno-exceptions -fno-rtti`. All errors are return values. |
| No global mutable state | Every parser owns its buffer; multiple instances are independent. |
| No clock dependency | Parsing is a pure function of the byte sequence; `timing.hpp` supplies gap thresholds as values, never a timer (see [Timeouts](#timeouts-and-resynchronisation)). |
| Bounded execution | No unbounded loops; work per byte is constant, per frame is linear in frame length. |
| Memory-safe on malformed input | Out-of-range reads return zeros; writes refuse to overrun. Fuzz-tested. |
| `constexpr`-friendly | CRC, packing, parsing and building are usable in constant expressions. |
| Byte-order independent | No type punning or multi-byte `memcpy`; identical wire bytes on big- and little-endian. |

## Requirements

- C++20 (`std::span`, inline variable templates, constexpr algorithms)
- A freestanding-compatible standard library (`<array>`, `<cstdint>`, `<span>`)
- No third-party dependencies. Tests additionally require GoogleTest (fetched automatically) and
  Python 3 to regenerate vectors.

No CPU, vendor, word-size, or byte-order assumptions: the library is plain integer arithmetic over
a caller-supplied byte span. Nothing in it names a vendor SDK, and the bare-metal build is gated on
`CMAKE_SYSTEM_NAME` being `Generic` rather than on any particular MCU family, so STM32, ESP32,
nRF, RP2040 and friends are all just "the target".

Verified toolchains: GCC 13 (x86-64, aarch64, arm-none-eabi), Clang 18 (x86-64).

### Architecture coverage

`tests/embedded/guardrail.cpp` instantiates the whole public surface and `static_assert`s the exact
wire bytes of a known frame. Because the codec is `constexpr`, a target that merely *compiles* that
file has already proven it produces byte-identical framing — which is how big-endian is covered
without needing to run anything there. `tests/embedded/check_portability.sh` compiles it for:

| Target | Why it is in the list |
| --- | --- |
| `thumbv6m-none-eabi` | Cortex-M0+: no FPU, no hardware divide |
| `thumbv7em-none-eabi` | Cortex-M4F, the bridge's own target |
| `riscv32` / `riscv64` | Non-ARM, both word sizes |
| `msp430-unknown-elf` | 16-bit `int` |
| `i686-unknown-elf` | 32-bit x86 |
| `armeb`, `mips`, `powerpc`, `sparc` | **Big-endian** |

```bash
./tests/embedded/check_portability.sh     # 10/10 targets built
```

## Integration

```cmake
add_subdirectory(crsf_cpp)                       # in-tree
target_link_libraries(your_target PRIVATE crsf::crsf)
```

Or standalone via `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(crsf_cpp GIT_REPOSITORY https://github.com/EncinoDev/crsf_cpp.git GIT_TAG v0.1.0)
FetchContent_MakeAvailable(crsf_cpp)
target_link_libraries(your_target PRIVATE crsf::crsf)
```

Or installed once and found as a package:

```bash
cmake -S . -B build -DCRSF_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --install build
```

```cmake
find_package(crsf 0.1 REQUIRED)
target_link_libraries(your_target PRIVATE crsf::crsf)
```

`crsf::crsf` is an `INTERFACE` target carrying include paths and `cxx_std_20` only — nothing is
compiled into a library archive, so cross-compiling needs no per-target artifacts and the installed
package is architecture-independent: one install serves the host build and every cross build.

Single umbrella include:

```cpp
#include "crsf/crsf.hpp"
```

## Quick start

Receiving — feed bytes as they arrive from the UART:

```cpp
crsf::Parser<> parser;
crsf::RcChannels channels{};

void on_uart_byte(std::uint8_t byte) {
  if (parser.feed(byte) != crsf::ParseStatus::kFrameReady) {
    return;
  }
  const auto& frame = parser.frame();
  if (frame.type == static_cast<std::uint8_t>(crsf::FrameType::kRcChannelsPacked)) {
    crsf::decode_rc_channels(frame.payload, channels);   // copy out before the next feed()
  }
}
```

Transmitting — build a frame into a caller-owned buffer:

```cpp
std::array<std::uint8_t, crsf::kMaxFrameSize> buffer{};
const auto channels = crsf::make_centered_channels();
const std::size_t size = crsf::build_rc_channels_frame(crsf::kSyncByte, channels, buffer);
if (size != 0) {
  uart_write(buffer.data(), size);
}
```

---

## API reference

### `crsf/protocol.hpp` — wire constants

| Symbol | Value | Meaning |
| --- | --- | --- |
| `kSyncByte` | `0xC8` | Conventional frame address/sync byte |
| `kMaxFrameSize` | `64` | Largest total frame, all fields included |
| `kMinLengthFieldValue` | `2` | Smallest legal length byte (type + CRC, empty payload) |
| `kMaxLengthFieldValue` | `62` | Largest legal length byte |
| `kMaxPayloadSize` | `60` | Largest payload |
| `kRcChannelCount` | `16` | Channels in an `RC_CHANNELS_PACKED` frame |
| `kRcChannelBits` | `11` | Bits per channel |
| `kRcChannelsPayloadSize` | `22` | `16 x 11 bits`, exactly byte-aligned |
| `kRcChannelMin/Mid/Max` | `172/992/1811` | Raw tick values for the usual 988/1500/2012 us positions |
| `kExtendedFrameTypeMin/Max` | `0x28/0x96` | Types in this range carry routing addresses |
| `kExtendedAddressFieldsSize` | `2` | Destination + origin, ahead of the payload |

```cpp
enum class FrameType : std::uint8_t {
  kLinkStatistics = 0x14,  kRcChannelsPacked = 0x16,
  kDevicePing = 0x28,      kDeviceInfo = 0x29,
  kParameterSettingsEntry = 0x2B, kParameterRead = 0x2C, kParameterWrite = 0x2D,
};

constexpr bool is_extended_frame_type(std::uint8_t type) noexcept;
```

Addresses: `kAddressBroadcast` `0x00`, `kAddressFlightController` `0xC8`, `kAddressReserved2` `0xCA`,
`kAddressRadioTransmitter` `0xEA`, `kAddressCrsfReceiver` `0xEC`, `kAddressCrsfTransmitter` `0xEE`.

**Frame layout.** `<address> <length> <type> <payload...> <crc>`. The length byte counts
*type + payload + CRC* — not itself and not the address. The CRC covers *type + payload* only.

### `crsf/crc8.hpp` — CRC8 (poly 0xD5, "DVB-S2")

```cpp
template <typename Policy>
constexpr std::uint8_t crc8(std::span<const std::uint8_t> data, std::uint8_t init = 0) noexcept;
```

Policies, selected at compile time:

- `Crc8Dvb` — 256-byte table generated at compile time. Faster; costs 256 B of `.rodata`.
- `Crc8DvbBitShift` — bit-by-bit. Smaller; no table.

Both are exhaustively proven equivalent, and validated against the public CRC-8/DVB-S2 catalog
check value (`"123456789"` -> `0xBC`). Use `Crc8BitShiftPolicy<Poly>` / `Crc8LutPolicy<Poly>` for
other polynomials.

### `crsf/parser.hpp` — frame parser

Three entry points do the same job — find a frame boundary, validate length and CRC, hand back a
view. They differ in **who owns the bytes**.

```cpp
// Caller owns the bytes. Stateless free function, nothing copied.
template <typename Crc8Policy = Crc8Dvb>
ParseResult parse(std::span<const std::uint8_t> data) noexcept;

struct ParseResult {
  ParseStatus status;
  std::size_t consumed;   // how far to advance the input
  FrameView   frame;      // valid when status == kFrameReady; points into `data`
};

// Caller owns the bytes, buffer may hold several frames. Returns bytes consumed.
template <typename Crc8Policy = Crc8Dvb, typename OnFrame>
std::size_t scan(std::span<const std::uint8_t> data, OnFrame&& on_frame);

// Same, and reports each frame the checksum rejected.
template <typename Crc8Policy = Crc8Dvb, typename OnFrame, typename OnCrcError>
std::size_t scan(std::span<const std::uint8_t> data, OnFrame&& on_frame, OnCrcError&& on_crc_error);

// Parser owns the bytes. Accumulates across calls.
template <typename Crc8Policy = Crc8Dvb>
class Parser {
 public:
  ParseStatus feed(std::uint8_t byte) noexcept;   // one byte at a time
  const FrameView& frame() const noexcept;        // valid only until the next feed()
  void reset() noexcept;                          // discard partial frame
};
```

#### Which one to call

| | `parse(span)` | `scan(span, fn)` | `Parser::feed(byte)` |
| --- | --- | --- | --- |
| Owns the bytes | Caller | Caller | Parser (64 B internal) |
| Frames per call | One | All that are complete | One, across many calls |
| State / RAM | None — free function | None — free function | 84 B per instance |
| `FrameView` lifetime | As long as you keep the buffer | Inside the callback | Until the next `feed()` |
| Recovery after a bad frame | Slides 1 byte, retries | Slides 1 byte, keeps going | Discards the buffer |
| Natural transport | DMA, or any contiguous block | Socket read, accumulator | Per-byte RX interrupt |

**Use `scan()` when the buffer may hold several frames** — a socket read, or an accumulator you
append to. It loops `parse()`, calls `on_frame` for each valid frame, and returns how many leading
bytes you may erase; a trailing partial frame is left for the next call. A stream of pure garbage
can hold bytes back indefinitely, so cap your own accumulator.

The three-argument overload also calls `on_crc_error` — taking no arguments — once per frame the
checksum rejected. **The rejections are evidence, not noise.** A stream read at the right baud and
polarity resynchronises at frame boundaries and rejects almost nothing; one read at the wrong rate
produces rejections continuously and passes a checksum only by chance. A caller that counts only
the frames that passed cannot tell a weak link from a misread one, which is why an auto-baud sweep
wants both counts. Ignore it — use the two-argument overload — when the link parameters are known
and fixed.

```cpp
std::size_t accepted = 0;
std::size_t rejected = 0;
const std::size_t used = crsf::scan(
    buffer,
    [&](const crsf::FrameView& frame) { ++accepted; handle(frame); },
    [&] { ++rejected; });
```

**Use `parse()` when the bytes are already contiguous in memory** — a DMA buffer, a test vector, a
block read from a file or socket. This is the zero-copy path: `frame.payload` aliases your buffer,
so nothing is copied and no parser object exists.

**Use `feed()` when bytes arrive one at a time** — a per-byte RX interrupt, where a frame spans
many interrupts and no contiguous span exists yet. Looping `feed()` over a buffer you already hold
works, but copies every byte a second time for no benefit.

They are not exclusive: a driver can use `feed()` across a ring-buffer wrap and `parse()` for the
contiguous remainder.

#### Extended-header frames

Frame types `0x28`–`0x96` carry a destination and an origin address ahead of the payload, so a frame
can be routed between two devices on a shared bus. `FrameView::payload` stays whole — routing bytes
included — so callers that predate this see no change; the accessors read them out:

```cpp
bool                         extended()       const noexcept;
std::uint8_t                 ext_destination() const noexcept;
std::uint8_t                 ext_origin()      const noexcept;
std::span<const std::uint8_t> ext_payload()    const noexcept;  // payload minus the two addresses
```

`extended()` is false — and the others return zero / an empty span — when the payload is too short
to hold the fields, so a truncated frame cannot be read past.

#### `consumed`, and why bad frames advance by one

| `status` | `consumed` |
| --- | --- |
| `kFrameReady` | Whole frame — advance past it |
| `kIncomplete` | `0` — keep the bytes, wait for more |
| `kInvalidLength` / `kCrcMismatch` | `1` — slide one byte and retry |

Advancing a single byte after a rejected frame is what lets `parse()` lock onto a frame that starts
part-way through garbage, including garbage whose length byte points into the real frame. `feed()`
cannot do this — it discards its buffer instead — so the two recover differently by design. A
rejected frame costs `feed()` up to one frame; `parse()` recovers at the first valid boundary.

```cpp
std::span<const std::uint8_t> remaining{dma_buffer, bytes_received};
while (!remaining.empty()) {
  const auto result = crsf::parse(remaining);
  if (result.status == crsf::ParseStatus::kIncomplete) break;   // need more bytes
  if (result.status == crsf::ParseStatus::kFrameReady) handle(result.frame);
  remaining = remaining.subspan(result.consumed);
}
```

**Ring buffers.** A span cannot describe a frame that wraps the end of a ring. Use a linear
double-buffer (the usual DMA pattern), or fall back to `feed()` across the wrap.

`ParseStatus`:

| Value | Meaning |
| --- | --- |
| `kIncomplete` | More bytes needed; nothing to do |
| `kFrameReady` | CRC-valid frame available via `frame()` |
| `kInvalidLength` | Length byte outside `[2, 62]`; parser resynchronised |
| `kCrcMismatch` | Frame complete but CRC failed; parser resynchronised |

`FrameView` exposes `address`, `type`, and `payload` (a `std::span` into the parser's buffer).

**Lifetime.** `frame().payload` points into the parser and is invalidated by the next `feed()`.
Copy out anything you need to keep — `decode_rc_channels` does exactly that.

**Address filtering is the caller's job.** The parser accepts any address byte, because valid CRSF
traffic uses several (broadcast `0x00`, flight controller `0xC8`, and others). Inspect
`frame().address` and apply your own policy.

### `crsf/rc_channels.hpp` — channel codec

```cpp
using RcChannels = std::array<std::uint16_t, kRcChannelCount>;

bool decode_rc_channels(std::span<const std::uint8_t> payload, RcChannels& out) noexcept;
bool encode_rc_channels(const RcChannels& channels, std::span<std::uint8_t> out) noexcept;
RcChannels make_centered_channels() noexcept;
```

Both return `false` if the buffer is smaller than `kRcChannelsPayloadSize`. Values wider than
11 bits are masked on encode, matching what a source puts on the wire; clamp beforehand if
out-of-range input should instead be treated as an error.

`make_centered_channels()` is for emulators and test frames. It is **not** a failsafe frame — see
below for why.

### `crsf/failsafe.hpp` — failsafe channel policy

```cpp
enum class FailsafeMode : std::uint8_t { kCenter, kMin, kMax, kHold, kValue };

struct FailsafeChannel {
  FailsafeMode mode = FailsafeMode::kCenter;
  std::uint16_t value = kRcChannelMid;   // read only when mode == kValue
};

using FailsafeConfig = std::array<FailsafeChannel, kRcChannelCount>;

RcChannels make_failsafe_channels(const FailsafeConfig& config,
                                  const RcChannels& last_valid) noexcept;
FailsafeConfig make_aetr_failsafe_config() noexcept;
```

The policy is **per channel**, because "safe" is not one value across all axes. Centring a steering
axis is correct; centring a throttle is half power. A single global rule gets one of the two wrong,
so each channel carries its own mode:

| Mode | Result | Typical use |
| --- | --- | --- |
| `kCenter` | `kRcChannelMid` (992) | Steering, attitude axes |
| `kMin` | `kRcChannelMin` (172) | Throttle; disarm on a switch channel |
| `kMax` | `kRcChannelMax` (1811) | A switch whose safe position is high |
| `kHold` | `last_valid[i]` | Aux switches whose state should survive the dropout |
| `kValue` | `config[i].value` | Anything else — a specific parachute or brake position |

`kHold` is the one to think twice about on a control axis: holding the last commanded value means a
link that dies at full throttle stays at full throttle. It is the right default for aux switches and
the wrong one for anything that moves the machine.

`make_aetr_failsafe_config()` centres everything and drops channel 3 to minimum. That assumes the
**AETR** order ELRS ships by default (1 roll, 2 pitch, 3 throttle, 4 yaw); a transmitter set to TAER
or a custom map puts throttle elsewhere. The library cannot detect the mapping, which is exactly why
the config is data rather than a hard-coded rule — the bridge persists the real one in Flash.

`last_valid` is a required argument rather than an optional one so that a config containing `kHold`
cannot silently fall back to a centre value. Initialise it to a safe frame at boot: before the first
valid frame there is nothing to hold.

### `crsf/link_quality.hpp` — EWMA filter and link quality

```cpp
template <int Shift, typename T = std::uint32_t>
class Ewma {
  void reset(T value) noexcept;
  T    update(T sample) noexcept;
  T    value() const noexcept;
};

template <int Shift = 4>
class LinkQuality {
  void on_frame(bool crc_ok) noexcept;   // a frame arrived
  void on_missed() noexcept;             // an expected frame did not
  void reset(std::uint8_t percent) noexcept;
  std::uint8_t percent() const noexcept;
};
```

An exponentially weighted moving average keeps **one** number instead of a window of samples:

```
value = alpha x sample + (1 - alpha) x value
```

with `alpha = 1 / 2^Shift`. O(1) memory, O(1) time, no buffer — which is why it is the standard
smoothing filter on an MCU. `Shift` is the whole tuning knob:

| `Shift` | alpha | Remembers roughly | At a 150 Hz frame rate |
| --- | --- | --- | --- |
| 2 | 1/4 | 4 samples | ~27 ms |
| 4 (default) | 1/16 | 16 samples | ~107 ms |
| 6 | 1/64 | 64 samples | ~427 ms |

Implemented in integers: the accumulator holds the value scaled by `2^Shift`, so the update is
shifts and adds with no division and no floating point. That matters because `crsf_cpp` builds for
cores without an FPU (Cortex-M0+ is in the portability set), where floats would become soft-float
calls and pull in symbols the bare-metal gate rejects.

**The caller owns the clock.** `LinkQuality` must be told about *missed* frames as well as received
ones — a link that stops cleanly delivers no frames at all, so a filter fed only by arrivals would
report its last value forever. `crsf_cpp` holds no clock by design (same reason `Parser` has no
byte-gap timeout), so the firmware drives `on_missed()` from the timer it already runs for its
source-loss timeout. Quality starts at 0 rather than assuming a healthy link.

### `crsf/frame_builder.hpp` — frame construction

```cpp
template <typename Crc8Policy = Crc8Dvb>
std::size_t build_frame(std::uint8_t address, std::uint8_t type,
                        std::span<const std::uint8_t> payload,
                        std::span<std::uint8_t> out) noexcept;

template <typename Crc8Policy = Crc8Dvb>
std::size_t build_rc_channels_frame(std::uint8_t address, const RcChannels& channels,
                                    std::span<std::uint8_t> out) noexcept;
```

Returns bytes written, or `0` if the payload exceeds `kMaxPayloadSize` or `out` is too small.
Never writes outside `out`.

### `crsf/timing.hpp` — wire timing

```cpp
std::uint32_t serial_time_us(std::size_t bytes, std::uint32_t baud) noexcept;
std::uint32_t idle_gap_timeout_us(std::uint32_t baud) noexcept;
std::uint32_t frame_assembly_timeout_us(std::uint32_t baud) noexcept;
```

All `constexpr`, all assume 10 bits per byte (1 start, 8 data, 1 stop, no parity), all round up.
A zero baud yields zero rather than dividing by zero.

The two timeouts measure different events and differ by about 70x at 420000 baud, so pick by the
event you are timing:

- `idle_gap_timeout_us()` is one character time — 24 us at 420000. This is the line-idle period
  that ends a frame, and what a USART IDLE line interrupt reports at no CPU cost.
- `frame_assembly_timeout_us()` is a maximal frame at that rate plus 15% — 1752 us at 420000. This
  is how long a whole frame may take to arrive before you give up on it.

Both are *values*, not timers: the parser holds no clock, so you run the timer and call
`Parser::reset()`. See [Timeouts](#timeouts-and-resynchronisation).

### `crsf/bit_packing.hpp` — N-bit stream primitives

```cpp
class BitReader {
  explicit BitReader(std::span<const std::uint8_t> data) noexcept;
  std::uint32_t read(int bits) noexcept;   // 1..32; zero-padded past the end
  bool exhausted() const noexcept;
};

class BitWriter {
  explicit BitWriter(std::span<std::uint8_t> out) noexcept;
  bool write(std::uint32_t value, int bits) noexcept;   // false once it cannot fit (sticky)
  std::size_t finish() noexcept;                        // flush partial byte; returns bytes written
  bool overflowed() const noexcept;
};
```

CRSF packs channels as one continuous **LSB-first** bitstream: bit 0 is the least-significant bit
of byte 0, and values straddle byte boundaries. Widths 10–13 are supported, so the same primitives
cover subset-RC frames.

### `crsf/version.hpp` — library version

```cpp
inline constexpr std::uint32_t kVersionMajor;   // 0
inline constexpr std::uint32_t kVersionMinor;   // 1
inline constexpr std::uint32_t kVersionPatch;   // 0
```

Compile-time constants, so a consumer can `static_assert` the version it was written against rather
than discover a missing symbol at link time — which a header-only library would never give it.
They track the git tag and the CMake `project(... VERSION)`; `find_package(crsf 0.1 REQUIRED)`
checks the same number.

---

## Versioning and stability

[Semantic Versioning](https://semver.org). Below 1.0 the minor number carries breaking changes, as
SemVer allows, and every one of them is listed in [`CHANGELOG.md`](CHANGELOG.md).

What is in scope for a compatibility promise: the names, signatures and semantics of everything in
the [API reference](#api-reference) above, and the wire bytes the codec produces. What is not:
anything in `namespace crsf::detail`, the layout and size of `Parser`, the exact footprint and
throughput figures, and the test and benchmark targets.

The wire format itself is not ours to version — it is CRSF, and a change to what the codec puts on
the wire would be a bug fix, not a release.

---

## Concurrency and ISR use

`Parser` is a plain value type with no internal synchronisation and no shared state.

- **Safe:** feeding one parser from one context (e.g. a UART ISR), one parser per port.
- **Not safe:** touching a single parser from an ISR and a task concurrently.

A common pattern is to run `feed()` inside the ISR and, on `kFrameReady`, decode immediately into
an application-owned `RcChannels` (32 bytes) and set a flag. Copying the decoded channels rather
than publishing the `FrameView` avoids handing a task a span that the ISR is about to overwrite.

## Timeouts and resynchronisation

The parser holds no clock, which is what keeps it platform-neutral. Framing recovers on its own:
an invalid length byte is rejected immediately, and a bad frame fails the CRC gate and resets.

If a source can stall mid-frame and later resume, a stale partial frame would merge with the new
one and cost a single frame before recovery. If that matters, run an idle timer in your driver
and call `reset()` when it expires. `crsf/timing.hpp` gives you the thresholds:

```cpp
std::uint32_t serial_time_us(std::size_t bytes, std::uint32_t baud) noexcept;       // rounded up
std::uint32_t idle_gap_timeout_us(std::uint32_t baud) noexcept;        // 1 char; 24 us at 420k
std::uint32_t frame_assembly_timeout_us(std::uint32_t baud) noexcept;  // max frame + 15%; 1752 us
```

Which one depends on the event: resync on line idle uses `idle_gap_timeout_us()`, abandoning a
frame that never finished uses `frame_assembly_timeout_us()`. They differ by 70x — reaching for the
wrong one waits far too long, or not long enough.

All are `constexpr` and assume 10 bits per byte (1 start, 8 data, 1 stop, no parity). A zero baud
yields zero rather than dividing by zero. Taking `baud` as an argument is the point: a threshold
fixed for one rate is far too short at the slow end of an auto-detect sweep.

## Footprint and performance

Flash, Cortex-M4F, `arm-none-eabi-g++ 13.2`, `-Os`, one TU exercising parse + decode + build:

| CRC policy | `.text` | `.rodata` | Total |
| --- | --- | --- | --- |
| `Crc8Dvb` (LUT) | 472 B | 256 B | **728 B** |
| `Crc8DvbBitShift` | 538 B | — | **538 B** |

Throughput, x86-64 release build (indicative; the relative comparison is the point, not the
absolute numbers):

| Operation | `Crc8Dvb` | `Crc8DvbBitShift` |
| --- | --- | --- |
| CRC8 over a 23-byte frame body | 1050–1080 MB/s | 124–135 MB/s |
| Full frame parse + channel decode | 140–160 ns/frame | 265–295 ns/frame |

Ranges are the spread over repeated runs on an unpinned desktop CPU; treat the ratio as the result,
not the absolute figures. The table costs 190 B of flash and buys roughly 8x CRC throughput, which
is about 1.9x on the full parse path. On parts where flash is not scarce, keep the default (`Crc8Dvb`); on very small
parts, pass `Crc8DvbBitShift`.

RAM — all caller-owned, nothing static except the optional CRC table:

| Type | ARM32 | x86-64 |
| --- | --- | --- |
| `Parser` | 84 B | 104 B |
| `RcChannels` | 32 B | 32 B |
| `FrameView` | 12 B | 24 B |

## Testing

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan
```

84 tests covering CRC equivalence and the public catalog value, bit-packing round trips across
widths 10–13, parser framing (length bounds, CRC rejection, recovery, back-to-back frames),
`scan()` over multi-frame and garbage buffers, extended-header routing fields, baud-derived gap
timeouts, failsafe mode resolution, and full build -> parse -> decode cycles.

Test vectors are generated data, not hand-written literals:

```bash
python3 tests/vectors/generate_vectors.py
```

The generator implements CRC and bit packing independently in Python, so the C++ code is checked
against a second implementation rather than against itself.

Fuzzing (Clang):

```bash
cmake -S . -B build/fuzz -DCMAKE_CXX_COMPILER=clang++ -DCRSF_BUILD_FUZZERS=ON
cmake --build build/fuzz --target crsf_cpp_parser_fuzz
./build/fuzz/tests/crsf_cpp_parser_fuzz -runs=1000000
```

The fuzzer drives the parser with arbitrary bytes under ASan/UBSan and asserts that any accepted
frame survives a rebuild and re-parse unchanged, that `scan()` agrees with an equivalent hand-rolled
`parse()` loop, and that neither hands out a view outside the caller's buffer.

Benchmark: `./build/release/tests/crsf_cpp_bench`.

Bare-metal guarantees are checked by building for a freestanding target, which compiles
`tests/embedded/guardrail.cpp` and then runs the symbol gate over the resulting object:

```bash
cmake --preset stm32-debug && cmake --build --preset stm32-debug
```

A build that starts allocating, throwing, or emitting RTTI fails there rather than in review. Any
`Generic` toolchain works; the STM32 preset is simply the one this repo ships.

## Repository layout

| Path | What |
| --- | --- |
| `include/crsf/` | The library. Eleven headers; `crsf.hpp` includes them all |
| `tests/` | GoogleTest suite, generated vectors, benchmark |
| `tests/embedded/` | Freestanding guardrail TU, the `nm` symbol gate, the architecture sweep |
| `tests/fuzz/` | libFuzzer target for the parser |
| `cmake/toolchains/` | `arm-none-eabi` toolchain file for the freestanding presets |

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the build, format and test gates a change has to pass —
they are the same ones CI runs, and `make quality` runs all of them locally.

Security reports: [`SECURITY.md`](SECURITY.md).

## License and provenance

Apache-2.0 — see [`LICENSE`](LICENSE), with copyright and provenance in [`NOTICE`](NOTICE). Every
source file carries an `SPDX-License-Identifier`, because a header-only library is vendored one
header at a time and the file is the unit that travels.

CRSF is a published protocol. This is an independent implementation of it, written from protocol
facts — frame layout, the CRC-8/DVB-S2 polynomial, the LSB-first bit packing, and the length and
checksum validity rules.

**On Betaflight.** Betaflight's CRSF receiver (GPL-3.0) was consulted as a behavioural reference for
those facts. No Betaflight code was copied, adapted, translated, linked or redistributed, and this
library is not a derivative work of it: the parser is a value type with no globals and no clock, the
channel codec is explicit shift/mask arithmetic rather than a packed-bitfield reinterpret, and the
CRC is a compile-time policy pair. Where a source comment names Betaflight, it is citing a protocol
fact or recording where this implementation deliberately differs — `protocol.hpp`'s maximum payload
size is the example, and it differs on purpose. Test vectors are generated by
`tests/vectors/generate_vectors.py` here, not taken from anywhere.

CRSF and Crossfire are trademarks of Team BlackSheep. This project is not affiliated with, endorsed
by, or sponsored by TBS.
