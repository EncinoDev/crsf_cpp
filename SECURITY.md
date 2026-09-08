# Security policy

## Scope

`crsf_cpp` parses bytes that arrive from a radio link. Everything it decodes is attacker-influenced
in the sense that matters: a receiver in a noisy RF environment produces arbitrary byte sequences,
and the library must survive all of them without reading or writing outside the caller's buffers.

In scope:

- Any input that makes the parser read or write outside the span it was given.
- Any input that makes an accepted frame's `FrameView` point outside the caller's buffer.
- Any input that causes unbounded work, or work not linear in the frame length.
- Any path that allocates, throws, or requires RTTI, since the symbol gate is a safety guarantee
  callers rely on.

Out of scope, because the library does not claim them: authenticity and confidentiality. CRSF is an
unauthenticated, unencrypted protocol — a CRC-8 detects corruption, not forgery. Anything that can
put bytes on the wire can command the receiver, and that is a property of the protocol, not of this
implementation. Systems that need to resist an active attacker need a trust boundary above this
library.

## Reporting

Report privately through GitHub's **Report a vulnerability** button on the repository's Security
tab, or by opening a draft advisory. Please do not open a public issue for a memory-safety defect
until a fix is available.

Include the byte sequence that triggers it. The parser is deterministic and holds no clock, so a
sequence of bytes is a complete reproduction — a `crsf_cpp_parser_fuzz` corpus file is ideal.

## What happens next

A confirmed memory-safety defect is treated as a release blocker, gets a regression test built from
the reproducing bytes, and is credited in `CHANGELOG.md` unless you ask otherwise.

## Testing done here

Every change runs an ASan/UBSan test pass and a libFuzzer run over the parser that asserts no
accepted frame ever aliases memory outside the input span. See
[`CONTRIBUTING.md`](CONTRIBUTING.md).
