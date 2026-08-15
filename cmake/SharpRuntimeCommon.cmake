# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

include_guard(GLOBAL)

add_library(sharp_runtime_headers INTERFACE)
add_library(SharpRuntime::Headers ALIAS sharp_runtime_headers)

target_include_directories(sharp_runtime_headers
    SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${SHARP_RUNTIME_ROOT}/vendor>"
)

target_compile_features(sharp_runtime_headers INTERFACE cxx_std_23)

function(sharp_runtime_apply_build_options target)
    if(MSVC)
        # /utf-8 states what every source file in this repository already is. Without it MSVC
        # decodes sources in the host's ANSI code page, so a multi-byte UTF-8 sequence inside a
        # char32_t or char16_t literal arrives as several characters -- IdnMapping's U+3002,
        # U+FF0E and U+FF61 label separators fail with C2015 for exactly that reason -- and every
        # other literal is decoded against a code page that varies by machine. GCC and Clang
        # already treat both the source and execution charset as UTF-8, so this is what makes the
        # three compilers agree rather than a Windows-specific concession.
        target_compile_options("${target}" PRIVATE /W4 /WX /utf-8)

        # <windows.h> defines function-like min and max macros unless NOMINMAX is set, and
        # <winsock2.h> pulls it in. Every std::min, std::max and numeric_limits<T>::max() in a
        # translation unit that reaches a Windows header is then mangled into a macro invocation
        # -- Socket.cpp and TcpClient.cpp fail with C2589 and C4003 for that reason alone. Set
        # once for the whole library rather than defined ahead of each of the eleven Windows
        # include blocks, where a single wrong ordering would bring the macros back.
        target_compile_definitions("${target}" PRIVATE NOMINMAX)
    else()
        target_compile_options("${target}" PRIVATE -Wall -Wextra -Werror)

        # Clang does not implement this GCC-only diagnostic and treats the
        # unknown suppression as an error when -Werror is active.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options("${target}" PRIVATE -Wno-format-truncation)
        endif()
    endif()
endfunction()
