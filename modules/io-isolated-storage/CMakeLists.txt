# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME IO.IsolatedStorage
    TARGET sharp_runtime_io_isolated_storage
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base IO
    PRIVATE_DEPENDENCIES Storage
)
