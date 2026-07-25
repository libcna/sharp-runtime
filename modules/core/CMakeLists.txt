# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_core_base target)
    # Preserve the historical archive filename for consumers that inspect
    # build artifacts, while the CMake target name remains unambiguous.
    set_target_properties("${target}" PROPERTIES OUTPUT_NAME sharp_runtime_core)
endfunction()

sharp_runtime_register_module(
    NAME Core.Base
    TARGET sharp_runtime_core_base
    TYPE STATIC
    SETUP _sharp_runtime_setup_core_base
    TEST_DEPENDENCIES
        Collections.Core
        Globalization
        IO
        Runtime
        Text
        Threading
        Threading.Tasks
        TimeZone
        Uri
)

# Compatibility umbrella. Internal modules depend on Core.Base and explicitly
# opt into optional clusters; existing consumers of SharpRuntime::Core keep the
# complete pre-split header surface.
sharp_runtime_register_component(
    NAME Core
    TARGET sharp_runtime_core
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Core.Base Console Uri TimeZone
    INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
