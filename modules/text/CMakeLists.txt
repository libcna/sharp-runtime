# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Text
    TARGET sharp_runtime_text
    TYPE STATIC
    PUBLIC_DEPENDENCIES Buffers Core.Base
)
