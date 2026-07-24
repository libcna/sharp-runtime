# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_component(
    NAME Core
    TARGET sharp_runtime_core
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_CORE_SOURCES}
)

sharp_runtime_register_component(
    NAME Diagnostics
    TARGET sharp_runtime_diagnostics
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_DIAGNOSTICS_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Globalization
    TARGET sharp_runtime_globalization
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_GLOBALIZATION_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Numerics
    TARGET sharp_runtime_numerics
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NUMERICS_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Runtime
    TARGET sharp_runtime_runtime
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_RUNTIME_SOURCES}
    DEPENDENCIES Core
)
