# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Text.Json
    TARGET sharp_runtime_text_json
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base Text
    PRIVATE_DEPENDENCIES Collections.Core
    TEST_DEPENDENCIES Collections.Core
)
