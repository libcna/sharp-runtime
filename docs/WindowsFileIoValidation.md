<!-- SPDX-License-Identifier: MIT -->
# Windows-targeted file I/O regression execution (2026-10-23)

At production revision `d051307b3316903a692846acf78200d74339bbef`, the actual unchanged
FileStream/File implementations and existing FileStreamSharingTests/FileDeletionTests
compile into a static MinGW x86_64 Windows executable and pass **16/16 tests under Wine**,
34ms test time. No test shim substitutes the native opening/deletion implementation.

The12 sharing/stream tests include144 native access/share pairs (119 refused/25 admitted),
lock-before-truncate, alias conflicts, release/defaults, length/resize after rename,
buffered read/write/EOF, append boundaries, invalid sharing, and write-only preservation.
The4 deletion tests cover exact directory refusal and preservation, missing parent versus
leaf, NUL-prefix preservation, and real Unicode-path deletion with a surviving sibling.
Linux-only flock/fork/permission/symlink tests are not asserted to have run on Windows.

This is **Windows-targeted product execution under Wine**, not native Windows validation,
not the whole Sharp test suite, and not a Windows CNA facade/demo execution. Native Windows
validation remains pending. The existing Win64 prefix was used with DISPLAY/WAYLAND_DISPLAY
unset; no window appeared on the owner's desktop.

## Reproduction

From the Sharp root, use the existing build-probe and build-tmp directories. One compiler,
MemoryHigh4GiB/MemoryMax6GiB/no swap. Compile with MinGW GCC14-win32, C++23, `-O1 -g0 -static
-Wall -Wextra -Wpedantic -Werror -DSHARP_RUNTIME_HAS_NATIVE_INT128=1`. Include roots:
modules/io/include, modules/core/include, modules/uri/include, modules/time-zone/include,
vendor/googletest/googletest and (system include) vendor/googletest/googletest/include.
Compile these unmodified sources into build-probe/cna-repair-fileio-windows-tests.exe:

```
modules/io/src/System/IO/FileStream.cpp
modules/io/src/System/IO/File.cpp
modules/io/src/System/IO/Stream.cpp
modules/io/src/System/IO/FileNotFoundException.cpp
modules/core/src/System/Exception.cpp
modules/core/src/System/SystemException.cpp
modules/core/src/System/ArgumentException.cpp
modules/core/src/System/ArgumentNullException.cpp
modules/core/src/System/ArgumentOutOfRangeException.cpp
modules/core/src/System/InvalidOperationException.cpp
modules/core/src/System/ObjectDisposedException.cpp
modules/core/src/System/NotSupportedException.cpp
modules/core/src/System/UnauthorizedAccessException.cpp
modules/core/src/System/IO/IOException.cpp
modules/core/src/System/IO/DirectoryNotFoundException.cpp
modules/io/tests/System/IO/FileStreamSharingTests.cpp
modules/io/tests/System/IO/FileDeletionTests.cpp
vendor/googletest/googletest/src/gtest-all.cc
vendor/googletest/googletest/src/gtest_main.cc
```

Run with `env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all
WINEPREFIX=/home/robertvokac/.wine-cna-d3d11 wine <executable>`.
The initial `-O0` probe failed to link a duplicate `std::type_info::operator==` between
MinGW libstdc++ and its C++23 inline definition. Its diagnostic is retained. The normal
`-O1` compilation succeeds without warnings or any allow-multiple-definition workaround;
production and test sources are identical. The corrected invocation also explicitly places
compiler temporaries in build-tmp via TMPDIR; the first compiler used its default temporary
directory for automatically cleaned intermediate objects, not a separate build tree.

Evidence: ../cna-demos/build/linux-full/reports/repair-storage-fileio-windows-build.log,
repair-storage-fileio-windows-v2-build.log and repair-storage-fileio-windows-runtime.log.
The disposable executable can be rebuilt from this record and should be removed after capture.
