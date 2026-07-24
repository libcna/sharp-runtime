# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_component(
    NAME Buffers
    TARGET sharp_runtime_buffers
    TYPE INTERFACE
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Collections
    TARGET sharp_runtime_collections
    TYPE INTERFACE
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME ComponentModel
    TARGET sharp_runtime_component_model
    TYPE INTERFACE
    DEPENDENCIES Core Collections
)

sharp_runtime_register_component(
    NAME Security
    TARGET sharp_runtime_security
    TYPE INTERFACE
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Text.RegularExpressions
    TARGET sharp_runtime_text_regular_expressions
    TYPE INTERFACE
    DEPENDENCIES Core Collections Text
)

sharp_runtime_register_component(
    NAME Threading.Channels
    TARGET sharp_runtime_threading_channels
    TYPE INTERFACE
    DEPENDENCIES Core Threading Threading.Tasks
)

sharp_runtime_register_component(
    NAME Net.Security
    TARGET sharp_runtime_net_security
    TYPE INTERFACE
    DEPENDENCIES Core IO Net Threading.Tasks
)

sharp_runtime_register_component(
    NAME Net.Http.Json
    TARGET sharp_runtime_net_http_json
    TYPE INTERFACE
    DEPENDENCIES Core Net.Http Text.Json Threading.Tasks
)
