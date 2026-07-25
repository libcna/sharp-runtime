# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Net.Http
    TARGET sharp_runtime_net_http
    TYPE STATIC
    DEPENDENCIES Core IO Net Threading.Tasks
)
