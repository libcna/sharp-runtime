# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME Collections.ObjectModel
    TARGET sharp_runtime_collections_object_model
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Collections.Core ComponentModel Core.Base
)
