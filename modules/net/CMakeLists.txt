# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_net target)
    if(WIN32)
        target_link_libraries("${target}" PRIVATE ws2_32)
    endif()
endfunction()

sharp_runtime_register_module(
    NAME Net
    TARGET sharp_runtime_net
    TYPE STATIC
    PUBLIC_DEPENDENCIES Collections.Core ComponentModel Core.Base Uri
    SETUP _sharp_runtime_setup_net
)
