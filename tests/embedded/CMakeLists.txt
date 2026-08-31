# crsf_cpp is header-only, so a freestanding build compiles nothing unless something instantiates
# it. This target is that something: the only thing proving the library builds, stays heap-free,
# and keeps its wire format under -fno-exceptions -fno-rtti (ADR 0006).
#
# Deliberately vendor-neutral: it is gated on CMAKE_SYSTEM_NAME being Generic, not on any
# particular MCU family, and carries no CPU, HAL, or linker-script assumptions. It builds for any
# bare-metal target whose toolchain provides freestanding <array>/<span>/<cstdint>.
add_library(crsf_cpp_guardrail STATIC guardrail.cpp)
target_link_libraries(crsf_cpp_guardrail PRIVATE crsf::crsf)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(crsf_cpp_guardrail PRIVATE -Wall -Wextra -Wconversion -Wsign-conversion
                                                      -Werror)
endif()

# The heap/exception/RTTI symbol gate needs a binutils-style nm. Toolchains that ship something
# else still compile the guardrail; they just skip this check rather than failing the build.
if(CMAKE_NM AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_custom_command(
        TARGET crsf_cpp_guardrail
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DOBJECT=$<TARGET_OBJECTS:crsf_cpp_guardrail> -DNM_TOOL=${CMAKE_NM} -P
                ${CMAKE_CURRENT_SOURCE_DIR}/check_symbols.cmake
        COMMAND_EXPAND_LISTS
        VERBATIM
        COMMENT "Checking crsf_cpp for heap/exception/RTTI symbols")
else()
    message(STATUS "crsf_cpp: no usable nm for ${CMAKE_CXX_COMPILER_ID}; skipping symbol guardrail")
endif()
