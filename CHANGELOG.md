# Changelog

Notable changes to this library. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
dates in ISO 8601.

## [Unreleased]

### Added

- extracted from the `crsf-failsafe-bridge` monorepo with its history intact, via
  `git subtree split`. The library was written to be splittable from the first commit — no HAL, no
  OS, no sibling includes, its own `project()` and exported `crsf::crsf` target — and the extraction
  needed no source change: the first standalone clone configured, built, passed all 84 tests and
  compiled for all ten guardrail architectures unmodified.
- repository scaffolding the monorepo used to supply: license, presets, formatting configuration,
  the freestanding toolchain file, and a CI workflow that runs on a plain runner rather than the
  bridge's dev container image.
