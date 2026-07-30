# Audit: `modules/core/include/System/ArrayTypeMismatchException.hpp`

## Metadata

- Audit status: AUDITED (55-line inline implementation, fully read).
- Validation: its six shared fixture cases passed within 124/124 on 2026-07-26.
- Reference/probe: local .NET `ArrayTypeMismatchException.cs` assigns
  `COR_E_ARRAYTYPEMISMATCH`; `/tmp/sharp-runtimervc-arraytypemismatch-audit-probe`
  prints the C++ default `80131501`.

## SR-AUD-093 — medium — ArrayTypeMismatchException inherits SystemException HResult instead of its own error code

None of the inline constructors calls `setHResultProperty`, so every instance
keeps `COR_E_SYSTEM` (`0x80131501`).  Current .NET assigns
`COR_E_ARRAYTYPEMISMATCH` (`0x80131503`) in each constructor.  The direct probe
confirms the incorrect C++ value; existing message/inheritance tests omit it.

## Other missing assertions and diagnostics

- Tests omit all constructor HResults, null C-string, inner identity, exact
  resource text, and actual array-store integration.
- C++ arrays are statically typed, so no public store path documents when this
  exception is meaningful versus a compile error or native cast failure.

## Final assessment

Message/inheritance paths pass, but the public diagnostic code is wrong. No source or test was modified.

---

## SR-AUD-093 — REMEDIATED (ticket #1874, 2026-07-30, CCF-016)

The original evidence above is retained unchanged.

All **four** public constructors of `ArrayTypeMismatchException` — default,
`const std::string&`, `const char*`, and `(message, inner)` — now call
`setHResultProperty(0x80131503)` (`COR_E_ARRAYTYPEMISMATCH`), matching
`ArrayTypeMismatchException.cs:23,33,39`. Measured before:
`0x80131501` (`COR_E_SYSTEM`, inherited from `SystemException`) from every
constructor; measured after: `0x80131503` from every constructor
(`build-probe/1873_prefix.log` / `build-probe/1874_postfix.log`).

The finding named three .NET constructors; the port has four, because it adds a
`const char*` overload the reference does not need. All four are covered.

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
