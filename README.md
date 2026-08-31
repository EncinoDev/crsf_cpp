# crsf_cpp

Universal, hardware-independent CRSF protocol library. Header-only C++20, no HAL/OS dependencies,
no heap, no exceptions, no RTTI — safe to use on the STM32F411 bridge, the Raspberry Pi master,
and host tests alike.

This directory is self-contained and configures standalone, so it can be extracted into its own
repository later without changes:

```
cmake -S crsf_cpp -B build/crsf_cpp
```

## Consume

```cmake
add_subdirectory(crsf_cpp)          # in-tree
# or FetchContent from the extracted repo, then:
target_link_libraries(your_target PRIVATE crsf::crsf)
```

Scope grows over the following stages: CRC8 (policy-based), zero-copy frame parser, 11-bit channel
reader, frame builder, and an EWMA link-quality filter.
