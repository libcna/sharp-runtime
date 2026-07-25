# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Net.WebSockets
    TARGET sharp_runtime_net_websockets
    TYPE STATIC
    DEPENDENCIES
        ComponentModel
        Core
        Net
        Net.Sockets
        Threading
        Threading.Tasks
)
