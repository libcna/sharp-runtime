# Audit: `modules/core/include/System/DuplicateWaitObjectException.hpp`

## Metadata

- Audit status: AUDITED (45-line inline implementation, fully read).
- Validation: the focused exception-family filter passed 38/38 on 2026-07-26.
- Reference/probe: local .NET `DuplicateWaitObjectException.cs` assigns `COR_E_DUPLICATEWAITOBJECT` (`0x80131529`); the shared HResult probe prints C++ `80070057`.

## SR-AUD-100 — medium — DuplicateWaitObjectException inherits ArgumentException HResult and uses a divergent default diagnostic

None of the four constructors sets `COR_E_DUPLICATEWAITOBJECT`, so instances
retain `ArgumentException`'s `COR_E_ARGUMENT` (`0x80070057`) instead of
`0x80131529`. The default C++ text, `Duplicate objects in argument.`, also does
not match the current .NET diagnostic identifying duplicate objects in the wait
array. Local .NET source assigns the derived HResult in every overload and the
probe reproduces the inherited C++ code. The three current tests check only
non-empty/custom text and broad inheritance.

## Other missing assertions and diagnostics

- Tests omit every overload HResult, the default exact diagnostic, parameter-name suffix format, stored-inner identity/rethrow, and empty/UTF-8 parameter/message paths.
- No reviewed wait-handle/multi-wait implementation constructs this type for duplicate input, so the exception is only manually exercised.

## Final assessment

Constructor diagnostic compatibility is incomplete. No source or test was modified.

---

## SR-AUD-100 — REMEDIATED, with one half corrected as a false positive (ticket #1874, 2026-07-30, CCF-016)

The original evidence above is retained unchanged.

**The HResult half is real and is fixed.** All **four** public constructors —
default, `(parameterName)`, `(parameterName, message)` and `(message, inner)` —
now assign `0x80131529` (`COR_E_DUPLICATEWAITOBJECT`), matching
`DuplicateWaitObjectException.cs:22,28,34,40`. Measured before: every constructor
reported `0x80070057` (`COR_E_ARGUMENT`, inherited from `ArgumentException`),
exactly as the finding states; measured after: every constructor reports
`0x80131529`, including when caught through a `const ArgumentException&`
reference.

**Correction — the message half is a false positive (measured 2026-07-30).** The
finding states that "the default C++ text, `Duplicate objects in argument.`, also
does not match the current .NET diagnostic identifying duplicate objects in the
wait array." It does match. `SR.Arg_DuplicateWaitObjectException` is
`<value>Duplicate objects in argument.</value>`
(`System.Private.CoreLib/src/Resources/Strings.resx:319-321`) — **byte-identical**
to the port's text, which the probe prints verbatim as
`DuplicateWaitObjectException.defaultMessage=[Duplicate objects in argument.]`.
**No message change was made**, and a permanent test now pins the text verbatim so
a future reader acting on the finding's wording cannot introduce a divergence.

The finding is marked `remediated` on the strength of its real half rather than
split, and this Correction is appended rather than editing the historical text —
the conservative convention this repository uses (cf. SR-AUD-081, SR-AUD-362) when
a finding is partly wrong and no false-positive status exists.

The parameter-name behaviour is unchanged and pinned: `(parameterName)` still
appends `(Parameter 'x')` exactly once, guarding the #1776 duplicate-suffix
regression.

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
