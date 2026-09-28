# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_xml target)
    if(NOT TARGET sharp_runtime_tinyxml2)
        # The vendored copy lives in namespace SharpRuntime::Vendor::tinyxml2, so it links beside a
        # consumer's own tinyxml2 without sharing a symbol (docs/VendoredTinyXml2.md). For the same
        # reason it publishes no include directory: headers reach it as <tinyxml2/tinyxml2.h>
        # through the vendor/ root every component already carries, and exporting vendor/tinyxml2
        # as well let a consumer's own `#include <tinyxml2.h>` resolve to this copy instead of its
        # own, depending on include order. tinyxml2.cpp finds "tinyxml2.h" beside itself.
        add_library(
            sharp_runtime_tinyxml2
            STATIC
            "${SHARP_RUNTIME_ROOT}/vendor/tinyxml2/tinyxml2.cpp"
        )
    endif()
    target_link_libraries("${target}" PUBLIC sharp_runtime_tinyxml2)
endfunction()

sharp_runtime_register_module(
    NAME Xml
    TARGET sharp_runtime_xml
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base Uri
    PRIVATE_DEPENDENCIES Diagnostics TimeZone
    TEST_DEPENDENCIES Xml.Linq TimeZone
    SETUP _sharp_runtime_setup_xml
)

# Xml and XPath have mutual binary dependencies, so both public component
# names intentionally resolve to the same physical archive.
sharp_runtime_register_component(
    NAME Xml.XPath
    ALIAS_OF Xml
)
