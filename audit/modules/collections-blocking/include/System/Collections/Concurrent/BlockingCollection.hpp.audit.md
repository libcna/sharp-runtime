# Audit: `modules/collections-blocking/include/System/Collections/Concurrent/BlockingCollection.hpp`

## Metadata

- Audit status: AUDITED (604 lines, full read).
- Subsystem: concurrent blocking collections.
- Reference reviewed: local dotnet/runtime `BlockingCollection.cs`, especially
  `ValidateTimeout` and `ValidateCollectionsArray`.
- Validation: direct selective component consumer and all eight owned tests
  passed during this audit.

## Purpose

Provides the C++ counterpart to `BlockingCollection<T>`: bounded/unbounded
producer-consumer operations, cancellation, consuming enumeration, and
multi-collection `*ToAny` operations.  The C++ implementation uses condition
variables and documents one-millisecond polling for the `*ToAny` paths because
C++ has no direct `WaitAny` equivalent.

## Assessment

The ownership model, disposal wakeups, `CompleteAdding` signaling, snapshot
enumeration, cancellation registration lifetime, and public component
dependencies are explicit and internally coherent.  The primary blocking
single-collection paths use predicates that include cancellation, disposal,
capacity, and completion state, avoiding ordinary lost-wakeup behavior.

## Findings

### SR-AUD-003 — low — fractional negative `TimeSpan` timeouts diverge from .NET validation

`validateTimeout` treats every `TimeSpan` whose raw ticks are less than
`Timeout::InfiniteTimeSpan` as invalid (lines 108–116).  The .NET reference
first truncates `timeout.TotalMilliseconds` to an integer and then accepts
the resulting `-1` as `Timeout.Infinite` (`BlockingCollection.cs` lines
1697–1704).  Therefore a representable duration between -2 ms and -1 ms,
such as -1.5 ms, is accepted as infinite by the reference but throws
`ArgumentOutOfRangeException` here before the C++ cast can produce `-1`.

**Impact:** every C++ `TimeSpan` overload for `TryAdd`, `TryTake`,
`TryAddToAny`, and `TryTakeFromAny` has a narrow but observable parity gap.
The owned tests cover only positive `TimeSpan` values.

**Follow-up evidence needed:** a focused regression should construct a
negative fractional `TimeSpan`, verify the intended C++ policy against the
reference's truncation behavior, and apply the same conversion rule to all
four overload families.

## Open parity note

The reference rejects arrays longer than 63 because its `WaitHandle.WaitAny`
implementation reserves a handle; this C++ port deliberately polls vectors and
does not impose that ceiling.  The behavior is more permissive, but the header
does not call out the difference.  Treat it as an intentional-adaptation
candidate to document during remediation rather than a confirmed defect until
the project decides whether strict exception parity is required.

## Positive findings

The direct-mutation restriction is documented at the public type, and the
implementation refuses to move/copy synchronization state.  The direct
consumer check demonstrates that this header remains independently usable
without the compatibility umbrella.

## Final assessment

One low confirmed timeout-parity issue (SR-AUD-003); the remaining core
single-collection control flow is well structured, but `*ToAny` behavior needs
broader edge-case coverage.
