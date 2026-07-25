# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_xml target)
    if(NOT TARGET sharp_runtime_tinyxml2)
        add_library(
            sharp_runtime_tinyxml2
            STATIC
            "${SHARP_RUNTIME_ROOT}/vendor/tinyxml2/tinyxml2.cpp"
        )
        target_include_directories(
            sharp_runtime_tinyxml2
            PUBLIC
                "${SHARP_RUNTIME_ROOT}/vendor/tinyxml2"
        )
    endif()
    target_link_libraries("${target}" PUBLIC sharp_runtime_tinyxml2)
endfunction()

sharp_runtime_register_module(
    NAME Xml
    TARGET sharp_runtime_xml
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base Uri
    PRIVATE_DEPENDENCIES Diagnostics
    TEST_DEPENDENCIES Xml.Linq
    SETUP _sharp_runtime_setup_xml
)

# Xml and XPath have mutual binary dependencies, so both public component
# names intentionally resolve to the same physical archive.
sharp_runtime_register_component(
    NAME Xml.XPath
    ALIAS_OF Xml
)
