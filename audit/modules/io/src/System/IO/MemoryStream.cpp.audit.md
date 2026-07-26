# Audit: `modules/io/src/System/IO/MemoryStream.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-341 — high — MemoryStream raw-buffer constructor dereferences null input before it can report an argument error

`MemoryStream(const bytecs* buffer, intcs size, ...)` initializes the vector with `buffer, buffer + size` before validating either argument.  The ASan/UBSan direct probe `MemoryStream(nullptr, 1)` terminates with a null read in the vector range copy.  This bypasses the managed-style `ArgumentNullException` that the public raw-buffer constructor must provide and differs from the class's otherwise explicit Read/Write argument validation.

## Missing assertions and diagnostics

- Stream tests cover Read/Write invalid buffers but omit constructor null source, negative size, and source-lifetime/copy assertions (SR-AUD-341).
- No near-limit capacity/position diagnostic verifies allocation failures or raw vector-range preconditions.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
