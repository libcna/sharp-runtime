# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Timers
    TARGET sharp_runtime_timers
    TYPE STATIC
    DEPENDENCIES ComponentModel Core Threading
)
