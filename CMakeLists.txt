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
    # The repository-wide suite exercises every namespace. Component consumers
    # should disable tests to retain a selective dependency closure.
    sharp_runtime_enable_component(All)

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/googletest/CMakeLists.txt")
        message(FATAL_ERROR
            "vendor/googletest is missing.\n"
            "Run: git submodule update --init --recursive")
    endif()
    add_subdirectory(vendor/googletest)

    enable_testing()

    file(GLOB_RECURSE SHARP_RUNTIME_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/*.cpp"
    )

    add_executable(SharpRuntimeTests
            ${SHARP_RUNTIME_TEST_SOURCES}
    )

    target_link_libraries(SharpRuntimeTests
            PUBLIC
            SharpRuntime::All
            PRIVATE
            gtest_main
    )

    if(NOT MSVC)
        target_compile_options(SharpRuntimeTests PRIVATE -Wall -Wextra -Werror -Wno-unused-result)
        # See SHARP_RUNTIME's own identical guard above for why this is GCC-only.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(SharpRuntimeTests PRIVATE -Wno-format-truncation)
        endif()
    endif()

    include(GoogleTest)
    gtest_discover_tests(SharpRuntimeTests)
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
