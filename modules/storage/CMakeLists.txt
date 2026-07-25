# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_storage target)
    if(ANDROID AND TARGET SDL3::SDL3)
        target_link_libraries("${target}" PRIVATE SDL3::SDL3)
    elseif(ANDROID AND TARGET SDL3::SDL3-static)
        target_link_libraries("${target}" PRIVATE SDL3::SDL3-static)
    endif()
endfunction()

sharp_runtime_register_module(
    NAME Storage
    TARGET sharp_runtime_storage
    TYPE STATIC
    SETUP _sharp_runtime_setup_storage
)
