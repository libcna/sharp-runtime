# Audit: `modules/io/src/System/IO/StreamWriter.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-337 — medium — StreamReader/StreamWriter Close with leaveOpen keeps the wrapper usable

See the paired StreamReader report for the shared direct probe.  `Close()` conditionally closes only the base stream and never marks this writer disposed, so `leaveOpen=true` leaves subsequent writes accepted rather than rejecting use of the closed wrapper.

## SR-AUD-338 — high — StreamWriter accepts a null base stream and dereferences it during Write

The constructor stores `nullptr` without validation.  The ASan/UBSan probe crashes in `StreamWriter::WriteRaw` after `StreamWriter(nullptr, true).Write("x")`; this is an externally reachable null dereference rather than a managed-style construction error.

## Missing assertions and diagnostics

- No post-Close writer test distinguishes wrapper disposal from keeping a base stream open.
- Null StreamWriter construction/write has no regression or diagnostic assertion.
- Flush, destructor, and text encoding failure paths are not checked with a throwing or buffered underlying stream.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
