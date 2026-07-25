# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Text.RegularExpressions
    TARGET sharp_runtime_text_regular_expressions
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Core.Base
)
