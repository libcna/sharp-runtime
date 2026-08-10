# Audit: `modules/core/include/System/Exception.hpp`

## Metadata

- Audit status: AUDITED (131-line public declaration, fully read).
- Validation: the complete `ExceptionTests.cpp` and `ExceptionNewTests.cpp`
  suite filter passed 124/124 in `SharpRuntimeTests_Core_Base` on 2026-07-26;
  their reports distinguish test-file coverage from remaining derived-type
  implementation audits.
- Reference: local .NET `System/Exception.cs` was reviewed; its `Message`
  property falls back to `Exception_WasThrown` when no explicit message exists.

## Assessment

The declaration clearly documents permanent C++ limitations around reflection,
stack capture, serialization, and base-exception cloning.  It exposes a stable
native `what()` bridge, HResult, source/help-link state, inner exception, and a
string-map Data adaptation.  Its default-message documentation, however,
declares an empty result rather than the .NET observable fallback.

## SR-AUD-092 — medium — default Exception exposes an empty message instead of the .NET fallback diagnostic

The default constructor documentation and implementation store an empty string;
therefore both `getMessageProperty()` and `what()` are empty.  Local .NET's
`Exception.Message` returns a formatted nonempty `Exception_WasThrown`
diagnostic whenever `_message` is null.  Reflection absence prevents deriving
an arbitrary native runtime class name, but it does not require a completely
blank base `System::Exception` message; the chosen silent fallback changes
logs and error diagnostics.

The direct `ExceptionTests.DefaultCtorEmptyMessage` and
`ExceptionNewTests.DefaultCtor_MessageEmpty` assertions explicitly preserve
the incompatible empty behavior.

## Other missing assertions and diagnostics

- The `Data` property narrows .NET's object-key/object-value dictionary to
  `map<string,string>` without a documented conversion/ownership policy; tests
  exercise one string pair only.
- No test verifies `what()` pointer stability across copy/move or source/help
  link mutation, HResult extrema, embedded-NUL/non-UTF-8 messages, or inner
  exception rethrow/identity.
- StackTrace's permanently empty adaptation is documented but has no runtime
  diagnostic reminding callers that a thrown exception will not capture one.
- `GetBaseException`, `ToString`, TargetSite, and serialization are excluded;
  no compile-time feature diagnostic directs users to these documented limits.

## Final assessment

The native storage bridge is coherent, but its default diagnostic is silently
empty where the .NET contract provides useful text.  No source or test was
modified during this audit.
