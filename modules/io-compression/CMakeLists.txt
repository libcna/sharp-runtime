# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_io_compression target)
    find_package(ZLIB REQUIRED)
    target_link_libraries("${target}" PRIVATE ZLIB::ZLIB)
endfunction()

sharp_runtime_register_module(
    NAME IO.Compression
    TARGET sharp_runtime_io_compression
    TYPE STATIC
    DEPENDENCIES Buffers Core IO
    SETUP _sharp_runtime_setup_io_compression
)
