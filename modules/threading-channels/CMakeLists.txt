# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Threading.Channels
    TARGET sharp_runtime_threading_channels
    TYPE INTERFACE
    DEPENDENCIES Core Threading.Tasks
)
