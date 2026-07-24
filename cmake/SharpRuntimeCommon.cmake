# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

include_guard(GLOBAL)

add_library(sharp_runtime_headers INTERFACE)
add_library(SharpRuntime::Headers ALIAS sharp_runtime_headers)

target_include_directories(sharp_runtime_headers
    INTERFACE
        "$<BUILD_INTERFACE:${SHARP_RUNTIME_ROOT}/include>"
)

target_include_directories(sharp_runtime_headers
    SYSTEM INTERFACE
        "$<BUILD_INTERFACE:${SHARP_RUNTIME_ROOT}/vendor>"
)

target_compile_features(sharp_runtime_headers INTERFACE cxx_std_23)

function(sharp_runtime_apply_build_options target)
    if(MSVC)
        target_compile_options("${target}" PRIVATE /W4 /WX)
    else()
        target_compile_options("${target}" PRIVATE -Wall -Wextra -Werror)

        # Clang does not implement this GCC-only diagnostic and treats the
        # unknown suppression as an error when -Werror is active.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options("${target}" PRIVATE -Wno-format-truncation)
        endif()
    endif()
endfunction()
