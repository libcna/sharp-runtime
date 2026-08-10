# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Threading.Tasks
    TARGET sharp_runtime_threading_tasks
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base Threading
)
