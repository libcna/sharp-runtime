cmake_minimum_required(VERSION 3.20)

project(SHARP_RUNTIME LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SHARP_RUNTIME_BUILD_TESTS "Build sharp-runtime tests" ON)
option(SHARP_RUNTIME_BUILD_BENCHMARKS "Build sharp-runtime standalone micro-benchmarks" OFF)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR
        AND NOT DEFINED SHARP_RUNTIME_COMPONENTS)
    set(
        SHARP_RUNTIME_COMPONENTS
        "All"
        CACHE STRING
        "Semicolon-separated Sharp Runtime components to enable"
    )
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SharpRuntimeModules.cmake")

if(NOT DEFINED SHARP_RUNTIME_COMPONENTS
        OR SHARP_RUNTIME_COMPONENTS STREQUAL "")
    set(sharp_runtime_requested_components All)
else()
    set(sharp_runtime_requested_components ${SHARP_RUNTIME_COMPONENTS})
endif()

sharp_runtime_enable_components(${sharp_runtime_requested_components})

if(SHARP_RUNTIME_BUILD_TESTS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/googletest/CMakeLists.txt")
        message(FATAL_ERROR
            "vendor/googletest is missing.\n"
            "Run: git submodule update --init --recursive")
    endif()
    add_subdirectory(vendor/googletest)

    enable_testing()
    include(GoogleTest)

    set(sharp_runtime_test_components)
    set(sharp_runtime_build_integration_tests FALSE)
    foreach(requested_component IN LISTS sharp_runtime_requested_components)
        if(requested_component STREQUAL "All")
            sharp_runtime_get_primary_components(sharp_runtime_primary_components)
            list(APPEND
                sharp_runtime_test_components
                ${sharp_runtime_primary_components}
            )
            set(sharp_runtime_build_integration_tests TRUE)
        else()
            list(APPEND
                sharp_runtime_test_components
                "${requested_component}"
            )
        endif()
    endforeach()
    list(REMOVE_DUPLICATES sharp_runtime_test_components)

    foreach(test_component IN LISTS sharp_runtime_test_components)
        sharp_runtime_add_component_tests("${test_component}")
    endforeach()

    if(sharp_runtime_build_integration_tests)
        file(GLOB_RECURSE SHARP_RUNTIME_INTEGRATION_TEST_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/*.cpp"
        )
        if(SHARP_RUNTIME_INTEGRATION_TEST_SOURCES)
            add_executable(
                SharpRuntimeIntegrationTests
                ${SHARP_RUNTIME_INTEGRATION_TEST_SOURCES}
            )
            target_link_libraries(
                SharpRuntimeIntegrationTests
                PRIVATE
                    SharpRuntime::All
                    gtest_main
            )
            sharp_runtime_apply_build_options(SharpRuntimeIntegrationTests)
            if(NOT MSVC)
                target_compile_options(
                    SharpRuntimeIntegrationTests
                    PRIVATE
                        -Wno-unused-result
                )
            endif()
            gtest_discover_tests(
                SharpRuntimeIntegrationTests
                TEST_PREFIX "Integration::"
            )
            set_property(
                GLOBAL APPEND PROPERTY
                SHARP_RUNTIME_TEST_TARGETS
                SharpRuntimeIntegrationTests
            )
        endif()
    endif()

    sharp_runtime_get_test_targets(sharp_runtime_test_targets)
    if(sharp_runtime_test_targets)
        add_custom_target(
            SharpRuntimeTests
            DEPENDS ${sharp_runtime_test_targets}
        )
    endif()
endif()

if(SHARP_RUNTIME_BUILD_BENCHMARKS)
    sharp_runtime_enable_component(All)

    add_executable(SharpRuntimeBench
            bench/StringBenchmark.cpp
    )

    target_link_libraries(SharpRuntimeBench PRIVATE SharpRuntime::All)

    if(NOT MSVC)
        target_compile_options(SharpRuntimeBench PRIVATE -Wall -Wextra -Werror -O2)
    else()
        target_compile_options(SharpRuntimeBench PRIVATE /O2)
    endif()
endif()

sharp_runtime_get_enabled_components(sharp_runtime_enabled_components)
list(JOIN sharp_runtime_enabled_components ", " sharp_runtime_enabled_summary)
message(STATUS "Sharp Runtime components: ${sharp_runtime_enabled_summary}")
