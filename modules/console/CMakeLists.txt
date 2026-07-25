# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Console
    TARGET sharp_runtime_console
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base
)
