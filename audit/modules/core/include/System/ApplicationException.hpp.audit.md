# Audit: `modules/core/include/System/ApplicationException.hpp`

## Metadata

- Audit status: AUDITED (26-line inline implementation, fully read).
- Validation: the complete five-type exception-family filter passed 43/43 on 2026-07-26.
- Reference/probe: local .NET `ApplicationException.cs` assigns
  `COR_E_APPLICATION` (`0x80131600`) in every constructor; the standalone
  `/tmp/sharp-runtimervc-exception-hresult-audit-probe` prints C++ value
  `80131500`.

## SR-AUD-094 — medium — five exception types retain their base HResult instead of their documented derived diagnostic code

`ApplicationException` never calls `setHResultProperty`, so all three overloads
retain `Exception`'s `COR_E_EXCEPTION` (`0x80131500`) rather than
`COR_E_APPLICATION` (`0x80131600`). The same omission occurs in sibling
`AppDomainUnloadedException`, `BadImageFormatException`,
`CannotUnloadAppDomainException`, and `DataMisalignedException`: each retains
the `SystemException` base `COR_E_SYSTEM` (`0x80131501`) rather than its
documented type-specific code. Local .NET source assigns the correct value in
every affected overload, and the direct probe prints the inherited C++ values.
All 43 existing focused tests pass because none asserts an HResult for these
types.

## Other missing assertions and diagnostics

- Tests do not assert the exact default message, HResult for any constructor,
  stored-inner identity/rethrow behavior, or empty/UTF-8 message boundaries.
- Current .NET advises application authors not to throw/catch this legacy base
  type for ordinary errors; its retained C++ API nonetheless advertises a .NET
  counterpart and should preserve the observable diagnostic code.
- No null C-string boundary exists because the message API requires
  `std::string`.

## Final assessment

The message and inheritance paths pass, but the public HResult is incompatible.
No source or test was modified.

---

## SR-AUD-094 — REMEDIATED (ticket #1874, 2026-07-30, CCF-016)

The original evidence above is retained unchanged.

All **17** public constructors of the five types now assign their documented
code, matching the reference line-for-line:

| Type | Ctors | Was | Now | Reference |
|---|---|---|---|---|
| `ApplicationException` | 3 | `0x80131500` (`COR_E_EXCEPTION`) | **`0x80131600`** | `ApplicationException.cs:28,38,44` |
| `AppDomainUnloadedException` | 3 | `0x80131501` (`COR_E_SYSTEM`) | **`0x80131014`** | `AppDomainUnloadedException.cs:17,23,29` |
| `BadImageFormatException` | 5 | `0x80131501` | **`0x8007000B`** | `BadImageFormatException.cs:25,31,37,42` |
| `CannotUnloadAppDomainException` | 3 | `0x80131501` | **`0x80131015`** | `CannotUnloadAppDomainException.cs:17,23,29` |
| `DataMisalignedException` | 3 | `0x80131501` | **`0x80131541`** | `DataMisalignedException.cs:20,26,32` |

**Extension of the finding's premise (measured).** The finding says "all 43
existing focused tests pass because none asserts an HResult for these types" —
confirmed, and the port has **five** `BadImageFormatException` constructors
against the reference's four, because it adds a `(message, fileName, inner)`
overload. Both file-name overloads are covered. The root cause is structural
rather than per-type: `Exception` initialises `hResult_` to `COR_E_EXCEPTION` and
`SystemException`'s body overwrites it with `COR_E_SYSTEM`, so a derived type
written as a pure forwarding constructor inherits whatever its nearest base last
wrote — the inherited value is not a decision but the absence of one.

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
