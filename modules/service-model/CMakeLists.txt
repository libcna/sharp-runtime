# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

sharp_runtime_register_module(
    NAME ServiceModel
    TARGET sharp_runtime_service_model
    TYPE STATIC
    PUBLIC_DEPENDENCIES Core.Base Uri Xml
    PRIVATE_DEPENDENCIES Net.Http
    TEST_DEPENDENCIES Net.Http Net.Sockets
)
