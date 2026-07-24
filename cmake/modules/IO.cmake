# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_io_compression target)
    find_package(ZLIB REQUIRED)
    target_link_libraries("${target}" PRIVATE ZLIB::ZLIB)
endfunction()

function(_sharp_runtime_setup_io_compression_zip target)
    if(NOT TARGET sharp_runtime_miniz)
        add_library(sharp_runtime_miniz STATIC
            "${SHARP_RUNTIME_ROOT}/vendor/miniz/miniz.c"
            "${SHARP_RUNTIME_ROOT}/vendor/miniz/miniz_tdef.c"
            "${SHARP_RUNTIME_ROOT}/vendor/miniz/miniz_tinfl.c"
            "${SHARP_RUNTIME_ROOT}/vendor/miniz/miniz_zip.c"
        )
        target_include_directories(
            sharp_runtime_miniz
            PUBLIC
                "${SHARP_RUNTIME_ROOT}/vendor/miniz"
        )
    endif()
    target_link_libraries("${target}" PRIVATE sharp_runtime_miniz)
endfunction()

function(_sharp_runtime_setup_storage target)
    if(ANDROID AND TARGET SDL3::SDL3)
        target_link_libraries("${target}" PRIVATE SDL3::SDL3)
    elseif(ANDROID AND TARGET SDL3::SDL3-static)
        target_link_libraries("${target}" PRIVATE SDL3::SDL3-static)
    endif()
endfunction()

sharp_runtime_register_component(
    NAME IO
    TARGET sharp_runtime_io
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_IO_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME IO.Compression
    TARGET sharp_runtime_io_compression
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_IO_COMPRESSION_SOURCES}
    DEPENDENCIES Core IO Buffers
    SETUP _sharp_runtime_setup_io_compression
)

sharp_runtime_register_component(
    NAME IO.Compression.Zip
    TARGET sharp_runtime_io_compression_zip
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_IO_COMPRESSION_ZIP_SOURCES}
    DEPENDENCIES Core IO
    SETUP _sharp_runtime_setup_io_compression_zip
)

sharp_runtime_register_component(
    NAME IO.Hashing
    TARGET sharp_runtime_io_hashing
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_IO_HASHING_SOURCES}
    DEPENDENCIES Core IO
)

sharp_runtime_register_component(
    NAME Storage
    TARGET sharp_runtime_storage
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_STORAGE_SOURCES}
    SETUP _sharp_runtime_setup_storage
)

sharp_runtime_register_component(
    NAME IO.IsolatedStorage
    TARGET sharp_runtime_io_isolated_storage
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_IO_ISOLATED_STORAGE_SOURCES}
    DEPENDENCIES Core IO Storage
)
