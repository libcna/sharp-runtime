# Audit: `modules/io/src/System/IO/RandomAccess.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-340 — medium — RandomAccess raw overloads silently accept invalid metadata and POSIX GetLength suppresses failures

The raw pointer/count overloads do not validate pointer, count, or offset.  In particular, the Write loop is skipped for a negative count.  A direct fd probe prints `negative-write=accepted`.  The same probe calls `GetLength(-1)` and prints `invalid-fd-length=-1`: both `lseek` failures are returned as a spurious length instead of becoming `IOException`.  The Read path casts a negative count to `size_t`, and neither platform branch detects zero-byte write progress, so the public contract has inconsistent error behavior.

## Missing assertions and diagnostics

- Existing tests exercise valid regular-file offsets, lengths, and flushes only; they omit invalid descriptors, null buffers, negative counts/offsets/lengths, read-only descriptors, non-seekable handles, and write-zero-progress behavior.
- No test asserts that an OS failure maps to a System I/O exception with the operation/path/errno context needed for diagnosis.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
