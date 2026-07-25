# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Net.Http.Json
    TARGET sharp_runtime_net_http_json
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Net.Http Text.Json Threading.Tasks
    TEST_DEPENDENCIES Net.Sockets
)
