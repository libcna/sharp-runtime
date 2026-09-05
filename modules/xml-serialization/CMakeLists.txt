# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Xml.Serialization
    TARGET sharp_runtime_xml_serialization
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Collections.Core IO Xml
)
