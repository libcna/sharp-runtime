# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

function(_sharp_runtime_setup_net target)
    if(WIN32)
        target_link_libraries("${target}" PRIVATE ws2_32)
    endif()
endfunction()

sharp_runtime_register_component(
    NAME Net
    TARGET sharp_runtime_net
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NET_SOURCES}
    DEPENDENCIES Core
    SETUP _sharp_runtime_setup_net
)

sharp_runtime_register_component(
    NAME Net.Sockets
    TARGET sharp_runtime_net_sockets
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NET_SOCKETS_SOURCES}
    DEPENDENCIES Core IO Net
)

sharp_runtime_register_component(
    NAME Net.Http
    TARGET sharp_runtime_net_http
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NET_HTTP_SOURCES}
    DEPENDENCIES Core IO Net Threading.Tasks
)

sharp_runtime_register_component(
    NAME Net.Http.Headers
    TARGET sharp_runtime_net_http_headers
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NET_HTTP_HEADERS_SOURCES}
    DEPENDENCIES Core Collections
)

sharp_runtime_register_component(
    NAME Net.Mime
    TARGET sharp_runtime_net_mime
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NET_MIME_SOURCES}
    DEPENDENCIES Core
)

sharp_runtime_register_component(
    NAME Net.NetworkInformation
    TARGET sharp_runtime_net_network_information
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NET_NETWORK_INFORMATION_SOURCES}
    DEPENDENCIES Core Net
)

sharp_runtime_register_component(
    NAME Net.WebSockets
    TARGET sharp_runtime_net_websockets
    TYPE STATIC
    SOURCES ${SHARP_RUNTIME_NET_WEBSOCKETS_SOURCES}
    DEPENDENCIES Core Net.Sockets Threading.Tasks
)
