cmake_minimum_required(VERSION 3.20)

project(CPP_DOTNET LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPP_DOTNET_BUILD_TESTS "Build cpp-dotnet tests" ON)

file(GLOB_RECURSE CPP_DOTNET_SOURCES CONFIGURE_DEPENDS
        "src/*.cpp"
)

add_library(CPP_DOTNET STATIC
        ${CPP_DOTNET_SOURCES}
)

target_include_directories(CPP_DOTNET
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CPP_DOTNET_BUILD_TESTS)
    add_subdirectory(vendor/googletest)

    enable_testing()

    file(GLOB_RECURSE CPP_DOTNET_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/*.cpp"
    )

    add_executable(CppDotNetTests
            ${CPP_DOTNET_TEST_SOURCES}
    )

    target_link_libraries(CppDotNetTests
            PUBLIC
            CPP_DOTNET
            PRIVATE
            gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(CppDotNetTests)
endif()