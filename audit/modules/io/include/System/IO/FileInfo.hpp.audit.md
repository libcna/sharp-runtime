# Audit: `modules/io/include/System/IO/FileInfo.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-345 — medium — FileInfo.Delete can remove an empty directory

`FileInfo::Delete` delegates directly to `std::filesystem::remove`, which removes an empty directory.  A direct probe constructs FileInfo for an empty directory, calls Delete, and prints `file-info-delete-directory=accepted` and `directory-still-exists=0`.  The static File.Delete implementation correctly prevents this, so the instance API is inconsistent and allows an unintended destructive operation.

## Missing assertions and diagnostics

- FileInfo tests cover absent files and normal copy/move paths, but omit FileInfo constructed over a directory, Delete on a directory, and special-file/symlink behavior.
- The test suite does not compare static File and FileInfo error taxonomy on identical paths.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
