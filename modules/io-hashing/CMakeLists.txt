# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME IO.Hashing
    TARGET sharp_runtime_io_hashing
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base IO
)
