# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_core_base target)
    # Preserve the historical archive filename for consumers that inspect
    # build artifacts, while the CMake target name remains unambiguous.
    set_target_properties("${target}" PROPERTIES OUTPUT_NAME sharp_runtime_core)

    # FINAL-STAB-001 i686 dependency boundary: without native __int128 the Decimal
    # implementation is outside the supported surface (its header hard-errors), so the
    # translation unit must leave the build exactly as it left the monolithic source list.
    if(NOT SHARP_RUNTIME_COMPILER_HAS_NATIVE_INT128)
        get_target_property(_core_base_sources "${target}" SOURCES)
        list(FILTER _core_base_sources EXCLUDE REGEX "/System/Decimal\\.cpp$")
        set_target_properties("${target}" PROPERTIES SOURCES "${_core_base_sources}")
    endif()
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
