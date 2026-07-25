# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Xml.Linq
    TARGET sharp_runtime_xml_linq
    TYPE STATIC
    DEPENDENCIES Core Xml
)
