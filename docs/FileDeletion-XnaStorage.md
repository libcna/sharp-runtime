<!-- SPDX-License-Identifier: MIT -->
# File-only deletion — CNA-DEMOS-ISSUE-010

## Confirmed contract and defect

CNA StorageContainer.DeleteFile used filesystem::remove directly, which can delete an
empty directory. Forwarding to the existing Sharp File::Delete alone was insufficient:
it rejected directories with the wrong exception, ignored a missing parent, could delete
the prefix of a NUL-containing path, and rejected a symlink to a directory instead of
unlinking the symlink. Its directory check followed by remove also left a replacement race.

The actual XNA 4.0 Storage assembly under Framework4/Wine passes12/12 filesystem
exception/preservation checks in cna-demos StorageOperationsOracle.cs. This is constructor-free
root injection, not platform selection or mounting validation. Independently, actual .NET8.0.31
on Linux passes7/7 in FileDeletionOracle.cs, including exact UnauthorizedAccessException for
a directory, DirectoryNotFoundException for a missing parent, missing-leaf success and
ArgumentException for NUL with the existing prefix preserved. Sources and logs are retained
in ../cna-demos/docs/repair-evidence/storage/ and build/linux-full/reports/repair-storage-*.
The local .NET11-preview FileSystem.Unix.cs additionally corroborates unlink, EISDIR-to-access
translation and symlink behavior; it is not presented as a .NET8 runtime measurement.

## Repair and compatibility

Use one file-only native operation: DeleteFileW on Windows and unlink on Unix/Emscripten.
There is no pre-check followed by directory-capable remove. Use the existing UTF-8 native-path
conversion, reject embedded NUL before native calls, preserve missing-leaf success and report
missing parents/access failures with the corresponding System exceptions. A Unix directory
symlink is removed without following or deleting its target.

No public signature, layout, vtable or noexcept change. The old plain-IOException exception
expectation for deleting a directory was incorrect; callers must handle UnauthorizedAccessException,
as in the reference. The old test is corrected with its preservation assertion retained,
not removed or generalized to any exception. This is a necessary lower-layer repair before
CNA delegates DeleteFile; other storage creation/enumeration and content exceptions remain open.

## Validation (2026-10-23)

- Five new native regressions before repair:1PASS/4FAIL; after repair:5PASS.
- First full IO after repair:748/749, only the incorrect older IOException oracle fails.
- Corrected reference-based oracle: full warning-free build and **18,223/18,223** tests
  across42 executables,zero failures/skips; IO749. Original Yacht SOAP fixture runs privately.
- Strict C++23 Wall/Wextra/Wpedantic/Werror syntax checks pass for MinGW Windows and Emscripten.
  Windows/browser product runtime is not claimed.
- Module boundaries44 physical modules/109 edges pass.
- Existing build/ and build-probe/ only, one compiler at a time, shared ccache, C++ builds
  under4GiB soft/6GiB hard/no-swap scope. No clean rebuild or temporary build tree.

Reports: cna-demos build/linux-full/reports/repair-storage-filedelete-*.

Follow-up: actual Windows-compiled File/FileStream code passes all16 existing regression tests
under Win64 Wine, including all4 Windows-applicable deletion tests. This is emulated product
execution, not native Windows or browser validation. Reproduction and the initial MinGW link
diagnostic are retained in [WindowsFileIoValidation.md](WindowsFileIoValidation.md).
