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

## Remediation record — SR-AUD-344 (ticket #2108, 2026-08-04)

**Status: `confirmed` → `remediated`.** The original evidence above is retained verbatim.

**#2108 was split out of #2098 by measurement.** #2098 bundled SR-AUD-337, SR-AUD-343 and
SR-AUD-344 and was gated behind an object-layout decision. Measured
(`build-probe/2098_probe1_layout.log`), **`UnmanagedMemoryStream` needs no layout change at
all**: it already carries `bool isOpen_`, `Close()` already clears it, and
`getCanReadProperty`/`getCanWriteProperty`/`getCanSeekProperty` already consult it. The split
follows the existing finding boundary rather than inventing one, and this half needs no approval.

**Wider than the index summary.** The summary names `Length` and `Position`. Measured, **six**
members ignored the state — `getLengthProperty`, `getCapacityProperty`, `getPositionProperty`,
`setPositionProperty`, `Flush` and, sharpest of all, **`getPositionPointerProperty`, which handed
out a live raw pointer into the buffer of a closed stream**. A seventh, `SetLength`, did throw
but checked `value < 0` *before* `isOpen_` — the opposite order from `Read`/`Write` and from the
.NET rule this file's own transcribed note records. `Read`, `Write` and `WriteByte` were already
correct.

**Decidable without the reference tree.** `UnmanagedMemoryStream.cpp` already carried a
transcribed note recording that .NET's `EnsureNotClosed()` is checked **first** and throws
`ObjectDisposedException("Cannot access a closed Stream.")`. That is repository-contained
evidence, so no behaviour here rests on a guess.

**Repair:** one private `EnsureNotClosed()`, routed through by every member that depends on the
closed resource including the three that were already correct, so they cannot drift apart again.

**Validation:** `SharpRuntimeTests_IO` 626 → **635**, zero warnings. Three mutations each
discriminate — the guard neutered (**5** tests), `SetLength`'s original check order restored
(**1**), the raw-pointer getter left unguarded (**2**) — with a clean control and byte-identical
restores. ASan + UBSan + LSan clean over **22,000 rejections and 22,000 acceptances**, control
proven live, with the `.cpp` compiled into the instrumented translation unit.

**No object-layout, vtable, mangled-symbol, signature or `noexcept` change.** `sizeof` stays 40.
