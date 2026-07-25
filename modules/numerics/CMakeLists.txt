# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Numerics
    TARGET sharp_runtime_numerics
    TYPE STATIC
    DEPENDENCIES Buffers Collections Core
)
