cmake_minimum_required(VERSION 3.20)

project(SHARP_RUNTIME LANGUAGES CXX)

find_package(ZLIB REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SHARP_RUNTIME_BUILD_TESTS "Build sharp-runtime tests" ON)

file(GLOB_RECURSE SHARP_RUNTIME_SOURCES CONFIGURE_DEPENDS
        "src/*.cpp"
)

# Vendored tinyxml2 (two-file library, MIT licence)
add_library(tinyxml2 STATIC vendor/tinyxml2/tinyxml2.cpp)
target_include_directories(tinyxml2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyxml2)

add_library(SHARP_RUNTIME STATIC
        ${SHARP_RUNTIME_SOURCES}
)

target_include_directories(SHARP_RUNTIME
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

# On Android, StoragePaths.cpp uses SDL_GetPrefPath / SDL_GetAndroidInternalStoragePath
# which require SDL3 headers and library.  SDL3 is provided by the parent project
# (CNA) via cna_configure_vendored_sdl() before this subdirectory is added.
target_link_libraries(SHARP_RUNTIME PRIVATE ZLIB::ZLIB tinyxml2)

if(ANDROID AND TARGET SDL3::SDL3)
    target_link_libraries(SHARP_RUNTIME PRIVATE SDL3::SDL3)
elseif(ANDROID AND TARGET SDL3::SDL3-static)
    target_link_libraries(SHARP_RUNTIME PRIVATE SDL3::SDL3-static)
endif()

if(SHARP_RUNTIME_BUILD_TESTS)
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
            SHARP_RUNTIME
            PRIVATE
            gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(SharpRuntimeTests)
endif()