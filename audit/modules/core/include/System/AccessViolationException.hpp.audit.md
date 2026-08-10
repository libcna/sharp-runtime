# Audit: `modules/core/include/System/AccessViolationException.hpp`

## Metadata

- Audit status: AUDITED (26-line inline implementation, fully read).
- Validation: the focused protection/context/program exception filter passed 32/32 on 2026-07-26.
- Reference/probe: local .NET `AccessViolationException.cs` sets `E_POINTER` (`0x80004003`) for all constructors; `/tmp/sharp-runtimervc-exception-hresult-audit-probe` prints C++ `80131501`.

## SR-AUD-096 — medium — AccessViolationException and ContextMarshalException omit their distinct HResults

This header delegates every constructor to `SystemException` but never calls
`setHResultProperty`, so the public value remains `COR_E_SYSTEM` (`0x80131501`)
rather than .NET's `E_POINTER` (`0x80004003`).
`ContextMarshalException` has the same omission and retains that base code
instead of `COR_E_CONTEXTMARSHAL` (`0x80131504`). Local .NET sources assign
their derived value in every overload, while the shared C++ probe prints the
inherited value for both types. The 32 passing focused tests omit their HResult.

## Other missing assertions and diagnostics

- Tests cover ordinary messages and base inheritance, but omit every HResult, exact default text, stored-inner identity/rethrow, and empty/UTF-8 messages.
- No reviewed managed/native memory-protection boundary turns a native fault into this exception. It remains constructible as a diagnostic type, but the missing integration does not justify the wrong code.

## Final assessment

The normal message path passes, but the observable HResult is incompatible. No source or test was modified.

---

## SR-AUD-096 — REMEDIATED (ticket #1874, 2026-07-30, CCF-016)

The original evidence above is retained unchanged.

All **seven** public constructors of the two types now assign their own code:
`AccessViolationException` → `0x80004003` (`E_POINTER`,
`AccessViolationException.cs:20,26,32`) and `ContextMarshalException` →
`0x80131504` (`COR_E_CONTEXTMARSHAL`, `Context.cs:27-30`). Measured before: both
reported `0x80131501` (`COR_E_SYSTEM`); measured after: each reports its own.

**One reference detail worth recording.** `ContextMarshalException` is the single
type in this family where .NET uses constructor *chaining* — its no-arg and
one-arg forms delegate to `(message, inner)`, which is the sole assignment point.
The port's four constructors each forward to `SystemException` directly, so each
gains the assignment directly; the observable result is identical, and the
repository's per-constructor idiom (used by ~30 already-correct siblings) is kept
rather than introducing a second shape for one class.

Compatibility: **none** beyond the observable value. No signature, `noexcept`
specification, virtual function, vtable slot or data member changed; every
affected type stays an empty derived class and the assignment is a constructor
*body* statement, which in C++ always runs after the base subobject is fully
constructed and therefore always wins. Correcting a wrong constant to its
documented reference value is required by this project's own porting checklist
(CLAUDE.md item 5: "Verify that default messages, constants, and HResult/error
codes match the .NET source where applicable"), so it is remediation under a
standing rule rather than a discretionary behaviour change.

Closure evidence: 18 new permanent regressions in `ExceptionRemainingTests.cpp`
(`DerivedExceptionHResultTests`) — one exact-hexadecimal assertion per public
constructor of all eleven types, plus custom-message preservation, parameter-name
preservation, catch-through-base-reference, copy and copy-assign preservation, the
unchanged base codes (`Exception`, `SystemException`, `TypeLoadException`,
`ArgumentException`) and the `AggregateException` control.
`DerivedExceptionHResultTests` 18/18, `SharpRuntimeTests_Core_Base` 5,319/5,319,
whole-repository build clean with zero errors and zero warnings.
**Mutation-checked:** deleting one of `DllNotFoundException`'s three assignments
fails two permanent tests. The probe `build-probe/1873_hresult_probe.cpp` reports
`wrong=0` after the change (`build-probe/1874_postfix.log`), with the three base
values and all three controls still `OK`.

Sanitizers are **not applicable** and were not run: the change adds one integer
store per constructor body, with no allocation, ownership transfer, pointer
arithmetic, lifetime change, shared state or new member. Recorded as a judgement,
not an omission.

The plan for this family is `docs/DerivedExceptionHResultPlan.md` (ticket #1873).
