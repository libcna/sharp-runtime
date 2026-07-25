# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Net.NetworkInformation
    TARGET sharp_runtime_net_network_information
    TYPE STATIC
    DEPENDENCIES ComponentModel Core Net Threading.Tasks
)
