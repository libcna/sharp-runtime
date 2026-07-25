# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Collections.Blocking
    TARGET sharp_runtime_collections_blocking
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Collections.Core Core.Base Threading
)
