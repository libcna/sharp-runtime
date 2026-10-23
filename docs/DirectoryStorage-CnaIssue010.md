<!-- SPDX-License-Identifier: MIT -->
# Directory creation and enumeration for CNA-DEMOS-ISSUE-010

At parent4c7c69bd731b53bbb2fd87cb0f8da233278eb67d, Directory already rejects existing
files and maps enumeration failures to System exceptions. However, embedded NUL in
CreateDirectory/GetFiles/GetDirectories is passed to native filesystem functions,
which act on the prefix. Five new DirectoryStorageTests give4PASS/1FAIL on Linux
and4PASS/1FAIL for actual Windows-targeted product code under Win64 Wine.

The narrow repair validates these operation paths before filesystem access and uses
the existing Utf8Path.hpp NativePath/Utf8Of conversions. This preserves the UTF-8
contract required when CNA delegates to Directory. Unicode tests passed on the
MinGW/Wine baseline too; no Unicode runtime failure is claimed for that environment.
Matching rules, other Directory methods and public signatures/layouts are unchanged.

Actual .NET8.0.31/Unix DirectoryOperationsOracle12/12 confirms NUL refusal with exact
ArgumentException, absence of prefix side effects, Unicode creation/listing,
existing-file IOException/preservation and idempotent directory creation.
The genuine XNA StorageOperationsOracle12/12 independently supplies the consumer's
creation/enumeration contract. Both sources are retained in cna-demos storage evidence;
the new DirectoryOperationsOracle is published with the matching consumer evidence batch.

Validation: warning-free full incremental build;18,228/18,228 tests across42 executables,
zero failures/skips, including754 IO tests and the original Yacht SOAP fixture on a private
port. Boundaries44 modules/109 edges pass. Actual Windows-targeted five tests pass5/5
under Win64 Wine after the fix; native Windows/MSVC and browser runtime are not claimed.
There is no public layout/signature change or new memory-lifetime code requiring a
sanitizer claim. CNA's new native facade tests independently change53PASS/2FAIL to55PASS.

Reports: ../cna-demos/build/linux-full/reports/repair-storage-directory-*.
The Windows probe uses the source/flags pattern in WindowsFileIoValidation.md but inputs
Directory.cpp, DirectoryStorageTests.cpp, gtest-all.cc/gtest_main.cc and these core sources:
Exception.cpp, SystemException.cpp, ArgumentException.cpp, UnauthorizedAccessException.cpp,
IO/IOException.cpp and IO/DirectoryNotFoundException.cpp. Include IO/Core headers and gtest.
Output build-probe/cna-repair-directory-windows-tests.exe; C++23/-O1/-g0/-static,
-Wall/-Wextra/-Wpedantic/-Werror and SHARP_RUNTIME_HAS_NATIVE_INT128=1. Native gate reuses
build/, one compiler, shared ccache; all builds have MemoryHigh4GiB/MemoryMax6GiB/no swap;
cross-compiler TMPDIR is the existing build-tmp. No source/test substitution under Wine.

This is a partial ISSUE-010 repair, not closure of all storage/content findings.

## Creation failure categories (2026-10-23)

CreateDirectory reported every native failure as IOException. Actual .NET 8.0.31/Unix
`DirectoryCreationErrorsOracle.cs` (cna-demos storage evidence) gives
UnauthorizedAccessException for an unwritable parent and PathTooLongException for a component
longer than NAME_MAX; genuine XNA container construction under Wine reports the same denial
type. Two new POSIX tests failed (2 of 7 DirectoryStorageTests) before the change, which maps
EACCES/EPERM and ENAMETOOLONG accordingly and leaves every other failure an IOException.

A file anywhere in the chain stays an exact IOException. That matches genuine XNA/.NET
Framework 4 and .NET on Windows; .NET 8 on Unix instead reports a file *ancestor* as
DirectoryNotFoundException (measured, 1 of 6 oracle checks). Sharp keeps the existing
IOException there because CNA's XNA layer relies on it; this is a recorded difference, not a
claim of Unix .NET parity.

Validation: warning-free full build; IO 756/756; full gate 18,230/18,230 across 42 executables,
zero failures/skips, with the XNA sample fixtures and the original Yacht SOAP fixture on a
private port; boundaries 44/109. MinGW-w64 strict C++23 syntax check of Directory.cpp passes;
no Windows runtime is claimed for the change. Reports:
../cna-demos/build/linux-full/reports/repair-storage-ctor-*.
