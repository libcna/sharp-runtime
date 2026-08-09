cmake_minimum_required(VERSION 3.20)

project(SHARP_RUNTIME LANGUAGES CXX C)

find_package(ZLIB REQUIRED)

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

file(GLOB_RECURSE SHARP_RUNTIME_SOURCES CONFIGURE_DEPENDS
        "src/*.cpp"
)

if(NOT SHARP_RUNTIME_COMPILER_HAS_NATIVE_INT128)
    list(REMOVE_ITEM SHARP_RUNTIME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/System/Decimal.cpp")
endif()

# Vendored tinyxml2 (two-file library, MIT licence)
add_library(tinyxml2 STATIC vendor/tinyxml2/tinyxml2.cpp)
target_include_directories(tinyxml2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyxml2)

# Vendored miniz (multi-file library, MIT/public domain)
add_library(miniz STATIC
    vendor/miniz/miniz.c
    vendor/miniz/miniz_tdef.c
    vendor/miniz/miniz_tinfl.c
    vendor/miniz/miniz_zip.c
)
target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniz)

add_library(SHARP_RUNTIME STATIC
        ${SHARP_RUNTIME_SOURCES}
)

target_include_directories(SHARP_RUNTIME PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(SHARP_RUNTIME SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
target_compile_definitions(SHARP_RUNTIME PUBLIC
        SHARP_RUNTIME_HAS_NATIVE_INT128=${SHARP_RUNTIME_NATIVE_INT128_VALUE})

# On Android, StoragePaths.cpp uses SDL_GetPrefPath / SDL_GetAndroidInternalStoragePath
# which require SDL3 headers and library.  SDL3 is provided by the parent project
# (CNA) via cna_configure_vendored_sdl() before this subdirectory is added.
target_link_libraries(SHARP_RUNTIME PRIVATE ZLIB::ZLIB tinyxml2 miniz)

if(MSVC)
    target_compile_options(SHARP_RUNTIME PRIVATE /W4 /WX)
else()
    target_compile_options(SHARP_RUNTIME PRIVATE -Wall -Wextra -Werror)
    # -Wformat-truncation is a GCC-only diagnostic -- Clang (incl. Apple Clang) never implemented
    # it, so -Wno-format-truncation is an unrecognized flag there, which -Werror then turns into a
    # hard build error (-Werror,-Wunknown-warning-option) rather than a harmless no-op.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(SHARP_RUNTIME PRIVATE -Wno-format-truncation)
    endif()
endif()

if(WIN32)
    target_link_libraries(SHARP_RUNTIME PRIVATE ws2_32)
endif()

if(ANDROID AND TARGET SDL3::SDL3)
    target_link_libraries(SHARP_RUNTIME PRIVATE SDL3::SDL3)
elseif(ANDROID AND TARGET SDL3::SDL3-static)
    target_link_libraries(SHARP_RUNTIME PRIVATE SDL3::SDL3-static)
endif()

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

    file(GLOB_RECURSE SHARP_RUNTIME_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/*.cpp"
    )
    list(REMOVE_ITEM SHARP_RUNTIME_TEST_SOURCES ${SHARP_RUNTIME_I686_REGRESSION_SOURCE})

    add_executable(SharpRuntimeTests
            ${SHARP_RUNTIME_TEST_SOURCES}
    )

    target_link_libraries(SharpRuntimeTests
            PUBLIC
            SHARP_RUNTIME
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
    add_executable(SharpRuntimeBench
            bench/StringBenchmark.cpp
    )

    target_link_libraries(SharpRuntimeBench PRIVATE SHARP_RUNTIME)

    if(NOT MSVC)
        target_compile_options(SharpRuntimeBench PRIVATE -Wall -Wextra -Werror -O2)
    else()
        target_compile_options(SharpRuntimeBench PRIVATE /O2)
    endif()
endif()
