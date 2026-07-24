<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Sharp Runtime CMake components

Sharp Runtime exposes independently selectable CMake targets. Applications
request only their direct components; Sharp Runtime resolves and enables the
transitive dependency closure.

No C++ source or public header is moved by this split. The existing
`include/System/...` and `src/System/...` layouts remain the source of truth.
The components provide build, link, and external-dependency isolation; they do
not prevent an application from including a header owned by a component it did
not request. Using an out-of-line API from such a header will normally fail at
link time.

## Selecting components

Set `SHARP_RUNTIME_COMPONENTS` before adding the project:

```cmake
set(SHARP_RUNTIME_COMPONENTS
    Net.WebSockets
    Text.Json
)
set(SHARP_RUNTIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(path/to/sharp-runtime)

target_link_libraries(MyApp PRIVATE
    SharpRuntime::Net.WebSockets
    SharpRuntime::Text.Json
)
```

The application does not list transitive dependencies. In this example,
`Net.WebSockets` enables `Net.Sockets`, `Net`, `IO`, `Threading.Tasks`,
`Threading`, and `Core`; `Text.Json` adds `Text` and `Collections`.

For a standalone configuration, pass a semicolon-separated list:

```bash
cmake -S . -B build-components \
  -DSHARP_RUNTIME_BUILD_TESTS=OFF \
  '-DSHARP_RUNTIME_COMPONENTS=IO;IO.Hashing'
cmake --build build-components --parallel 4
```

An unset or empty component list selects `All`. The repository-wide tests and
benchmarks also enable `All`, regardless of a narrower requested list.

## Compiled components

| Target | Direct Sharp Runtime dependencies | Optional external/platform dependency |
|---|---|---|
| `SharpRuntime::Core` | — | — |
| `SharpRuntime::Diagnostics` | Core | — |
| `SharpRuntime::Globalization` | Core | — |
| `SharpRuntime::Numerics` | Core | — |
| `SharpRuntime::Runtime` | Core | — |
| `SharpRuntime::Text` | Core | — |
| `SharpRuntime::Text.Json` | Core, Text, Collections | — |
| `SharpRuntime::Threading` | Core | — |
| `SharpRuntime::Threading.Tasks` | Core, Threading | — |
| `SharpRuntime::Timers` | Core, Threading | — |
| `SharpRuntime::IO` | Core | — |
| `SharpRuntime::IO.Compression` | Core, IO, Buffers | ZLIB |
| `SharpRuntime::IO.Compression.Zip` | Core, IO | vendored miniz |
| `SharpRuntime::IO.Hashing` | Core, IO | — |
| `SharpRuntime::Storage` | — | SDL3 on Android when provided by the parent |
| `SharpRuntime::IO.IsolatedStorage` | Core, IO, Storage | — |
| `SharpRuntime::Net` | Core | `ws2_32` on Windows |
| `SharpRuntime::Net.Sockets` | Core, IO, Net | — |
| `SharpRuntime::Net.Http` | Core, IO, Net, Threading.Tasks | — |
| `SharpRuntime::Net.Http.Headers` | Core, Collections | — |
| `SharpRuntime::Net.Mime` | Core | — |
| `SharpRuntime::Net.NetworkInformation` | Core, Net | — |
| `SharpRuntime::Net.WebSockets` | Core, Net.Sockets, Threading.Tasks | — |
| `SharpRuntime::Security.Cryptography` | Core | — |
| `SharpRuntime::Security.Cryptography.Random` | Core | bcrypt on Windows |
| `SharpRuntime::Xml` | Core | vendored tinyxml2 |
| `SharpRuntime::Xml.Linq` | Core, Xml | — |

`SharpRuntime::Xml.XPath` is an alias of `SharpRuntime::Xml`. Xml and XPath
have mutual binary dependencies in the existing implementation, so they
intentionally share one physical archive.

`Core` also owns the implementations of `System::IO::IOException` and
`System::IO::DirectoryNotFoundException`. `Environment.cpp` needs the latter;
keeping these two implementations in Core avoids a `Core`/`IO` static-library
cycle without changing namespaces or source locations.

## Header-only components

These targets currently have no `.cpp` sources, but provide stable ownership
and dependency names:

- `SharpRuntime::Buffers`
- `SharpRuntime::Collections`
- `SharpRuntime::ComponentModel`
- `SharpRuntime::Security`
- `SharpRuntime::Text.RegularExpressions`
- `SharpRuntime::Threading.Channels`
- `SharpRuntime::Net.Security`
- `SharpRuntime::Net.Http.Json`

## External dependency isolation

External libraries are configured only when their owning component is enabled:

- `IO.Compression` calls `find_package(ZLIB REQUIRED)`.
- `IO.Compression.Zip` builds the vendored miniz sources.
- `Xml` builds the vendored tinyxml2 source.
- `Net` propagates `ws2_32` only on Windows.
- `Security.Cryptography.Random` propagates bcrypt only on Windows.
- `Storage` links an existing `SDL3::SDL3` or `SDL3::SDL3-static` target only
  on Android.

Consequently, a JSON-only application does not need any compression, XML,
socket, crypto RNG, or SDL dependency.

## Compatibility and migration

`SharpRuntime::All` is an interface umbrella over every component. When `All`
is enabled, the legacy `SHARP_RUNTIME` CMake target is also created and
forwards to it:

```cmake
target_link_libraries(ExistingApp PRIVATE SHARP_RUNTIME)
```

Existing consumers can therefore migrate incrementally:

1. Keep the default `All` selection and the `SHARP_RUNTIME` target.
2. Replace `SHARP_RUNTIME` with `SharpRuntime::All`.
3. Set `SHARP_RUNTIME_COMPONENTS` and link only direct component targets.

The compatibility target is deliberately not created for a selective build:
creating an unconditional `SHARP_RUNTIME -> All` dependency would instantiate
every optional target and defeat component selection.

## Maintaining the partition

Component source lists live in `cmake/SharpRuntimeSources.cmake`; component
dependencies and platform setup live in `cmake/modules/`.

Every configure checks the complete `src/*.cpp` partition, including selective
builds. Configuration fails if a source is missing from the partition, is
assigned more than once, or points to a nonexistent path. This keeps a newly
added implementation from silently disappearing from one or more build modes.
