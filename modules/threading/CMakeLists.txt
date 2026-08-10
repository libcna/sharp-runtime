# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Threading
    TARGET sharp_runtime_threading
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base TimeZone
)
