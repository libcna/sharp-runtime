cmake_minimum_required(VERSION 3.20)

project(SHARP_RUNTIME VERSION 0.1.0 LANGUAGES CXX C)

# --- Release identity (docs/releasing.md) --------------------------------------------------
# project(VERSION) accepts numeric components only, so the Semantic Versioning pre-release
# identifier lives beside it and is joined back on here. Deliberately a normal variable and not
# a cache entry: the version identifies the release, it is not a per-build knob, and a cached
# copy would keep an already-configured build directory reporting the previous release after a
# bump -- exactly the kind of stale claim a release must not make. SHARP_RUNTIME_VERSION_STRING
# is the single spelling the rest of the build reads. Do not re-derive it elsewhere.
set(SHARP_RUNTIME_VERSION_PRERELEASE "alpha.1")
if(SHARP_RUNTIME_VERSION_PRERELEASE)
    set(SHARP_RUNTIME_VERSION_STRING "${PROJECT_VERSION}-${SHARP_RUNTIME_VERSION_PRERELEASE}")
else()
    set(SHARP_RUNTIME_VERSION_STRING "${PROJECT_VERSION}")
endif()
message(STATUS "Sharp Runtime: version ${SHARP_RUNTIME_VERSION_STRING}")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.cmake")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SHARP_RUNTIME_BUILD_TESTS "Build sharp-runtime tests" ON)
option(SHARP_RUNTIME_BUILD_BENCHMARKS "Build sharp-runtime standalone micro-benchmarks" OFF)
option(SHARP_RUNTIME_BUILD_I686_REGRESSION "Build the i686 MinGW compile/link boundary regression" OFF)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles([=[
int main()
{
    __int128 signedValue = 1;
    unsigned __int128 unsignedValue = 1;
    static_assert(sizeof(signedValue) == 16);
    static_assert(sizeof(unsignedValue) == 16);
    return 0;
}
]=] SHARP_RUNTIME_COMPILER_HAS_NATIVE_INT128)

if(SHARP_RUNTIME_COMPILER_HAS_NATIVE_INT128)
    set(SHARP_RUNTIME_NATIVE_INT128_VALUE 1)
else()
    set(SHARP_RUNTIME_NATIVE_INT128_VALUE 0)
endif()

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

# FINAL-STAB-001: publish the native-__int128 capability to every component and consumer.
# The monolithic root carried this PUBLIC on the SHARP_RUNTIME archive; the modular carrier
# is the shared headers surface every component and consumer links transitively.
target_compile_definitions(sharp_runtime_headers INTERFACE
        SHARP_RUNTIME_HAS_NATIVE_INT128=${SHARP_RUNTIME_NATIVE_INT128_VALUE})

# The generated release identity (SharpRuntime/Version.hpp, cmake/Version.cmake) rides the same
# carrier for the same reason: it is a fact about the build that every component and consumer
# may ask about, and this interface target is the one surface all of them link transitively. A
# consumer writes #include "SharpRuntime/Version.hpp" without knowing which include root
# resolves it.
target_include_directories(sharp_runtime_headers INTERFACE
        "$<BUILD_INTERFACE:${SHARP_RUNTIME_GENERATED_INCLUDE_DIR}>")

if(NOT DEFINED SHARP_RUNTIME_COMPONENTS
        OR SHARP_RUNTIME_COMPONENTS STREQUAL "")
    set(sharp_runtime_requested_components All)
else()
    set(sharp_runtime_requested_components ${SHARP_RUNTIME_COMPONENTS})
endif()

sharp_runtime_enable_components(${sharp_runtime_requested_components})

set(SHARP_RUNTIME_I686_REGRESSION_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Platform/SharpRuntimeI686CompileBoundary.cpp")

if(SHARP_RUNTIME_BUILD_I686_REGRESSION)
    if(NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "SHARP_RUNTIME_BUILD_I686_REGRESSION requires a 32-bit MinGW toolchain.")
    endif()
    if(SHARP_RUNTIME_BUILD_TESTS)
        message(FATAL_ERROR
            "SHARP_RUNTIME_BUILD_I686_REGRESSION requires SHARP_RUNTIME_BUILD_TESTS=OFF; "
            "the native-128 test suites are outside the supported i686 boundary.")
    endif()
    if(SHARP_RUNTIME_COMPILER_HAS_NATIVE_INT128)
        message(FATAL_ERROR
            "The i686 regression must exercise a compiler without native __int128 support.")
    endif()

    add_executable(SharpRuntimeI686CompileBoundary
            ${SHARP_RUNTIME_I686_REGRESSION_SOURCE})
    target_link_libraries(SharpRuntimeI686CompileBoundary PRIVATE SHARP_RUNTIME)
    target_compile_options(SharpRuntimeI686CompileBoundary PRIVATE -Wall -Wextra -Werror)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(SharpRuntimeI686CompileBoundary PRIVATE -Wno-format-truncation)
    endif()
endif()

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
