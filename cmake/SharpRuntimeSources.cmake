# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

include_guard(GLOBAL)

function(_sharp_runtime_collect_module_sources output module)
    file(GLOB_RECURSE module_sources CONFIGURE_DEPENDS
        "${SHARP_RUNTIME_ROOT}/modules/${module}/src/*.cpp"
    )
    set("${output}" "${module_sources}" PARENT_SCOPE)
endfunction()

_sharp_runtime_collect_module_sources(SHARP_RUNTIME_CORE_SOURCES core)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_DIAGNOSTICS_SOURCES
    diagnostics
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_GLOBALIZATION_SOURCES
    globalization
)
_sharp_runtime_collect_module_sources(SHARP_RUNTIME_NUMERICS_SOURCES numerics)
_sharp_runtime_collect_module_sources(SHARP_RUNTIME_RUNTIME_SOURCES runtime)

_sharp_runtime_collect_module_sources(SHARP_RUNTIME_TEXT_SOURCES text)
_sharp_runtime_collect_module_sources(SHARP_RUNTIME_TEXT_JSON_SOURCES text-json)

_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_THREADING_SOURCES
    threading
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_THREADING_TASKS_SOURCES
    threading-tasks
)
_sharp_runtime_collect_module_sources(SHARP_RUNTIME_TIMERS_SOURCES timers)

_sharp_runtime_collect_module_sources(SHARP_RUNTIME_IO_SOURCES io)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_IO_COMPRESSION_SOURCES
    io-compression
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_IO_COMPRESSION_ZIP_SOURCES
    io-compression-zip
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_IO_HASHING_SOURCES
    io-hashing
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_IO_ISOLATED_STORAGE_SOURCES
    io-isolated-storage
)
_sharp_runtime_collect_module_sources(SHARP_RUNTIME_STORAGE_SOURCES storage)

_sharp_runtime_collect_module_sources(SHARP_RUNTIME_NET_SOURCES net)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_NET_HTTP_SOURCES
    net-http
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_NET_HTTP_HEADERS_SOURCES
    net-http-headers
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_NET_MIME_SOURCES
    net-mime
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_NET_NETWORK_INFORMATION_SOURCES
    net-network-information
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_NET_SOCKETS_SOURCES
    net-sockets
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_NET_WEBSOCKETS_SOURCES
    net-websockets
)

_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_CRYPTOGRAPHY_SOURCES
    security-cryptography
)
_sharp_runtime_collect_module_sources(
    SHARP_RUNTIME_CRYPTOGRAPHY_RANDOM_SOURCES
    security-cryptography-random
)

_sharp_runtime_collect_module_sources(SHARP_RUNTIME_XML_SOURCES xml)
_sharp_runtime_collect_module_sources(SHARP_RUNTIME_XML_LINQ_SOURCES xml-linq)
