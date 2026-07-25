# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME TimeZone
    TARGET sharp_runtime_time_zone
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base
)
