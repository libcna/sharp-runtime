# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_threading target)
    if(EMSCRIPTEN AND SHARP_RUNTIME_ENABLE_EMSCRIPTEN_THREADS)
        # Emscripten's std::thread implementation is selected at compile time and its shared
        # WebAssembly memory/runtime support is selected again when the final executable links.
        # Keep both requirements on the Threading component so consumers that never use
        # System.Threading remain single-threaded.
        #
        # The options are PUBLIC because this is a static library: the final consumer must see
        # -pthread on its own compile and link commands. A PRIVATE link option would build this
        # archive successfully but leave std::thread construction unsupported at runtime.
        target_compile_options("${target}" PUBLIC -pthread)
        target_link_options("${target}" PUBLIC
            -pthread
            "-sPTHREAD_POOL_SIZE=${SHARP_RUNTIME_EMSCRIPTEN_PTHREAD_POOL_SIZE}")
    endif()
endfunction()

sharp_runtime_register_module(
    NAME Threading
    TARGET sharp_runtime_threading
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base TimeZone
    SETUP _sharp_runtime_setup_threading
)
