# Asserts ADR 0006's guarantees on the compiled bare-metal object: no heap, no exception
# machinery, no RTTI. Run as a POST_BUILD step; needs OBJECT and NM_TOOL on the command line.
if(NOT EXISTS "${OBJECT}")
    message(FATAL_ERROR "guardrail object not found: ${OBJECT}")
endif()

execute_process(
    COMMAND "${NM_TOOL}" --demangle "${OBJECT}"
    OUTPUT_VARIABLE _symbols
    ERROR_VARIABLE _nm_error
    RESULT_VARIABLE _nm_result)
if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR "nm failed on ${OBJECT}: ${_nm_error}")
endif()

# memset/memcpy are excluded deliberately: the compiler emits them for zero-initialisation of the
# library's own plain-old-data members, which is not dynamic allocation.
set(_forbidden
    "malloc" "calloc" "realloc" "[^_a-zA-Z]free"
    "operator new" "operator delete"
    "__cxa_throw" "__cxa_allocate_exception" "__cxa_begin_catch" "_Unwind_"
    "typeinfo for" "typeinfo name for")

set(_violations "")
foreach(_pattern IN LISTS _forbidden)
    if(_symbols MATCHES "${_pattern}")
        string(APPEND _violations "  - matched forbidden symbol pattern: ${_pattern}\n")
    endif()
endforeach()

if(NOT _violations STREQUAL "")
    message(FATAL_ERROR "crsf_cpp embedded guardrail failed on ${OBJECT}:\n${_violations}\nSymbols:\n${_symbols}")
endif()

message(STATUS "crsf_cpp guardrail: no heap/exception/RTTI symbols in ${OBJECT}")
