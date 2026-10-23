<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# FileStream native sharing — CNA-DEMOS-ISSUE-152

## Contract and migration

`FileStream(path, mode, access, share)` is additive. Existing constructors keep their
signatures and now use .NET's default `FileShare::Read`. `File::Create` explicitly
uses `FileShare::None`, unlike the short FileStream constructors. CNA's StorageContainer
facade must likewise pass its own exclusive defaults instead of inheriting FileStream's.

The owner selected native .NET platform behavior, not Windows emulation on Unix:

- Windows uses one `CreateFileW` handle with the requested access/share flags.
- POSIX uses one descriptor with nonblocking `flock`: None is exclusive and other
  share values are shared. Only contention is fatal; unsupported/denied locking is
  best-effort, as in .NET Unix. This does not prevent non-cooperating tools from opening
  a file, nor promise Windows's read/write/delete mask semantics on Unix.
- Browser virtual filesystems do not acquire mandatory OS sharing by this change.
  Emscripten compile validation is not a claim of browser cross-process locking.

Reference measurements in `../cna-demos/docs/repair-evidence/storage/` use actual
.NET Framework4 on Wine/Windows and .NET8 on Linux: of144 access/share pairs,
Windows refuses119 and allows25; Linux refuses63 and allows81. The native tests
assert these distinct matrices, not a weakened common denominator.

Rejected Create/Truncate opens must leave existing bytes intact. POSIX therefore
opens without O_TRUNC, acquires the lock, and only then truncates. Write-only
Open/OpenOrCreate no longer truncate existing files or silently request read permission.
Append starts at the initial end, disallows seeking/truncating before that boundary,
and permits rewriting bytes appended by this stream. Explicit Flush/Close report write
errors; destruction remains nonthrowing and always releases the handle.
Invalid mode/access/share enum values are rejected before opening a file, with
ArgumentOutOfRangeException naming the offending parameter.

## Why the backend changes

The current C++23 fstream has no portable native-handle opening path. Adding a second
locking handle would conflict with its own fstream open on Windows when sharing is
None. Adding ios::in to stop write-only truncation would incorrectly demand read access.
The localized replacement owns one native descriptor through buffered stdio; fdopen
adopts that descriptor without reopening or truncating the file. Native length and
resize operate on that same handle even if its path has been renamed/replaced.
Read/write transitions, repeated EOF, failed-constructor cleanup, buffering, and resize
after read-ahead have explicit regressions. No process-local pathname lock table is used.

## ABI / SA-3

**Rebuild all consumers from source. Do not mix old FileStream headers/objects with the
new library**, including derived IsolatedStorageFileStream and consumers embedding either
type. On this Linux x86_64 GCC14.2/libstdc++ build, sizeof(FileStream) changes **576 ->24**,
alignof stays8; Stream itself remains sizeof8/align8. The same regression test reconstructs
the old member layout and pins both values. Copy/move construction and assignment remain
disabled; the destructor remains nothrow. No existing signature, base, virtual member or
exception specification changes. The implementation-private handle is hidden behind a
unique_ptr, so no platform headers enter the public API.

Read-only downstream search found FileStream references in19 CNA module source/header
files, plus derived IsolatedStorageFileStream usage in mobile-eggbert's Worlds.cpp.
No source spelling migration is needed. The latter consumer was inspected, not edited
or rebuilt. CNA/cna-demos require their normal dependency-aware rebuild before validation.

## Sources

- `System/IO/FileStream.cs`: DefaultShare=Read and constructor delegation.
- `System/IO/Strategies/FileStreamHelpers.cs:63-113`: enum validation and mode/access guards.
- `Microsoft/Win32/SafeHandles/SafeFileHandle.Unix.cs:295-360,413-430,481-499`:
  native flags, advisory sharing and lock-before-truncate.
- `System/IO/Strategies/OSFileStreamStrategy.cs:166-185`: append boundaries.

Those local source citations refer to `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/`,
a .NET11 preview snapshot; the measured4/8 contract evidence above is independent.
Descriptor adoption/buffering follows [POSIX fdopen](https://pubs.opengroup.org/onlinepubs/9699919799.orig/functions/fdopen.html)
and [Microsoft _fdopen](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fdopen-wfdopen).
POSIX input-buffer invalidation uses [fflush](https://pubs.opengroup.org/onlinepubs/9699919799.2013edition/functions/fflush.html);
Windows uses positioning because [its fflush](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fflush)
does not discard input buffers.

## Validation checkpoint

Baseline31c7c08f: IO727 existing passes plus3 new failures (missing share overload and
write-only Open/OpenOrCreate truncation). First native-handle run740/741 exposed stale
read-ahead after resize; the regression was retained and the implementation corrected.
Subsequent741/741 passed. After three additional alias/permission/write-error tests,
the complete warning-free Sharp build and full component gate pass **18,218/18,218**,
42 executables, zero failures/skips (IO744). The original Yacht service ran privately
with the existing XNA fixture layout. Module boundaries44/109 pass unchanged.

MinGW and Emscripten final-source syntax checks pass with Wall/Wextra/Wpedantic/Werror;
neither is runtime validation. The actual FileStream implementation and23 focused
sharing/path/EOF regressions pass ASan/UBSan/leak detection. Other linked Sharp archives
are not sanitizer-instrumented, so this is not a whole-library sanitizer claim.
CNA storage consumers were rebuilt:46/50 passed before CNA forwarding/default repairs,
then50/50 passed with those repairs. Full demo rebuild/revalidation is still required;
do not treat this dependency checkpoint as final ISSUE-152 closure.
Evidence is under `../cna-demos/build/linux-full/reports/repair-fileshare-*`.
Build directories: existing `build/`, focused probes in existing `build-probe/`;
one compiler at a time, shared ccache, build MemoryHigh4GiB/MemoryMax6GiB/no swap.
