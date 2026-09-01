# Instantiates the header-only library so a bare-metal build has something to compile and check:
# heap-free, exception-free, wire format intact. Gated on Generic rather than any MCU family, so it
# builds for any target with freestanding <array>/<span>/<cstdint>.
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
