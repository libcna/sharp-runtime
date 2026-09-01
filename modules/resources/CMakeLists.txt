# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Resources
    TARGET sharp_runtime_resources
    TYPE STATIC
    PUBLIC_DEPENDENCIES Globalization
    PRIVATE_DEPENDENCIES Core.Base
)
