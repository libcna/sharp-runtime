# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_component(
    NAME Text
    TARGET sharp_runtime_text
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_TEXT_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Text.Json
    TARGET sharp_runtime_text_json
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_TEXT_JSON_SOURCES}
    DEPENDENCIES Core Text Collections
)
