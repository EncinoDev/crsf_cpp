# Contributing to crsf_cpp

This is a protocol library that runs inside a failsafe path on a moving machine. The gates below
exist because a defect here is not a crash, it is a vehicle that keeps driving.

## Before you open a pull request

```bash
make quality     # format, build, test, lint — everything CI runs on the host
```

Then the two gates `make quality` does not cover, because they need cross compilers:

```bash
cmake --preset stm32-debug && cmake --build --preset stm32-debug   # guardrail + nm symbol gate
./tests/embedded/check_portability.sh                              # 10 architectures
```

CI runs all of it, plus an ASan/UBSan test run and a 200 000-case fuzz run. Nothing merges red.

## What the gates are checking

| Gate | Command | What a failure means |
| --- | --- | --- |
| Format | `make format-check` | `clang-format` / `cmake-format` drift. `make format` fixes it in place |
| Tests | `ctest --preset debug` | A behavioural change, or a vector that no longer round-trips |
| Sanitizers | `ctest --preset asan` | An out-of-bounds read or undefined behaviour on some input |
| Symbol gate | `cmake --build --preset stm32-debug` | Something on the path now allocates, throws, or emits RTTI |
| Portability | `check_portability.sh` | A word-size, byte-order or FPU assumption crept in |
| Fuzz | `crsf_cpp_parser_fuzz` | Malformed bytes reached memory the caller does not own |

## Rules the code follows

- **C++20, extensions off. No heap, no exceptions, no RTTI, no global mutable state, no clock.**
  These are the library's advertised contract, not preferences — see the guarantee table in the
  README. A change that needs any of them needs a new design instead.
- **Naming:** functions and variables `lower_case`, constants `kName`, members `name_`, types
  `CamelCase`.
- **Comments are scarce and earn their place.** One short line where intent or a non-obvious
  relation is not visible in the code. No banners, no restating the code.
- **Every public entry point must be odr-used by `tests/embedded/guardrail.cpp`**, or the
  freestanding build compiles none of it and the symbol gate has nothing to inspect. This is the
  easiest gate to forget and the one that silently stops protecting you.
- **Test vectors are generated, never hand-written.** `tests/vectors/generate_vectors.py`
  re-derives the CRC and the bit packing in Python, sharing no code with the library, so the two
  implementations check each other. A round trip passes happily on a mistake both halves make.
- **Every new file carries the SPDX header.** Two lines, at the very top:

  ```cpp
  // SPDX-License-Identifier: Apache-2.0
  // Copyright <year> <your name>
  ```

## Provenance — read before adding protocol code

This library is licensed Apache-2.0 and must stay clean of GPL-licensed source. Existing CRSF
implementations — Betaflight's among them — are GPL-3.0.

**You may** consult them, and any published documentation, for behavioural *facts*: frame layout,
polynomials, bit packing, validity rules, timing figures. Facts are not copyrightable.

**You may not** copy, adapt, translate, or transliterate their code, their structure, or their test
vectors into this repository — including via an AI assistant, which is the modern way this happens
by accident. If a contribution reproduces another project's control flow, that is a defect
regardless of how it got there.

When you cite an external implementation in a comment, cite the fact and say what this one does
differently. `include/crsf/protocol.hpp` is the worked example.

## Commits

`type(scope) - short title`, a blank line, then a two- to three-line body saying what changed and
why. Types: `feat, fix, docs, chore, test, refactor`. Scopes: `crsf, infra, ci, docs`.

## Releases

[Semantic Versioning](https://semver.org), with the pre-1.0 rule stated in the README. A release
updates `include/crsf/version.hpp`, `project(... VERSION)` in `CMakeLists.txt`, and the
[`CHANGELOG.md`](CHANGELOG.md) section, then tags `vX.Y.Z`. All three carry the same number.
