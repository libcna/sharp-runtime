#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MATRIX_ROOT="$(mktemp -d)"
trap 'rm -rf "$MATRIX_ROOT"' EXIT

MATRIX=(
    "Core.Base:core_base.cpp"
    "Collections.Blocking:blocking_collection.cpp"
    "Text.Json:text_json.cpp"
    "Net.Http.Headers:net_http_headers.cpp"
    "Net.WebSockets:net_websockets.cpp"
    "IO.Compression:io_compression.cpp"
    "IO.Compression.Zip:io_compression_zip.cpp"
    "IO.IsolatedStorage:io_isolated_storage.cpp"
    "Security.Cryptography.Random:security_cryptography_random.cpp"
    "Xml.Linq:xml_linq.cpp"
)

assert_target_absent() {
    local build_dir="$1"
    local target="$2"
    if find "$build_dir" -type d -name "${target}.dir" -print -quit | grep -q .; then
        echo "FAIL: selective build unexpectedly configured target $target" >&2
        exit 1
    fi
}

configure_consumer() {
    local build_dir="$1"
    local component="$2"
    local source_name="$3"

    cmake \
        -S "$REPO_ROOT/test/consumer" \
        -B "$build_dir" \
        -DSHARP_RUNTIME_SOURCE_DIR="$REPO_ROOT" \
        -DFIXTURE_COMPONENT="$component" \
        -DFIXTURE_SOURCE="$REPO_ROOT/test/consumer/$source_name" \
        -DFIXTURE_COMPILE_ONLY=ON \
        -DCMAKE_BUILD_TYPE=Debug \
        >/dev/null
}

expect_consumer_failure() {
    local component="$1"
    local source_name="$2"
    local fixture_name="${source_name%.cpp}"
    local build_dir="$MATRIX_ROOT/negative-$fixture_name"
    local build_log="$MATRIX_ROOT/negative-$fixture_name.log"

    configure_consumer "$build_dir" "$component" "$source_name"
    if cmake --build "$build_dir" --parallel 3 >"$build_log" 2>&1; then
        echo "FAIL: forbidden consumer fixture $fixture_name compiled" >&2
        exit 1
    fi
    if ! grep -Eq 'No such file|file not found|cannot open include file' "$build_log"; then
        echo "FAIL: forbidden consumer fixture $fixture_name failed unexpectedly" >&2
        tail -40 "$build_log" >&2
        exit 1
    fi
    echo "    negative fixture $fixture_name rejected"
    rm -rf "$build_dir"
}

check_component() {
    local component="$1"
    local source_name="$2"
    local component_key="${component//./_}"
    local build_dir="$MATRIX_ROOT/component-$component_key"

    echo "==> Checking selective component $component"
    cmake \
        -S "$REPO_ROOT" \
        -B "$build_dir" \
        -DSHARP_RUNTIME_COMPONENTS="$component" \
        -DSHARP_RUNTIME_BUILD_TESTS=ON \
        -DSHARP_RUNTIME_FIXTURE_COMPONENT="$component" \
        -DSHARP_RUNTIME_FIXTURE_SOURCE="$REPO_ROOT/test/consumer/$source_name" \
        -DCMAKE_PROJECT_INCLUDE="$REPO_ROOT/test/consumer/InjectFixture.cmake" \
        -DCMAKE_BUILD_TYPE=Debug \
        >/dev/null
    cmake --build "$build_dir" --parallel 3 >/dev/null

    if find "$build_dir" -maxdepth 1 -type f -perm -111 \
            -name 'SharpRuntimeTests_*' -print -quit | grep -q .; then
        "$REPO_ROOT/scripts/run_component_tests.sh" "$build_dir"
    fi

    "$build_dir/sharp_runtime_consumer_fixture"

    if [ "$component" = "Text.Json" ]; then
        assert_target_absent "$build_dir" sharp_runtime_threading
        assert_target_absent "$build_dir" sharp_runtime_component_model
        assert_target_absent "$build_dir" sharp_runtime_net
        assert_target_absent "$build_dir" sharp_runtime_miniz
        assert_target_absent "$build_dir" sharp_runtime_tinyxml2
        assert_target_absent "$build_dir" sharp_runtime_storage
        assert_target_absent "$build_dir" sharp_runtime_security_cryptography_random
        if grep -Eq '^ZLIB_(INCLUDE_DIR|LIBRARY)' "$build_dir/CMakeCache.txt"; then
            echo "FAIL: Text.Json unexpectedly configured ZLIB" >&2
            exit 1
        fi
        expect_consumer_failure Text.Json forbidden_text_json_collections.cpp
        expect_consumer_failure Text.Json forbidden_text_json_object_model.cpp
    elif [ "$component" = "Xml.Linq" ]; then
        expect_consumer_failure Xml.Linq forbidden_xml_diagnostics.cpp
    fi

    echo "    $component isolated consumer check passed"
    rm -rf "$build_dir"
}

if [ "$#" -eq 0 ]; then
    for matrix_entry in "${MATRIX[@]}"; do
        check_component "${matrix_entry%%:*}" "${matrix_entry#*:}"
    done
elif [ "$#" -eq 2 ]; then
    check_component "$1" "$2"
else
    echo "Usage: $0 [COMPONENT FIXTURE_SOURCE]" >&2
    exit 2
fi

echo "==> Selective component checks passed"
