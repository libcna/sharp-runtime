<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Installed CMake package — cna-engine UP-001 prerequisite

Sharp Runtime can be consumed without its source checkout:

```cmake
find_package(SharpRuntime 0.1 CONFIG REQUIRED COMPONENTS Core.Base IO)
target_link_libraries(game PRIVATE SharpRuntime::Core.Base SharpRuntime::IO)
```

Build the desired `SHARP_RUNTIME_COMPONENTS` selection, then install it:

```sh
cmake --build build --parallel 1
cmake --install build --component SharpRuntime --prefix /path/to/sdk
```

Set the consumer's `CMAKE_PREFIX_PATH` to that prefix. The package version comes
from the existing project release version, with SameMinorVersion compatibility;
it is not a promise of binary compatibility across compiler/toolchain/ABI changes.
Use compatible compiler, standard library, architecture and build settings.

`SHARP_RUNTIME_INSTALL` defaults to ON for a standalone Sharp Runtime build and
OFF when embedded with `add_subdirectory`. The parent can explicitly enable it,
but must make any build-policy targets it attaches to the dependency exportable
or keep them build-local while preserving necessary installed ABI/link flags.
This matters for CNA's private instrumentation, debug-info, Emscripten ABI and
size-policy targets. The first export commit 42847935 enabled exports in every
source consumer and exposed these unexported parent links during CNA generation;
the follow-up preserves the source-consumer default while allowing explicit SDK
composition. `test/installed` has a separate configure-only parent-policy check,
followed by the source-independent installed tests (no fallback to source).

The package exports the actual configured component graph. A Core.Base-only
build does not provide IO, JSON, XML or All. `Xml.XPath` remains an alias of Xml,
not a second archive. `SharpRuntime::All` and the compatibility `SHARP_RUNTIME`
target exist only when All was enabled. Missing required components are rejected
at configure time. As with source builds, C++23, the native-int128 capability and
the generated `SharpRuntime/Version.hpp` reach consumers through Headers.

Public include roots remain physically separated under `include/sharp-runtime`:
linking Core.Base does not make IO or JSON headers visible, even in a full SDK.
Vendored nlohmann headers and the namespaced tinyxml2 headers/archive are included
only when needed. ZIP includes its private miniz archive. Native IO.Compression
uses `find_dependency(ZLIB)`; Android Storage requests SDL3 when it was linked
when building the package. These external SDK dependencies must be available in
the consuming toolchain. Emscripten retains its zlib port and, for threaded builds,
publishes the shared-memory pthread ABI flags. No vendor implementation changed.
The project/.NET notices are installed in `share/sharp-runtime/LICENSE`; the
original miniz files accompany its archive under `share/sharp-runtime/vendor`
because their copyright and permission notices reside in the C sources.

Relative GNUInstallDirs destinations produce a relocatable package: generated
target/configuration files contain no source or build directory paths. A full
package discovers dependencies for its configured graph, not just the component
named in a consumer's `COMPONENTS` list. Use a selective build when distributing a
minimal SDK. This work is the lower-layer prerequisite of CNA's installed C++
package; it does not, by itself, resolve CNA UP-001.

## Regression validation

`scripts/check_installed_package.py` installs an already-built graph to a fresh
explicit path under `build-probe`, renames the prefix, checks exported files for
source/build paths, and configures the source-independent `test/installed` fixture
in the reusable `build-consumer` directory. It verifies missing components and
incompatible versions are refused, then builds with strict warnings and executes
the positive tests. It uses one worker and never deletes existing artifacts.

```sh
python3 scripts/check_installed_package.py build --mode full \
  --stage "$PWD/build-probe/package-full-stage"
python3 scripts/check_installed_package.py build-modular --mode core \
  --stage "$PWD/build-probe/package-core-stage"
```

The second command requires a Core.Base-only `build-modular` configuration. Both
commands deliberately fail if either staging prefix already exists, so callers
must archive evidence and remove only their own completed test artifacts before
reusing a path. They do not clean or rebuild the producer libraries.

Native Linux/GCC validation uses existing `build` (All), standard `build-modular`
(Core.Base), and `build-consumer`, shared ccache, one compiler worker, and a
MemoryHigh=4GiB / MemoryMax=6GiB / MemorySwapMax=0 scope. Full tests exercise JSON,
XML/tinyxml2, zlib DEFLATE and miniz ZIP round trips; the minimal consumer calls
compiled Guid methods and compile-checks the narrow header closure. These are
not Windows, Android or Emscripten runtime validation claims.

2026-10-22 checkpoint: both full-package consumer tests and the Core.Base-only
consumer passed after relocation, with no original prefix remaining. Unknown
components and version 99.0 were rejected by both packages; the selective package
also rejected known-but-uninstalled Xml. The producer's full gate passed
18,195/18,195 tests across 41 executables, zero failures/skips/compiler warnings;
the boundary validator passed. Baseline was 18,193/18,193; the two additions protect
the independently corrected stochastic test oracle (7046edbb) in RandomEntropyTestOracle.md.
No runtime implementation changed for packaging. Evidence is retained under
cna-demos `build/linux-full/reports/repair-runtime-install-final-v2-*` and
`repair-runtime-package-after-v2-*`. Producer-gate memory peak 349,163,520 bytes,
package-gate peak 615,297,024 bytes, zero OOM/high/max events. Package validation
ran serially after the producer gate.

Embedded-policy follow-up: CNA's existing `cmake-build-multi` configuration and
generation now pass, including its SDL/platform audits (its pre-existing Draco
CMP0148 developer warning is unchanged). Sharp's full gate again passed
18,195/18,195 with zero failures/skips/warnings, memory peak 100,278,272 bytes,
events zero (`repair-runtime-package-after-v3-*`). Both installed consumer modes
and both configure-only parent-policy checks passed (`repair-runtime-install-final-v4-*`),
peak 617,811,968 bytes/events zero. The final-v3 fixture failure, caused by applying
PRIVATE compiler options to the fixture's new INTERFACE helper, is retained in
the preceding log; strict flags now apply only to its executable targets. No
producer or consumer runtime source changed in this follow-up. A full CNA runtime
suite has not been rerun for this configure-only integration change.
