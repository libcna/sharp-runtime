# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

include_guard(GLOBAL)

set(SHARP_RUNTIME_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
cmake_path(NORMAL_PATH SHARP_RUNTIME_ROOT)

include("${CMAKE_CURRENT_LIST_DIR}/SharpRuntimeCommon.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/SharpRuntimeComponents.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/SharpRuntimeSources.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/modules/Core.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/Text.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/Threading.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/IO.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/Net.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/Security.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/Xml.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/modules/HeaderOnly.cmake")

sharp_runtime_validate_source_partition()
