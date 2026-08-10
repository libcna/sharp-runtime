# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

if(NOT DEFINED SHARP_RUNTIME_FIXTURE_COMPONENT
        OR NOT DEFINED SHARP_RUNTIME_FIXTURE_SOURCE)
    return()
endif()

get_property(
    sharp_runtime_fixture_scheduled
    GLOBAL
    PROPERTY SHARP_RUNTIME_FIXTURE_SCHEDULED
)
if(sharp_runtime_fixture_scheduled)
    return()
endif()
set_property(GLOBAL PROPERTY SHARP_RUNTIME_FIXTURE_SCHEDULED TRUE)

function(_sharp_runtime_add_injected_fixture)
    add_executable(
        sharp_runtime_consumer_fixture
        "${SHARP_RUNTIME_FIXTURE_SOURCE}"
    )
    target_link_libraries(
        sharp_runtime_consumer_fixture
        PRIVATE
            "SharpRuntime::${SHARP_RUNTIME_FIXTURE_COMPONENT}"
    )
    if(NOT MSVC)
        target_compile_options(
            sharp_runtime_consumer_fixture
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Werror
        )
    endif()
endfunction()

cmake_language(
    DEFER
    DIRECTORY "${CMAKE_SOURCE_DIR}"
    CALL _sharp_runtime_add_injected_fixture
)
