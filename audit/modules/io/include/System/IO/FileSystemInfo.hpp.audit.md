# Audit: `modules/io/include/System/IO/FileSystemInfo.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-347 — medium — FileInfo/DirectoryInfo empty paths leak std::filesystem_error instead of System argument validation

The base constructor immediately calls `std::filesystem::absolute(path)` without a System.IO input check.  The direct probe constructs both FileInfo and DirectoryInfo with an empty path; each throws the raw message `filesystem error: cannot make absolute path: Invalid argument []` rather than the expected System argument exception.  This escapes the project’s exception hierarchy at a public API boundary.

## Missing assertions and diagnostics

- FileInfo/DirectoryInfo construction tests use valid nonempty paths only; no test asserts empty, embedded-NUL, malformed, or permission-denied exception taxonomy.
- Error paths should preserve a System exception and parameter context rather than a platform-library exception string.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
