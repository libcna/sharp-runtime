# Audit: `modules/threading/include/System/Threading/WaitHandle.hpp`

## Metadata

- AUDITED: 136-line public wait-handle base and static multi-wait adapter,
  fully read.
- Validation: `EventWaitHandleTests.*:WaitHandleTests.*` passed 9/9 in
  `SharpRuntimeTests_Threading` on 2026-07-27.  The relevant large Threading
  fixture sources are pending complete per-file audits.
- Reference/probe: local current-.NET `WaitHandle.cs`; C++/managed probes
  compare empty/null handle vectors and invalid timeout input.

## SR-AUD-183 — medium — WaitAll and WaitAny silently accept invalid handle collections and invalid timeout values

The vector APIs skip null pointers and have no empty-collection or timeout
validation.  `WaitAll({}, -2)` returns `true`; `WaitAny({}, 0)` and
`WaitAny({nullptr}, 0)` return `WaitTimeout` (258).  The no-timeout
`WaitAny({})` loop has no handle to poll and never terminates.  Current .NET
rejects all three probed inputs with `ArgumentException`/derived argument
validation before waiting.

The C++ probe prints `wait_any_empty_0=258 wait_all_empty_minus2=normal
wait_any_null_0=258`; the managed probe prints `argument` for each.  This is
not the documented sequential/polling implementation adaptation: invalid
public input reaches a false success, a timeout result, or an unbounded loop
instead of a deterministic boundary diagnostic.

## Assessment

The class-level timeout validator is correct and EventWaitHandle uses it for
`WaitOne`.  For ordinary nonempty valid collections, the explicit sequential
WaitAll and polling WaitAny implementation is a documented native
simplification.  It cannot provide atomic OS multiple-wait/abandoned-mutex
semantics, but that limitation is stated rather than separately classified.

## Other missing assertions and diagnostics

- Existing tests cover only nonempty valid Semaphore collections and one
  infinite timeout.  They omit empty/null lists, `-2`, zero/positive boundary
  behavior, duplicate handles, destroyed handles, and all error taxonomy.
- No timing test distinguishes the documented sequential WaitAll/polling
  WaitAny adaptation from an atomic multiple-wait result under concurrent
  signal/reset.
- The local vector API does not make the .NET maximum handle count or native
  wait-handle ownership representable; no diagnostic explains that public
  portability boundary.

## Final assessment

SR-AUD-183 is confirmed by C++/managed probes.  No source or test was
modified.
