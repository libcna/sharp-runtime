# Audit: `modules/core/include/System/DllNotFoundException.hpp`

## Metadata

- Audit status: AUDITED (25-line inline implementation, fully read).
- Validation: the complete type-load exception filter passed 48/48 on 2026-07-26.
- Reference/probe: local .NET `DllNotFoundException.cs` assigns `COR_E_DLLNOTFOUND` (`0x80131524`) in all constructors; the shared `/tmp/sharp-runtimervc-exception-hresult-audit-probe` prints C++ `80131522`.

## SR-AUD-095 — medium — DllNotFoundException and EntryPointNotFoundException retain the TypeLoad HResult

Neither constructor set in this header calls `setHResultProperty`, so all
instances retain `TypeLoadException`'s `COR_E_TYPELOAD` (`0x80131522`) instead
of `COR_E_DLLNOTFOUND` (`0x80131524`). `EntryPointNotFoundException` repeats
the same omission and inherited value rather than its documented
`COR_E_ENTRYPOINTNOTFOUND` (`0x80131523`). Current .NET source assigns each
derived code in every overload; the shared probe prints `80131522` for both
C++ types. The 48 passing family tests never assert either derived HResult.

## Other missing assertions and diagnostics

- Existing shared tests only check non-empty/default or supplied message and broad `Exception` inheritance. They omit the TypeLoad base relationship, every HResult, inner-pointer behavior, exact diagnostic text, and UTF-8 message boundaries.
- No reviewed P/Invoke or native-library resolver maps a missing dynamic library into this type. That missing integration is a separate adaptation boundary, not a reason to retain the wrong constructor code.

## Final assessment

The public diagnostic HResult is wrong despite normal message tests passing. No source or test was modified.

---

## SR-AUD-095 — REMEDIATED (ticket #1874, 2026-07-30, CCF-016)

The original evidence above is retained unchanged.

All **six** public constructors of the two `TypeLoadException` derivatives now
assign their own code: `DllNotFoundException` → `0x80131524`
(`COR_E_DLLNOTFOUND`, `DllNotFoundException.cs:20,26,32`) and
`EntryPointNotFoundException` → `0x80131523` (`COR_E_ENTRYPOINTNOTFOUND`,
`EntryPointNotFoundException.cs:20,26,32`). Measured before: both reported
`0x80131522` (`COR_E_TYPELOAD`) from every constructor, exactly as the finding
states; measured after: each reports its own value, and a permanent test catches
a `DllNotFoundException` through a `const TypeLoadException&` reference to show
the value survives the base-reference catch that made the two
indistinguishable.

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
