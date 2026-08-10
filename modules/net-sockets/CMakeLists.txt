# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Net.Sockets
    TARGET sharp_runtime_net_sockets
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base IO Net Threading.Tasks
)
