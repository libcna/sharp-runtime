# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Collections.Core
    TARGET sharp_runtime_collections_core
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Core.Base
    TEST_DEPENDENCIES IO Text
)

# Compatibility umbrella: existing consumers keep the complete collection
# header surface, while internal modules select the narrow physical component
# they actually use.
sharp_runtime_register_component(
    NAME Collections
    TARGET sharp_runtime_collections
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES
        Collections.Core
        Collections.Blocking
        Collections.Async
        Collections.ObjectModel
    INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
