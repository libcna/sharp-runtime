# Audit: `modules/runtime/include/System/Runtime/ExceptionServices/ExceptionDispatchInfo.hpp`

## Metadata

- AUDITED: 48-line inline dispatch wrapper, fully read.
- Validation: `ExceptionDispatchInfoTests.*` passed 4/4 on 2026-07-27.
- Reference/probe: local `ExceptionDispatchInfo.cs`, matching C# null probe,
  and `/tmp/sharp-runtimervc-exception-dispatch-null-probe`.

## SR-AUD-155 — medium — ExceptionDispatchInfo accepts null exception_ptr instead of rejecting a missing source at Capture/Throw entry

Current .NET `Capture(null)` and static `Throw(null)` reject the source with
ArgumentNullException. C++ stores a null `std::exception_ptr` unchanged; the
matching C# probe prints ArgumentNullException while C++ accepts it. A later
`std::rethrow_exception(nullptr)` has no defined public error contract, so the
failure is deferred beyond the public boundary rather than diagnosed at Capture
or static Throw entry.

## Other missing assertions and diagnostics

- The C++ SourceException is an `exception_ptr`, not a managed Exception object,
  so unthrown-instance capture and object diagnostics cannot be represented.
- Current .NET SetCurrentStackTrace and SetRemoteStackTrace are explicitly
  absent; the header documents the broader Exception stack-trace design change
  required for a future adaptation.
- Tests omit null capture/static throw, independent capture instances, nested
  exception types, and post-capture state/stack diagnostics.

## Final assessment

Normal captured rethrow including a cross-thread pointer works, but null-source
validation is missing. No source or test was modified during this audit.

## Post-audit correction — the `System::Runtime` namespace review, ticket #1972 (2026-08-03)

SR-AUD-155 remains **confirmed** and its historical text above is unchanged. One
correction to its **extent**, measured on 2026-08-03
(`build-probe/1972_probe1_runtime_boundaries.cpp`, `build-probe/1972_probe1_before.log`):

The finding names two routes to the undefined `std::rethrow_exception(nullptr)` —
`Capture(null)` and the static `Throw(null)`. Both reproduce as `SIGSEGV`
(`capture_null=died_signal_11`, `static_throw_null=died_signal_11`). There are **two
further routes the finding does not name**, and neither passes a null anywhere:

```
moved_from_source_is_null=1     moved_from_throw=died_signal_11
move_assigned_source_is_null=1  move_assign_throw=died_signal_11
```

`ExceptionDispatchInfo` holds a `std::exception_ptr` by value and declares no move
operations, so the **implicitly declared** move constructor and move assignment leave
the moved-from object's `exception_` null through ordinary well-formed C++. The
instance `Throw()` therefore needs its own guard: a check placed only at the two
named entries — which is what the finding literally asks for — leaves both routes
open.

This is the same shape as `docs/ThreadingNamespaceReviewPlan.md` §18.5's correction
to SR-AUD-199 (a moved-from `CancellationToken` reaching an identical crash the
finding never named). Owned by cause **R-E** and ticket **#1973** in
`docs/SystemRuntimeNamespaceReviewPlan.md`. **No new `SR-AUD-*` identifier was
issued**; numbering stays frozen at **364**.
