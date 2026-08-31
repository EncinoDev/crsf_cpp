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
| No clock dependency | Parsing is a pure function of the byte sequence (see [Timeouts](#timeouts-and-resynchronisation)). |
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
./crsf_cpp/tests/embedded/check_portability.sh     # 10/10 targets built
```

## Integration

```cmake
add_subdirectory(crsf_cpp)                       # in-tree
target_link_libraries(your_target PRIVATE crsf::crsf)
```

Or standalone via `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(crsf_cpp GIT_REPOSITORY <url> GIT_TAG <tag>)
FetchContent_MakeAvailable(crsf_cpp)
target_link_libraries(your_target PRIVATE crsf::crsf)
```

`crsf::crsf` is an `INTERFACE` target carrying include paths and `cxx_std_20` only — nothing is
compiled into a library archive, so cross-compiling needs no per-target artifacts.

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

`enum class FrameType : std::uint8_t { kRcChannelsPacked = 0x16 };`

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

```cpp
template <typename Crc8Policy = Crc8Dvb>
class Parser {
 public:
  ParseStatus feed(std::uint8_t byte) noexcept;   // one byte at a time
  const FrameView& frame() const noexcept;        // valid only until the next feed()
  void reset() noexcept;                          // discard partial frame
};
```

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
one and cost a single frame before recovery. If that matters, run a byte-gap timer in your driver
and call `reset()` when it expires. At 420 kbaud a gap longer than one frame time (~1.5 ms for a
maximal frame) is a reasonable threshold.

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

The suite covers CRC equivalence and the public catalog value, bit-packing round trips across
widths 10–13, parser framing (length bounds, CRC rejection, recovery, back-to-back frames), and
full build -> parse -> decode cycles.

Test vectors are generated data, not hand-written literals:

```bash
python3 crsf_cpp/tests/vectors/generate_vectors.py
```

The generator implements CRC and bit packing independently in Python, so the C++ code is checked
against a second implementation rather than against itself.

Fuzzing (Clang):

```bash
cmake -S . -B build/fuzz -DCMAKE_CXX_COMPILER=clang++ -DCRSF_BUILD_FUZZERS=ON
cmake --build build/fuzz --target crsf_cpp_parser_fuzz
./build/fuzz/crsf_cpp/tests/crsf_cpp_parser_fuzz -runs=1000000
```

The fuzzer drives the parser with arbitrary bytes under ASan/UBSan and asserts that any accepted
frame survives a rebuild and re-parse unchanged.

Benchmark: `./build/release/crsf_cpp/tests/crsf_cpp_bench`.

Bare-metal guarantees are checked by building for a freestanding target, which compiles
`tests/embedded/guardrail.cpp` and then runs the symbol gate over the resulting object:

```bash
cmake --preset stm32-debug && cmake --build --preset stm32-debug
```

A build that starts allocating, throwing, or emitting RTTI fails there rather than in review. Any
`Generic` toolchain works; the STM32 preset is simply the one this repo ships.

## License and provenance

Apache-2.0.

CRSF is a published protocol; this implementation was written from protocol facts (frame layout,
CRC polynomial, bit packing). It contains no third-party protocol source, and its test vectors are
generated by the script in this repository rather than taken from another project.
