# Audit: `modules/io/src/System/IO/UnmanagedMemoryStream.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-344 — medium — UnmanagedMemoryStream exposes length and mutable position after Close

Although Read/Write check `isOpen_`, length and position access do not.  The direct disposal probe closes a two-byte stream and prints `unmanaged-length-after-close=2` followed by `unmanaged-set-position-after-close=accepted`.  Current .NET’s stream metadata/position surface observes disposal consistently; this split can let callers mutate state after Close and get stale liveness signals.

## Missing assertions and diagnostics

- Existing tests exercise read/write disposal but omit Length, Position, PositionPointer, Seek, and SetPosition after Close.
- Tests also omit position beyond capacity followed by PositionPointer, capacity-limit diagnostics, and pointer lifetime/alignment coverage.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
