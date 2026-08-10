# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Security.Cryptography
    TARGET sharp_runtime_security_cryptography
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base
)
