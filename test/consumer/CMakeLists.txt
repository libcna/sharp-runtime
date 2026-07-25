# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

cmake_minimum_required(VERSION 3.20)
project(SharpRuntimeConsumerFixture LANGUAGES CXX C)

if(NOT DEFINED SHARP_RUNTIME_SOURCE_DIR)
    message(FATAL_ERROR "SHARP_RUNTIME_SOURCE_DIR must name the repository root")
endif()
if(NOT DEFINED FIXTURE_COMPONENT)
    message(FATAL_ERROR "FIXTURE_COMPONENT must name the requested component")
endif()
if(NOT DEFINED FIXTURE_SOURCE)
    message(FATAL_ERROR "FIXTURE_SOURCE must name the consumer source file")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SHARP_RUNTIME_COMPONENTS "${FIXTURE_COMPONENT}")
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SHARP_RUNTIME_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

add_subdirectory("${SHARP_RUNTIME_SOURCE_DIR}" sharp-runtime)

if(FIXTURE_COMPILE_ONLY)
    function(collect_public_usage_requirements target)
        get_target_property(aliased_target "${target}" ALIASED_TARGET)
        if(aliased_target)
            set(target "${aliased_target}")
        endif()

        get_property(visited_targets GLOBAL PROPERTY FIXTURE_VISITED_TARGETS)
        if(target IN_LIST visited_targets)
            return()
        endif()
        set_property(GLOBAL APPEND PROPERTY FIXTURE_VISITED_TARGETS "${target}")

        foreach(usage_property IN ITEMS
                INTERFACE_INCLUDE_DIRECTORIES
                INTERFACE_COMPILE_DEFINITIONS
                INTERFACE_COMPILE_FEATURES
                INTERFACE_COMPILE_OPTIONS)
            get_target_property(values "${target}" "${usage_property}")
            if(values)
                set_property(
                    GLOBAL APPEND PROPERTY
                    "FIXTURE_${usage_property}"
                    ${values}
                )
            endif()
        endforeach()

        get_target_property(link_libraries "${target}" INTERFACE_LINK_LIBRARIES)
        foreach(link_library IN LISTS link_libraries)
            # PRIVATE dependencies of static libraries appear as LINK_ONLY
            # generator expressions. Ignoring non-target expressions is what
            # makes this fixture model only the consumer-visible surface.
            if(TARGET "${link_library}")
                collect_public_usage_requirements("${link_library}")
            endif()
        endforeach()
    endfunction()

    collect_public_usage_requirements("SharpRuntime::${FIXTURE_COMPONENT}")

    add_library(
        sharp_runtime_consumer_fixture
        OBJECT
        "${FIXTURE_SOURCE}"
    )
    get_property(
        fixture_include_directories
        GLOBAL PROPERTY FIXTURE_INTERFACE_INCLUDE_DIRECTORIES
    )
    get_property(
        fixture_compile_definitions
        GLOBAL PROPERTY FIXTURE_INTERFACE_COMPILE_DEFINITIONS
    )
    get_property(
        fixture_compile_features
        GLOBAL PROPERTY FIXTURE_INTERFACE_COMPILE_FEATURES
    )
    get_property(
        fixture_compile_options
        GLOBAL PROPERTY FIXTURE_INTERFACE_COMPILE_OPTIONS
    )
    target_include_directories(
        sharp_runtime_consumer_fixture
        PRIVATE
            ${fixture_include_directories}
    )
    target_compile_definitions(
        sharp_runtime_consumer_fixture
        PRIVATE
            ${fixture_compile_definitions}
    )
    target_compile_features(
        sharp_runtime_consumer_fixture
        PRIVATE
            ${fixture_compile_features}
    )
    target_compile_options(
        sharp_runtime_consumer_fixture
        PRIVATE
            ${fixture_compile_options}
    )
else()
    add_executable(sharp_runtime_consumer_fixture "${FIXTURE_SOURCE}")
    target_link_libraries(
        sharp_runtime_consumer_fixture
        PRIVATE
            "SharpRuntime::${FIXTURE_COMPONENT}"
    )
endif()

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
