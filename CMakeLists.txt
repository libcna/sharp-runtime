cmake_minimum_required(VERSION 3.20)

project(CPP_DOTNET LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "src/*.cpp"
)

add_library(CPP_DOTNET STATIC
        ${SOURCES}
)

target_include_directories(CPP_DOTNET
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(CPP_DOTNET
        PUBLIC
)

target_link_libraries(CPP_DOTNET
        PRIVATE
)

add_subdirectory(vendor/googletest)

enable_testing()

file(GLOB_RECURSE TESTS CONFIGURE_DEPENDS
        "tests/*.cpp"
)

add_executable(Tests
        ${TESTS}
)

target_include_directories(Tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/googletest/googletest/include
)

target_link_libraries(Tests
        PRIVATE
        CPP_DOTNET
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(Tests)
