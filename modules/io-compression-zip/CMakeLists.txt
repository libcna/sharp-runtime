# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

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

sharp_runtime_register_module(
    NAME IO.Compression.Zip
    TARGET sharp_runtime_io_compression_zip
    TYPE STATIC
    DEPENDENCIES Core IO
    SETUP _sharp_runtime_setup_io_compression_zip
)
