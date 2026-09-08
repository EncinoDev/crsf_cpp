#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Bohdan Puhach

# Compiles the guardrail TU for a spread of architectures to prove crsf_cpp carries no CPU, word
# size, or byte order assumptions. The TU's static_asserts pin the exact wire bytes, so a target
# that compiles has also confirmed byte-identical framing -- big-endian targets included.
#
# Needs clang plus any freestanding C++ headers; the arm-none-eabi newlib set is used by default
# because it is the one that exists in this dev container.
set -uo pipefail

CLANG=${CLANG:-clang++}
SRC="$(dirname "$0")/guardrail.cpp"
INCLUDE_ROOT=${INCLUDE_ROOT:-/usr/lib/arm-none-eabi/include}
CXX_HEADERS=${CXX_HEADERS:-$(echo "${INCLUDE_ROOT}"/c++/*)}

TARGETS=(
    thumbv6m-none-eabi     # Cortex-M0+: no FPU, no hardware divide
    thumbv7em-none-eabi    # Cortex-M4F
    riscv32-unknown-elf
    riscv64-unknown-elf
    msp430-unknown-elf     # 16-bit int
    i686-unknown-elf
    armeb-none-eabi        # big endian
    mips-unknown-elf       # big endian
    powerpc-unknown-elf    # big endian
    sparc-unknown-elf      # big endian
)

pass=0
fail=0
for target in "${TARGETS[@]}"; do
    if "${CLANG}" -std=c++20 -Os --target="${target}" -ffreestanding -fno-exceptions -fno-rtti \
        -isystem "${CXX_HEADERS}" -isystem "${CXX_HEADERS}/arm-none-eabi" -isystem "${INCLUDE_ROOT}" \
        -I "$(dirname "$0")/../../include" -c "${SRC}" -o /dev/null 2>/tmp/crsf_portability.log; then
        printf '  ok    %s\n' "${target}"
        pass=$((pass + 1))
    else
        printf '  FAIL  %s\n' "${target}"
        sed 's/^/          /' /tmp/crsf_portability.log | head -5
        fail=$((fail + 1))
    fi
done

printf '\n%d/%d targets built.\n' "${pass}" "$((pass + fail))"
[ "${fail}" -eq 0 ]
