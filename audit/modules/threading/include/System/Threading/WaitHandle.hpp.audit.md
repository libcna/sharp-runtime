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


---

## Remediation record — ticket #1952 (2026-08-03), SR-AUD-183 → `remediated`

Cause **T-C** of `docs/ThreadingNamespaceReviewPlan.md` ("public arguments are not validated
at the boundary"). All four static multi-wait entry points now validate in the order
`WaitHandle.cs`'s `WaitMultiple` uses:

1. **empty collection** → `System::ArgumentException("Waithandle array may not be empty.", "waitHandles")`;
2. **timeout** → the pre-existing `ValidateTimeout`, i.e.
   `ArgumentOutOfRangeException("millisecondsTimeout")` below `-1`;
3. **null element** → `System::ArgumentNullException("waitHandles[i]", "At least one element in the specified array was null.")`.

The C++ parameter was renamed `handles` → `waitHandles` so the signature, the doc-comments
and the exception messages agree on .NET's own name. A C++ function-parameter name
contributes nothing to the interface or to mangling, so this is source-compatible and
ABI-neutral — the same treatment ticket #1869 applied to `Array`'s `predicate` → `match`.

The two overloads that take no timeout run only checks 1 and 3, because .NET's
`WaitAll(WaitHandle[])` / `WaitAny(WaitHandle[])` delegate to the timed forms with
`Timeout.Infinite`, which cannot be invalid.

### Three corrections to the finding, all measured

Probe `build-probe/1952_probe1_waithandle_multiwait.cpp`, logs `1952_probe1_before.log` and
`1952_probe1_after.log`. The unbounded cases run in forked children under `alarm(2)`, so a
hang is reported rather than suffered.

1. **There are three non-terminating shapes, not one.** The finding names the no-timeout
   `WaitAny({})`. Measured: `WaitAny({})`, `WaitAny({}, -1)` **and** `WaitAny({nullptr})` all
   printed `child-hang`. The infinite-timeout branch is a second copy of the same loop, and a
   null-only collection reaches it too because null elements were skipped rather than
   rejected — leaving a non-empty collection with nothing to poll.
2. **The `-2` timeout produced two different wrong answers.** The finding records
   `WaitAll({}, -2) = true`. Measured: that holds for an *empty* collection, but
   `WaitAll({handle}, -2)` returned **`false`** — a spurious timeout — because
   `now() + milliseconds(-2)` is already in the past and the first `remaining.count() < 0`
   test short-circuits. Same argument, opposite result, decided by the collection's size.
3. **A mixed collection silently skipped its null and answered.**
   `WaitAny({signalledHandle, nullptr}, 0)` returned `0` before the change. This is the only
   pre-fix call in the finding's surface that produced a *usable* result, and it is therefore
   the sharpest observable change: it now throws `ArgumentNullException("waitHandles[1]")`,
   which is what .NET does. Pinned by
   `ThreadingMultiWaitValidationTests.MixedCollection_NoLongerSkipsTheNullSilently`.

### One deliberate non-repair

.NET's `MaxWaitHandles` ceiling (64, `NotSupportedException`) is **not** reproduced. It
exists because Win32 `WaitForMultipleObjects` accepts at most 64 handles; this port waits
sequentially and polls, so it has no such limit, and adopting the ceiling would reject input
that currently works and would keep working. This report's "Other missing assertions" section
notes the unrepresentable maximum as an *undocumented portability boundary*, not as a
required rejection; the boundary is now documented in the header instead. No `SR-AUD-*`
identifier is issued and no ticket is opened.

### Coverage

`modules/threading/tests/System/Threading/ThreadingBoundaryTests.cpp`,
`ThreadingMultiWaitValidationTests.*` — 7 cases covering every entry point against empty,
null-only, mixed, `-2`, `-1` and `0`, both validation-order boundaries, the three formerly
unbounded shapes (which hang the suite rather than fail quietly if they regress), and a
control asserting the documented sequential/polling behaviour for valid input is untouched.
No signature, object layout, vtable or exception-specification change; the module graph stays
41 / 91.
