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
