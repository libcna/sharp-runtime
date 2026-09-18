# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

# Compiled since the TypeConverter / TypeDescriptor / PropertyDescriptor subsystem arrived:
# the type-descriptor registry and the converter bodies live in src/. Globalization is a public
# dependency because every TypeConverter signature names CultureInfo; Collections.Core because
# CreateInstance takes a Hashtable; Uri because UriTypeConverter, a System-namespace type that
# .NET ships beside ComponentModel, is owned here so that Uri itself stays a leaf.
sharp_runtime_register_module(
    NAME ComponentModel
    TARGET sharp_runtime_component_model
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base Collections.Core Globalization Uri
)
