# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_io_compression target)
    if(EMSCRIPTEN)
        # Emscripten ships zlib as a PORT, not as a system library, so find_package(ZLIB) fails
        # with "missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR" until someone runs `embuilder build zlib`
        # and passes both paths by hand. Requesting the port here means every consumer no longer
        # has to know that.
        #
        # The link option is PUBLIC on purpose: this is a STATIC library, so the port has to reach
        # the FINAL executable's link line as well. PRIVATE would configure and build cleanly and
        # then fail whoever links the result, which is the worse failure of the two.
        target_compile_options("${target}" PUBLIC "-sUSE_ZLIB=1")
        target_link_options("${target}" PUBLIC "-sUSE_ZLIB=1")
    else()
        find_package(ZLIB REQUIRED)
        target_link_libraries("${target}" PRIVATE ZLIB::ZLIB)
    endif()
endfunction()

sharp_runtime_register_module(
    NAME IO.Compression
    TARGET sharp_runtime_io_compression
    TYPE STATIC
    PUBLIC_DEPENDENCIES Buffers Core.Base IO
    SETUP _sharp_runtime_setup_io_compression
)
