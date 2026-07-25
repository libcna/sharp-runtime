# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME IO
    TARGET sharp_runtime_io
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base Uri
    PRIVATE_DEPENDENCIES TimeZone
    TEST_DEPENDENCIES IO.IsolatedStorage TimeZone
)
