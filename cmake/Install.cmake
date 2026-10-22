# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Preserve the physical include/dependency boundaries of add_subdirectory(). In
# particular, do not merge all installed System/ headers into one include root.
set(_sr_include "${CMAKE_INSTALL_INCLUDEDIR}/sharp-runtime")
set(_sr_config "${CMAKE_INSTALL_LIBDIR}/cmake/SharpRuntime")
set(_sr_targets sharp_runtime_headers)
set_target_properties(sharp_runtime_headers PROPERTIES EXPORT_NAME Headers)
target_include_directories(sharp_runtime_headers SYSTEM INTERFACE
    "$<INSTALL_INTERFACE:${_sr_include}/vendor>")
target_include_directories(sharp_runtime_headers INTERFACE
    "$<INSTALL_INTERFACE:${_sr_include}/generated>")

# The WebAssembly shared-memory ABI must also reach installed consumers; the
# directory-level build flags alone do not form an exported usage requirement.
if(EMSCRIPTEN AND SHARP_RUNTIME_ENABLE_EMSCRIPTEN_THREADS)
    target_compile_options(sharp_runtime_headers INTERFACE "$<INSTALL_INTERFACE:-pthread>")
    target_link_options(sharp_runtime_headers INTERFACE "$<INSTALL_INTERFACE:-pthread>")
endif()

set(SHARP_RUNTIME_PACKAGE_ALIASES "")
set(_sr_installed_include_roots)
sharp_runtime_get_primary_components(_sr_components)
foreach(_sr_component IN LISTS _sr_components)
    if(NOT TARGET "SharpRuntime::${_sr_component}")
        continue()
    endif()
    _sharp_runtime_component_key(_sr_key "${_sr_component}")
    get_property(_sr_target GLOBAL PROPERTY "SHARP_RUNTIME_COMPONENT_${_sr_key}_TARGET")
    get_property(_sr_source_include GLOBAL PROPERTY "SHARP_RUNTIME_COMPONENT_${_sr_key}_INCLUDE_DIRECTORY")
    file(RELATIVE_PATH _sr_relative_include "${SHARP_RUNTIME_ROOT}" "${_sr_source_include}")
    set(_sr_dest "${_sr_include}/${_sr_relative_include}")
    set_target_properties("${_sr_target}" PROPERTIES EXPORT_NAME "${_sr_component}")
    target_include_directories("${_sr_target}" INTERFACE "$<INSTALL_INTERFACE:${_sr_dest}>")
    list(APPEND _sr_targets "${_sr_target}")
    if(NOT _sr_source_include IN_LIST _sr_installed_include_roots)
        install(DIRECTORY "${_sr_source_include}/" DESTINATION "${_sr_dest}"
            COMPONENT SharpRuntime FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.inl")
        list(APPEND _sr_installed_include_roots "${_sr_source_include}")
    endif()
    get_property(_sr_aliases GLOBAL PROPERTY "SHARP_RUNTIME_COMPONENT_${_sr_key}_ALIASES")
    foreach(_sr_alias IN LISTS _sr_aliases)
        string(APPEND SHARP_RUNTIME_PACKAGE_ALIASES
            "if(NOT TARGET SharpRuntime::${_sr_alias})\n"
            "  add_library(SharpRuntime::${_sr_alias} ALIAS SharpRuntime::${_sr_component})\n"
            "endif()\n")
    endforeach()
endforeach()

if(TARGET SharpRuntime::All)
    list(APPEND _sr_targets sharp_runtime_all)
    set_target_properties(sharp_runtime_all PROPERTIES EXPORT_NAME All)
endif()
foreach(_sr_vendor tinyxml2 miniz)
    if(TARGET "sharp_runtime_${_sr_vendor}")
        list(APPEND _sr_targets "sharp_runtime_${_sr_vendor}")
        set_target_properties("sharp_runtime_${_sr_vendor}" PROPERTIES EXPORT_NAME "Vendor.${_sr_vendor}")
    endif()
endforeach()

# Public XML/JSON headers include these exact vendored paths. Keep the vendor
# prefix (especially tinyxml2/) so consumers' unqualified headers do not resolve
# to our namespaced copy. The original licence notices remain in the headers.
foreach(_sr_vendor nlohmann tinyxml2)
    if((_sr_vendor STREQUAL "nlohmann" AND TARGET SharpRuntime::Text.Json)
            OR (_sr_vendor STREQUAL "tinyxml2" AND TARGET SharpRuntime::Xml))
        install(DIRECTORY "${SHARP_RUNTIME_ROOT}/vendor/${_sr_vendor}"
            DESTINATION "${_sr_include}/vendor" COMPONENT SharpRuntime
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
    endif()
endforeach()
# Headers always publishes this directory, including a Core.Base-only install.
install(DIRECTORY DESTINATION "${_sr_include}/vendor" COMPONENT SharpRuntime)
install(FILES "${SHARP_RUNTIME_GENERATED_INCLUDE_DIR}/SharpRuntime/Version.hpp"
    DESTINATION "${_sr_include}/generated/SharpRuntime" COMPONENT SharpRuntime)
install(FILES "${SHARP_RUNTIME_ROOT}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/sharp-runtime" COMPONENT SharpRuntime)
if(TARGET sharp_runtime_miniz)
    # miniz's copyright/permission notices are in its .c files, not its public
    # headers. Ship the original files alongside the binary, without exposing
    # their include directory or compiling another copy in the consumer.
    install(DIRECTORY "${SHARP_RUNTIME_ROOT}/vendor/miniz"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/sharp-runtime/vendor" COMPONENT SharpRuntime
        FILES_MATCHING PATTERN "*.c" PATTERN "*.h")
endif()

set(SHARP_RUNTIME_PACKAGE_DEPENDENCIES "")
if(TARGET SharpRuntime::IO.Compression AND NOT EMSCRIPTEN)
    string(APPEND SHARP_RUNTIME_PACKAGE_DEPENDENCIES "find_dependency(ZLIB)\n")
endif()
if(ANDROID AND TARGET SharpRuntime::Storage AND (TARGET SDL3::SDL3 OR TARGET SDL3::SDL3-static))
    string(APPEND SHARP_RUNTIME_PACKAGE_DEPENDENCIES "find_dependency(SDL3 CONFIG)\n")
endif()

install(TARGETS ${_sr_targets} EXPORT SharpRuntimeTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT SharpRuntime)
install(EXPORT SharpRuntimeTargets NAMESPACE SharpRuntime::
    DESTINATION "${_sr_config}" COMPONENT SharpRuntime)
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/SharpRuntimeConfig.cmake.in"
    "${SHARP_RUNTIME_BINARY_DIR}/SharpRuntimeConfig.cmake"
    INSTALL_DESTINATION "${_sr_config}")
write_basic_package_version_file("${SHARP_RUNTIME_BINARY_DIR}/SharpRuntimeConfigVersion.cmake"
    VERSION "${SHARP_RUNTIME_VERSION}" COMPATIBILITY SameMinorVersion)
install(FILES "${SHARP_RUNTIME_BINARY_DIR}/SharpRuntimeConfig.cmake"
    "${SHARP_RUNTIME_BINARY_DIR}/SharpRuntimeConfigVersion.cmake"
    DESTINATION "${_sr_config}" COMPONENT SharpRuntime)
