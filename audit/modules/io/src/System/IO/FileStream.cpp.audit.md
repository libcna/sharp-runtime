# Audit: `modules/io/src/System/IO/FileStream.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-342 — medium — FileStream does not enforce requested access or disposed state consistently

Read/Write capability flags are not checked by operations or property access.  A direct probe opens an existing file read-only, calls Write, and prints `write-to-readonly=accepted`; after Close it prints `can-read-after-close=1` and `length-after-close=1`.  A second probe uses `FileMode::OpenOrCreate` with `FileAccess::Read`; the implementation opens the missing file with output flags and its unchecked Write creates content, printing `created-with-read-only=1` and `read-only-write-content=y`.  This permits actual writes through a read-only public FileStream and leaves closed properties usable instead of producing the expected NotSupported/ObjectDisposed failures.

## Missing assertions and diagnostics

- Tests cover normal modes and some SetLength validation but omit Read-on-write-only, Write/WriteByte-on-read-only, OpenOrCreate+Read, Flush/Length/Position/CanRead/CanWrite after Close, and write-error propagation.
- No test verifies that the requested FileAccess rather than incidental `std::fstream` flags governs all operations.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
