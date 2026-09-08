# Changelog

Notable changes to this library. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning per [SemVer](https://semver.org), with the pre-1.0 rule stated in the README. Dates are
ISO 8601.

## [Unreleased]

### Added

- `NOTICE`, `CONTRIBUTING.md` and `SECURITY.md`, and an `SPDX-License-Identifier` on every source
  file — a header-only library is vendored one header at a time, so the file is the unit that has
  to carry its terms.
- `crsfConfigVersion.cmake`, so the exported package answers `find_package(crsf 0.1 REQUIRED)`
  instead of ignoring the version request.

### Fixed

- `tests/embedded/check_portability.sh` and `tests/vectors/generate_vectors.py` are executable
  again. Both the README and the CI workflow invoke the sweep as `./tests/embedded/...`, which a
  fresh clone answered with "Permission denied" — the portability gate could not run at all.
- The installed package exported `crsf::crsf_cpp` while every documented integration path names
  `crsf::crsf` — an `ALIAS` is not exported, so only in-tree consumers ever saw the short name.
  `EXPORT_NAME` now makes all three paths link the same target.

### Changed

- README: documented the `scan()` overload that reports checksum rejections, `version.hpp`, the
  install/`find_package` path, and the versioning and stability policy; restated the provenance in
  full.

## [0.1.0] - 2026-09-08

First tagged release: the library extracted from the `crsf-failsafe-bridge` monorepo and made to
stand on its own.

### Added

- **Framing.** `parse()` over caller-owned bytes, `scan()` over a buffer holding several frames,
  and a byte-at-a-time `Parser` for per-byte interrupts. Length bounds and CRC gate acceptance; a
  rejected frame slides one byte so the parser locks onto the next real boundary.
- **CRC-8/DVB-S2** as a compile-time policy pair — a `constexpr` 256-byte table or a bit-by-bit
  loop — proven equivalent over all 256 inputs and checked against the public catalog value.
- **RC channel codec.** `RC_CHANNELS_PACKED` encode and decode over explicit shift/mask arithmetic,
  on `BitReader`/`BitWriter` primitives that also cover widths 10–13 for subset-RC frames.
- **Frame construction.** `build_frame()` and `build_rc_channels_frame()` into a caller-owned span,
  refusing to overrun it.
- **Per-channel failsafe policy** — centre, minimum, maximum, hold, or a fixed value — because a
  centred throttle is half power and one global rule gets some axis wrong.
- **Extended-header addressing** for frame types `0x28`–`0x96`, read through accessors that refuse
  to look past a truncated payload.
- **Link quality** as an integer EWMA, driven by both arrivals and caller-reported misses.
- **Baud-derived timing** as values rather than timers: the idle gap that ends a frame and the
  budget in which a whole frame must arrive, which differ by about 70× and are routinely confused.
- Checksum rejections reported by `scan()`. Their absence is evidence too: a stream read at the
  wrong rate rejects continuously, and a caller that counts only the frames that passed cannot tell
  that from a weak link.

### Guarantees, enforced by CI

- No heap, no exceptions, no RTTI — checked by `nm` over a freestanding build, not by review.
- No global mutable state, no clock dependency, bounded execution, `constexpr`-friendly.
- Byte-order independent, verified by compiling the guardrail TU — whose `static_assert`s pin the
  exact wire bytes — for ten architectures including four big-endian ones and one with 16-bit `int`.
- Memory-safe on malformed input, under ASan/UBSan and libFuzzer.

### Notes

Extracted from the monorepo with its history intact via `git subtree split`. The library was
written to be splittable from the first commit — no HAL, no OS, no sibling includes, its own
`project()` and an exported `crsf::crsf` target — and the extraction needed no source change: the
first standalone clone configured, built, passed all 84 tests and compiled for all ten guardrail
architectures unmodified. What had to be added was the scaffolding the monorepo root used to
supply: the licence, the presets, the formatting configuration, the freestanding toolchain file,
and a CI workflow that runs on a plain runner rather than the bridge's dev container image.

[Unreleased]: https://github.com/EncinoDev/crsf_cpp/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/EncinoDev/crsf_cpp/releases/tag/v0.1.0
