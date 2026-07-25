# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

include_guard(GLOBAL)

include(CMakeParseArguments)

function(_sharp_runtime_component_key output component)
    string(MAKE_C_IDENTIFIER "${component}" component_key)
    set("${output}" "${component_key}" PARENT_SCOPE)
endfunction()

function(sharp_runtime_register_component)
    set(options)
    set(one_value_args NAME TARGET TYPE SETUP ALIAS_OF INCLUDE_DIRECTORY)
    set(multi_value_args SOURCES DEPENDENCIES)
    cmake_parse_arguments(
        SHARP_COMPONENT
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    if(NOT SHARP_COMPONENT_NAME)
        message(FATAL_ERROR "A Sharp Runtime component must have a NAME")
    endif()

    _sharp_runtime_component_key(component_key "${SHARP_COMPONENT_NAME}")
    get_property(
        already_registered
        GLOBAL
        PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_REGISTERED"
    )
    if(already_registered)
        message(FATAL_ERROR
            "Sharp Runtime component '${SHARP_COMPONENT_NAME}' is registered twice")
    endif()

    if(SHARP_COMPONENT_ALIAS_OF)
        if(SHARP_COMPONENT_TARGET OR SHARP_COMPONENT_TYPE OR SHARP_COMPONENT_SOURCES
                OR SHARP_COMPONENT_SETUP OR SHARP_COMPONENT_INCLUDE_DIRECTORY)
            message(FATAL_ERROR
                "Alias component '${SHARP_COMPONENT_NAME}' may only specify ALIAS_OF")
        endif()
        set(component_type "ALIAS")
        set(component_target "")
        set(component_dependencies "${SHARP_COMPONENT_ALIAS_OF}")

        _sharp_runtime_component_key(
            aliased_component_key
            "${SHARP_COMPONENT_ALIAS_OF}"
        )
        set_property(
            GLOBAL APPEND PROPERTY
            "SHARP_RUNTIME_COMPONENT_${aliased_component_key}_ALIASES"
            "${SHARP_COMPONENT_NAME}"
        )
    else()
        if(NOT SHARP_COMPONENT_TARGET)
            message(FATAL_ERROR
                "Sharp Runtime component '${SHARP_COMPONENT_NAME}' must have a TARGET")
        endif()
        if(NOT SHARP_COMPONENT_TYPE MATCHES "^(STATIC|INTERFACE)$")
            message(FATAL_ERROR
                "Sharp Runtime component '${SHARP_COMPONENT_NAME}' has invalid TYPE "
                "'${SHARP_COMPONENT_TYPE}'")
        endif()
        if(SHARP_COMPONENT_TYPE STREQUAL "STATIC" AND NOT SHARP_COMPONENT_SOURCES)
            message(FATAL_ERROR
                "Compiled component '${SHARP_COMPONENT_NAME}' has no sources")
        endif()
        if(SHARP_COMPONENT_TYPE STREQUAL "INTERFACE" AND SHARP_COMPONENT_SOURCES)
            message(FATAL_ERROR
                "Header-only component '${SHARP_COMPONENT_NAME}' may not have sources")
        endif()
        if(NOT SHARP_COMPONENT_INCLUDE_DIRECTORY
                OR NOT IS_DIRECTORY "${SHARP_COMPONENT_INCLUDE_DIRECTORY}")
            message(FATAL_ERROR
                "Sharp Runtime component '${SHARP_COMPONENT_NAME}' has no valid "
                "INCLUDE_DIRECTORY")
        endif()

        set(component_type "${SHARP_COMPONENT_TYPE}")
        set(component_target "${SHARP_COMPONENT_TARGET}")
        set(component_dependencies "${SHARP_COMPONENT_DEPENDENCIES}")
    endif()

    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_REGISTERED"
        TRUE
    )
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_NAME"
        "${SHARP_COMPONENT_NAME}"
    )
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_TYPE"
        "${component_type}"
    )
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_TARGET"
        "${component_target}"
    )
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_SOURCES"
        "${SHARP_COMPONENT_SOURCES}"
    )
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_DEPENDENCIES"
        "${component_dependencies}"
    )
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_SETUP"
        "${SHARP_COMPONENT_SETUP}"
    )
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_INCLUDE_DIRECTORY"
        "${SHARP_COMPONENT_INCLUDE_DIRECTORY}"
    )

    set_property(
        GLOBAL APPEND PROPERTY
        SHARP_RUNTIME_REGISTERED_COMPONENTS
        "${SHARP_COMPONENT_NAME}"
    )

    if(NOT component_type STREQUAL "ALIAS")
        set_property(
            GLOBAL APPEND PROPERTY
            SHARP_RUNTIME_PRIMARY_COMPONENTS
            "${SHARP_COMPONENT_NAME}"
        )
    endif()

    if(component_type STREQUAL "STATIC")
        set_property(
            GLOBAL APPEND PROPERTY
            SHARP_RUNTIME_REGISTERED_SOURCES
            ${SHARP_COMPONENT_SOURCES}
        )
    endif()
endfunction()

function(sharp_runtime_register_module)
    set(options)
    set(one_value_args NAME TARGET TYPE SETUP)
    set(multi_value_args DEPENDENCIES)
    cmake_parse_arguments(
        SHARP_MODULE
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN}
    )

    set(module_sources)
    if(SHARP_MODULE_TYPE STREQUAL "STATIC")
        file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
        )
    elseif(NOT SHARP_MODULE_TYPE STREQUAL "INTERFACE")
        message(FATAL_ERROR
            "Sharp Runtime module '${SHARP_MODULE_NAME}' has invalid TYPE "
            "'${SHARP_MODULE_TYPE}'")
    endif()

    sharp_runtime_register_component(
        NAME "${SHARP_MODULE_NAME}"
        TARGET "${SHARP_MODULE_TARGET}"
        TYPE "${SHARP_MODULE_TYPE}"
        SOURCES ${module_sources}
        DEPENDENCIES ${SHARP_MODULE_DEPENDENCIES}
        SETUP "${SHARP_MODULE_SETUP}"
        INCLUDE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
endfunction()

function(sharp_runtime_validate_source_partition)
    file(GLOB_RECURSE all_runtime_sources CONFIGURE_DEPENDS
        "${SHARP_RUNTIME_ROOT}/modules/*/src/*.cpp"
    )
    get_property(
        registered_sources
        GLOBAL
        PROPERTY SHARP_RUNTIME_REGISTERED_SOURCES
    )

    set(unique_registered_sources)
    set(duplicate_sources)
    foreach(source IN LISTS registered_sources)
        list(FIND unique_registered_sources "${source}" source_index)
        if(source_index EQUAL -1)
            list(APPEND unique_registered_sources "${source}")
        else()
            list(APPEND duplicate_sources "${source}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES duplicate_sources)

    set(missing_sources)
    foreach(source IN LISTS all_runtime_sources)
        list(FIND unique_registered_sources "${source}" source_index)
        if(source_index EQUAL -1)
            list(APPEND missing_sources "${source}")
        endif()
    endforeach()

    set(unknown_sources)
    foreach(source IN LISTS unique_registered_sources)
        list(FIND all_runtime_sources "${source}" source_index)
        if(source_index EQUAL -1)
            list(APPEND unknown_sources "${source}")
        endif()
    endforeach()

    if(duplicate_sources OR missing_sources OR unknown_sources)
        message(FATAL_ERROR
            "Invalid Sharp Runtime source partition.\n"
            "Sources assigned more than once: ${duplicate_sources}\n"
            "Sources without a component: ${missing_sources}\n"
            "Registered paths that do not exist: ${unknown_sources}")
    endif()
endfunction()

function(sharp_runtime_enable_component component)
    if(component STREQUAL "All")
        if(TARGET SharpRuntime::All)
            return()
        endif()

        get_property(
            primary_components
            GLOBAL
            PROPERTY SHARP_RUNTIME_PRIMARY_COMPONENTS
        )
        foreach(primary_component IN LISTS primary_components)
            sharp_runtime_enable_component("${primary_component}")
        endforeach()

        add_library(sharp_runtime_all INTERFACE)
        add_library(SharpRuntime::All ALIAS sharp_runtime_all)
        target_link_libraries(
            sharp_runtime_all
            INTERFACE
                SharpRuntime::Headers
        )
        foreach(primary_component IN LISTS primary_components)
            target_link_libraries(
                sharp_runtime_all
                INTERFACE
                    "SharpRuntime::${primary_component}"
            )
        endforeach()

        # Compatibility for existing add_subdirectory() consumers. It is only
        # available when All is requested, because making it unconditional
        # would instantiate every optional component and external dependency.
        if(NOT TARGET SHARP_RUNTIME)
            add_library(SHARP_RUNTIME INTERFACE)
            target_link_libraries(SHARP_RUNTIME INTERFACE SharpRuntime::All)
        endif()
        return()
    endif()

    if(TARGET "SharpRuntime::${component}")
        return()
    endif()

    _sharp_runtime_component_key(component_key "${component}")
    get_property(
        registered
        GLOBAL
        PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_REGISTERED"
    )
    if(NOT registered)
        get_property(
            registered_components
            GLOBAL
            PROPERTY SHARP_RUNTIME_REGISTERED_COMPONENTS
        )
        list(JOIN registered_components ", " component_choices)
        message(FATAL_ERROR
            "Unknown Sharp Runtime component '${component}'. "
            "Available components: ${component_choices}, All")
    endif()

    get_property(
        component_state
        GLOBAL
        PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_STATE"
    )
    if(component_state STREQUAL "ENABLING")
        message(FATAL_ERROR
            "Cyclic Sharp Runtime component dependency involving '${component}'")
    endif()
    if(component_state STREQUAL "ENABLED")
        return()
    endif()
    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_STATE"
        "ENABLING"
    )

    get_property(
        component_dependencies
        GLOBAL
        PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_DEPENDENCIES"
    )
    foreach(dependency IN LISTS component_dependencies)
        sharp_runtime_enable_component("${dependency}")
    endforeach()

    # Enabling a physical component also creates all names that alias it. An
    # explicitly requested alias therefore already exists after its dependency
    # has been enabled.
    if(TARGET "SharpRuntime::${component}")
        set_property(
            GLOBAL PROPERTY
            "SHARP_RUNTIME_COMPONENT_${component_key}_STATE"
            "ENABLED"
        )
        set_property(
            GLOBAL APPEND PROPERTY
            SHARP_RUNTIME_ENABLED_COMPONENTS
            "${component}"
        )
        return()
    endif()

    get_property(
        component_type
        GLOBAL
        PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_TYPE"
    )

    if(component_type STREQUAL "ALIAS")
        list(GET component_dependencies 0 aliased_component)
        _sharp_runtime_component_key(aliased_key "${aliased_component}")
        get_property(
            aliased_target
            GLOBAL
            PROPERTY "SHARP_RUNTIME_COMPONENT_${aliased_key}_TARGET"
        )
        add_library("SharpRuntime::${component}" ALIAS "${aliased_target}")
    else()
        get_property(
            component_target
            GLOBAL
            PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_TARGET"
        )
        get_property(
            component_include_directory
            GLOBAL
            PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_INCLUDE_DIRECTORY"
        )

        if(component_type STREQUAL "STATIC")
            get_property(
                component_sources
                GLOBAL
                PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_SOURCES"
            )
            add_library("${component_target}" STATIC ${component_sources})
            target_link_libraries(
                "${component_target}"
                PUBLIC
                    SharpRuntime::Headers
            )
            target_include_directories(
                "${component_target}"
                PUBLIC
                    "$<BUILD_INTERFACE:${component_include_directory}>"
            )
            sharp_runtime_apply_build_options("${component_target}")
        elseif(component_type STREQUAL "INTERFACE")
            add_library("${component_target}" INTERFACE)
            target_link_libraries(
                "${component_target}"
                INTERFACE
                    SharpRuntime::Headers
            )
            target_include_directories(
                "${component_target}"
                INTERFACE
                    "$<BUILD_INTERFACE:${component_include_directory}>"
            )
        else()
            message(FATAL_ERROR
                "Unsupported component type '${component_type}' for '${component}'")
        endif()

        add_library(
            "SharpRuntime::${component}"
            ALIAS
            "${component_target}"
        )

        get_property(
            component_aliases
            GLOBAL
            PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_ALIASES"
        )
        foreach(component_alias IN LISTS component_aliases)
            if(NOT TARGET "SharpRuntime::${component_alias}")
                add_library(
                    "SharpRuntime::${component_alias}"
                    ALIAS
                    "${component_target}"
                )
            endif()
        endforeach()

        foreach(dependency IN LISTS component_dependencies)
            if(component_type STREQUAL "STATIC")
                target_link_libraries(
                    "${component_target}"
                    PUBLIC
                        "SharpRuntime::${dependency}"
                )
            else()
                target_link_libraries(
                    "${component_target}"
                    INTERFACE
                        "SharpRuntime::${dependency}"
                )
            endif()
        endforeach()

        get_property(
            component_setup
            GLOBAL
            PROPERTY "SHARP_RUNTIME_COMPONENT_${component_key}_SETUP"
        )
        if(component_setup)
            cmake_language(CALL "${component_setup}" "${component_target}")
        endif()
    endif()

    set_property(
        GLOBAL PROPERTY
        "SHARP_RUNTIME_COMPONENT_${component_key}_STATE"
        "ENABLED"
    )
    set_property(
        GLOBAL APPEND PROPERTY
        SHARP_RUNTIME_ENABLED_COMPONENTS
        "${component}"
    )
endfunction()

function(sharp_runtime_enable_components)
    foreach(component IN LISTS ARGN)
        sharp_runtime_enable_component("${component}")
    endforeach()
endfunction()

function(sharp_runtime_get_enabled_components output)
    get_property(
        enabled_components
        GLOBAL
        PROPERTY SHARP_RUNTIME_ENABLED_COMPONENTS
    )
    set("${output}" "${enabled_components}" PARENT_SCOPE)
endfunction()
