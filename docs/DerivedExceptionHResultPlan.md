<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Derived-exception HResult contract — CCF-016 plan

*Authored 2026-07-30 by the autonomous remediation batch on branch
`feature/remediation-batch-ccf014-ccf016`, immediately after CCF-014 closed
(#1871/#1872). This is the durable, evidence-based plan for **CCF-016 — "inline
exception constructors need a complete derived-HResult audit"**
(`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-016). Five findings, all
`confirmed`: SR-AUD-093, SR-AUD-094, SR-AUD-095, SR-AUD-096, SR-AUD-100.*

*Every current-behaviour statement was **measured** by
`build-probe/1873_hresult_probe.cpp` compiled against the shipped headers on
2026-07-30; raw output in `build-probe/1873_prefix.log`. Every required value was
read from `/rv/tmp/runtime/src/libraries/Common/src/System/HResults.cs` and each
type's own `.cs` file, not from memory. **All five findings still reproduce.***

**This document creates no `SR-AUD-*` identifier** (numbering frozen at 364) and
**marks no finding remediated**.

---

## 1. Exact family scope

Eleven public exception types whose constructors never call
`setHResultProperty`, so every instance reports the *base's* code instead of the
type's own. Measured: **38 wrong results across 40 public constructors** (the two
`BadImageFormatException` file-name overloads are counted among the 38; no
constructor of any of the eleven is correct today).

**Five findings ≠ five classes.** The audit's own summary lists five findings but
names eleven types: SR-AUD-094 alone covers five. §3 gives the exact mapping.

---

## 2. Complete inventory

| # | Type | Base | Ctors | Today | Required | .NET constant | Finding |
|---|---|---|---|---|---|---|---|
| 1 | `ArrayTypeMismatchException` | `SystemException` | 4 | `0x80131501` | **`0x80131503`** | `COR_E_ARRAYTYPEMISMATCH` | SR-AUD-093 |
| 2 | `ApplicationException` | `Exception` | 3 | `0x80131500` | **`0x80131600`** | `COR_E_APPLICATION` | SR-AUD-094 |
| 3 | `AppDomainUnloadedException` | `SystemException` | 3 | `0x80131501` | **`0x80131014`** | `COR_E_APPDOMAINUNLOADED` | SR-AUD-094 |
| 4 | `BadImageFormatException` | `SystemException` | 5 | `0x80131501` | **`0x8007000B`** | `COR_E_BADIMAGEFORMAT` | SR-AUD-094 |
| 5 | `CannotUnloadAppDomainException` | `SystemException` | 3 | `0x80131501` | **`0x80131015`** | `COR_E_CANNOTUNLOADAPPDOMAIN` | SR-AUD-094 |
| 6 | `DataMisalignedException` | `SystemException` | 3 | `0x80131501` | **`0x80131541`** | `COR_E_DATAMISALIGNED` | SR-AUD-094 |
| 7 | `DllNotFoundException` | `TypeLoadException` | 3 | `0x80131522` | **`0x80131524`** | `COR_E_DLLNOTFOUND` | SR-AUD-095 |
| 8 | `EntryPointNotFoundException` | `TypeLoadException` | 3 | `0x80131522` | **`0x80131523`** | `COR_E_ENTRYPOINTNOTFOUND` | SR-AUD-095 |
| 9 | `AccessViolationException` | `SystemException` | 3 | `0x80131501` | **`0x80004003`** | `E_POINTER` | SR-AUD-096 |
| 10 | `ContextMarshalException` | `SystemException` | 4 | `0x80131501` | **`0x80131504`** | `COR_E_CONTEXTMARSHAL` | SR-AUD-096 |
| 11 | `DuplicateWaitObjectException` | `ArgumentException` | 4 | `0x80070057` | **`0x80131529`** | `COR_E_DUPLICATEWAITOBJECT` | SR-AUD-100 |

Constructor forms per type: default; `const std::string&`; `const char*` (types
1 and 10 only); `(message, inner)`; plus `BadImageFormatException`'s
`(message, fileName)` and `(message, fileName, inner)` and
`DuplicateWaitObjectException`'s `(parameterName)` and
`(parameterName, message)`.

Every affected header is header-only and inline; three of them
(`ArrayTypeMismatchException`, `BadImageFormatException`,
`ContextMarshalException`, `DuplicateWaitObjectException`) carry Doxygen blocks
per constructor, the rest are one-liners.

Owning tests: `ExceptionRemainingTests.cpp` (via `EXCEPT_SIMPLE` macros),
`ArrayTypeMismatchExceptionTests.cpp`, `ContextMarshalExceptionTests.cpp`,
plus the direct fixtures named in each finding. Test target:
`SharpRuntimeTests_Core_Base`.

---

## 3. Findings → types

| Finding | Types | Count |
|---|---|---|
| SR-AUD-093 | `ArrayTypeMismatchException` | 1 |
| SR-AUD-094 | `ApplicationException`, `AppDomainUnloadedException`, `BadImageFormatException`, `CannotUnloadAppDomainException`, `DataMisalignedException` | 5 |
| SR-AUD-095 | `DllNotFoundException`, `EntryPointNotFoundException` | 2 |
| SR-AUD-096 | `AccessViolationException`, `ContextMarshalException` | 2 |
| SR-AUD-100 | `DuplicateWaitObjectException` | 1 |

---

## 4. Reference behaviour

Read from the current local sources:

- **Which constructors set it?** *All public ones*, in every affected type.
  `ArrayTypeMismatchException.cs:23,33,39`; `ApplicationException.cs:28,38,44`;
  `AppDomainUnloadedException.cs:17,23,29`;
  `BadImageFormatException.cs:25,31,37,42`;
  `CannotUnloadAppDomainException.cs:17,23,29`;
  `DataMisalignedException.cs:20,26,32`; `DllNotFoundException.cs:20,26,32`;
  `EntryPointNotFoundException.cs:20,26,32`;
  `AccessViolationException.cs:20,26,32`;
  `DuplicateWaitObjectException.cs:22,28,34,40`.
- **Common initializer?** Only `ContextMarshalException` (`Context.cs:17-30`)
  uses C# constructor chaining — its no-arg and one-arg forms delegate to
  `(message, inner)`, which is the single assignment point. Every other type
  repeats the assignment per constructor. .NET is therefore *mixed*, and this
  port's dominant per-constructor idiom (used by ~30 already-correct types such
  as `FieldAccessException` and `InsufficientExecutionStackException`) matches
  the reference's majority form.
- **Values.** All eleven read from
  `Common/src/System/HResults.cs` lines 32-123 (table in §2).
- **Copy construction.** .NET exceptions are reference types; the C++ port
  copies the `hResult_` member with the implicitly declared copy constructor, so
  a copy preserves the value. Nothing to do, but §8 pins it.
- **Serialization / metadata hooks.** The reference's
  `protected T(SerializationInfo, StreamingContext)` constructors are
  `[Obsolete]` and out of scope for this port by the standing serialization
  deviation (CLAUDE.md "Parity philosophy"). They set no HResult anyway.
- **Message and parameter-name behaviour.** Unchanged by this family — with one
  correction, §6.

---

## 5. Common root cause

`Exception` initialises `hResult_` to `COR_E_EXCEPTION` and `SystemException`'s
constructor body overwrites it with `COR_E_SYSTEM`. Because a C++ base
constructor runs *before* the derived body, a derived class that adds no body
statement silently inherits whatever its nearest base last wrote. Eleven inline
types were written as pure forwarding constructors — `: SystemException(message)
{}` — so the inherited value is not a decision, it is the absence of one.

The defect is invisible to the existing suites because they assert message text
and `catch`-ability but never the code: measured, **43 focused tests for the
SR-AUD-094 group and 48 for the SR-AUD-095 group all pass today** with the wrong
value. That is the "repeatable constructor-audit and assertion gap" CCF-016
names.

---

## 6. Premise corrections (measured)

**6.1 — SR-AUD-100's message claim is a false positive.** The finding states
that "the default C++ text, `Duplicate objects in argument.`, also does not match
the current .NET diagnostic identifying duplicate objects in the wait array."
Measured, .NET's `SR.Arg_DuplicateWaitObjectException` is
`<value>Duplicate objects in argument.</value>`
(`System.Private.CoreLib/src/Resources/Strings.resx:319-321`) — **byte-identical
to the port's text**, which the probe prints verbatim as
`DuplicateWaitObjectException.defaultMessage=[Duplicate objects in argument.]`.
The finding's HResult half is real and is remediated; the message half is not a
defect and **no message change is made**. SR-AUD-100 stays a single finding and
is marked `remediated` on the strength of its real half, with this correction
appended to its report — the conservative convention this repository uses when a
finding is partly wrong and no false-positive status exists.

**6.2 — `AggregateException` is *not* a twelfth member.** A sweep of all 50
`modules/core/include/System/*Exception.hpp` headers found exactly twelve with no
`setHResultProperty` anywhere: the eleven above plus `AggregateException`.
Measured, `.NET`'s `AggregateException.cs` contains **no** `HResult` assignment
at all, so inheriting `COR_E_EXCEPTION` is correct parity. The probe pins it as a
control (`control.AggregateException.default=0x80131500 OK`). It is deliberately
excluded.

**6.3 — the affected-type count is eleven, not five.** The audit's index rows
read as five findings; SR-AUD-094 alone spans five types and
`BadImageFormatException` has five constructors. The measured surface is 11 types
/ 40 constructors / 38 wrong results.

**6.4 — a much larger population exists outside the audited set, and is not
absorbed.** The same sweep across the other modules found **45 of 59** exception
types outside `modules/core/include/System/` with no explicit HResult
(`Threading`, `Net`, `IO`, `Text.Json`, `Xml`, …). Most may well be correct —
`AggregateException` proves that inheriting can be right — but none has been
checked against the reference. That is a **newly discovered population**, not a
CCF-016 finding: it receives **no `SR-AUD-*` identifier**, is recorded as
inactive ticket **#1875**, and is explicitly out of this family's scope (§10).

---

## 7. Compatibility classification

**Compatible. Implement.** Measured rather than assumed:

| Dimension | Effect |
|---|---|
| Public source | none — no signature, parameter, default argument or overload set changes |
| Vtable / virtuals | none — no virtual added, removed or reordered; the destructor chain is untouched |
| Object layout | none — `hResult_` already exists on `Exception`; **no member is added to any of the eleven**, all of which stay empty derived classes |
| Calling convention / mangling | none — constructors keep their signatures; these are header-only inline definitions |
| `noexcept` | none — no constructor is `noexcept` before or after |
| Exception taxonomy | none — no type changes, no `catch` clause changes meaning |
| Observable behaviour | **yes**: `getHResultProperty()` returns the documented .NET value instead of the base's |

The observable change is precisely what the project's own porting checklist
requires — CLAUDE.md item 5: *"Verify that default messages, constants, and
HResult/error codes match the .NET source where applicable."* Correcting a wrong
constant to its documented reference value is remediation under a standing rule,
not a discretionary behaviour change, so it is **not** approval-gated. This
repository has already landed the identical shape: `InvalidCastException` and
`InsufficientExecutionStackException` carry per-constructor
`setHResultProperty` with `COR_E_*` comments and HResult assertions in
`ExceptionRemainingTests.cpp`.

---

## 8. Structural repair

Follow the **repository's established idiom**, which is also .NET's majority
form: one `setHResultProperty(static_cast<SharpRuntime::intcs>(0x…));` statement
with a trailing `// COR_E_…` comment in **every** public constructor body.

Constructor delegation was considered and rejected as the general shape:

- It cannot remove the duplication anyway — a `(message, inner)` constructor
  cannot delegate to a `(message)` one — so most types would still need two or
  three assignment sites.
- It would introduce a second idiom into a family of ~30 already-correct
  siblings, which CLAUDE.md's "no broad header refactor" rule exists to prevent.
- The risk it protects against — a mistyped hex constant repeated per
  constructor — is closed instead by §9's per-constructor exact-value tests,
  which is the enforcement CCF-016 actually asked for.

**No base constructor can overwrite the derived value**: in C++ the base
subobject is fully constructed before the derived constructor body runs, so a
statement in the derived body is always last. The probe's before/after pairs
demonstrate this empirically for all three base chains present here (`Exception`,
`SystemException`, `TypeLoadException`, and `ArgumentException` via
`SystemException`).

`ContextMarshalException` is the one type where .NET delegates. The port's four
constructors already forward to `SystemException` individually; they gain the
assignment individually too, which produces the same observable result.

---

## 9. Permanent test matrix

Add-only. For **every one of the 40 public constructors**:

| Axis | Requirement |
|---|---|
| Exact HResult | `EXPECT_EQ(ex.getHResultProperty(), static_cast<intcs>(0x…))` — hexadecimal literal, one assertion per constructor |
| Exact runtime type | the object is of the derived type (compile-time by construction) |
| Default message | unchanged for all eleven; `DuplicateWaitObjectException`'s asserted **verbatim** against the reference string, pinning §6.1 |
| Custom message | preserved and still reachable through `what()` |
| Parameter name | `DuplicateWaitObjectException(parameterName)` and `(parameterName, message)` still append `(Parameter 'x')` exactly once |
| Inner exception | the `(message, inner)` forms still carry the inner exception, and still set the code |
| Catch through derived reference | `catch (const DerivedType&)` |
| Catch through base reference | `catch (const SystemException&)` / `catch (const ArgumentException&)` / `catch (const Exception&)` as appropriate per type, with the HResult still correct when caught as a base |
| Copy / move | copy-construct and copy-assign preserve the HResult (public, implicitly declared) |
| Base contrast | `SystemException` still reports `0x80131501` and `TypeLoadException` still `0x80131522` — the derived fix must not leak upward |
| Control group | `AggregateException` still reports `COR_E_EXCEPTION`, pinning §6.2 |

Throw sites: the eleven types have no in-repository `throw` sites that assert an
HResult today; the tests above cover the constructors, which is where the value
is decided. A grep-verified list is recorded in the ticket notes.

**Mutation check.** Removing any single `setHResultProperty` line must fail at
least one permanent test. Verified for a sample before the ticket closes and
recorded in the notes.

---

## 10. Explicit exclusions

1. **`AggregateException`** — correct parity (§6.2).
2. **The 45 unaudited exception types in other modules** — inactive ticket
   **#1875**, no `SR-AUD-*` identifier, no change in this batch (§6.4).
3. **`DuplicateWaitObjectException`'s default message** — false positive
   (§6.1); the text already matches the reference byte-for-byte.
4. **Serialization constructors** — out of scope by the standing project
   deviation; the reference's are `[Obsolete]` and set no HResult.
5. **The `errorCode`-taking constructor form** that some already-correct types
   expose (e.g. `InvalidCastException(message, errorCode)`) — none of the eleven
   declares one, and this family does not add API surface.
6. **`HResults` as a named-constant header** — the repository spells the value as
   a hex literal with a `// COR_E_…` comment in ~30 existing types; introducing a
   constants header would be a cross-cutting refactor with its own review.

---

## 11. Sanitizer applicability

**Not applicable, stated rather than skipped.** The change adds one integer
store to each of 40 constructor bodies. No allocation, no ownership transfer, no
pointer arithmetic, no lifetime change, no shared state, and no new member. ASan,
UBSan, LSan and TSan have nothing to observe that the existing suites do not.

The exception-family suites already run under `build-asan` from earlier tickets
(#1807's `AggregateException` work), and nothing in this change alters
constructor ownership or inner-exception handling, so no sanitizer target needs
refreshing. This is recorded as a deliberate judgement, not an omission.

---

## 12. Completion criteria

1. All five findings `remediated` in `audit/AUDIT_FINDINGS_INDEX.md` and in their
   per-file reports, with §6's corrections appended and historical text preserved.
2. All 40 public constructors of the 11 types assign their documented value.
3. The post-fix run of `build-probe/1873_hresult_probe.cpp` reports `wrong=0`,
   with the control group and the three base values still `OK`.
4. The §9 matrix landed, add-only, mutation-checked, no test-count regression
   against the 14,444 floor.
5. `cmake --build build --parallel 3` clean; `scripts/local_ci_check.sh build`
   passes; Doxygen within the 1,942 ceiling; graph 41/91; fixtures 9/66; seams
   2/18 — this family changes none.
6. `AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-016 gains a closure paragraph.
7. Inactive ticket #1875 exists for the 45-type population, carrying the measured
   evidence.

---

## 13. Ticket breakdown and status

| Ticket | Findings | Scope | Size | Status |
|---|---|---|---|---|
| **#1873** | — | This plan. Design only. | S | todo → done |
| **#1874** | SR-AUD-093/094/095/096/100 | Assign the documented HResult in all 40 public constructors of the 11 types; add per-constructor exact-value tests; correct SR-AUD-100's message premise without changing the message. | M | todo |
| **#1875** | — (no identifier) | **Inactive.** Audit the 45 exception types outside `modules/core/include/System/` that carry no explicit HResult, against the reference. Evidence recorded; no change. | L | todo (inactive) |

### Implementation status

| Finding | Ticket | Status |
|---|---|---|
| SR-AUD-093 | #1874 | **`remediated`** (2026-07-30) |
| SR-AUD-094 | #1874 | **`remediated`** (2026-07-30) |
| SR-AUD-095 | #1874 | **`remediated`** (2026-07-30) |
| SR-AUD-096 | #1874 | **`remediated`** (2026-07-30) |
| SR-AUD-100 | #1874 | **`remediated`** (2026-07-30) |

**CCF-016 is CLOSED (2026-07-30).** All five findings are `remediated` and every
completion criterion in §12 is met. `build-probe/1873_hresult_probe.cpp` reports
`wrong=0` after #1874, with the three base values and all three controls still
`OK`. Ticket #1875 remains open and inactive for the 45-type population in §6.4.

---

## 14. Correction and completion of the 45-type population (#1875, 2026-08-01)

The historical scope and inactive classification above are preserved. The user
subsequently confirmed that #1875 was still wanted after the approved text
subset work, and the complete reference sweep is now recorded in
`docs/ExceptionHResultPopulationDecision.md`.

The earlier binary expectation — each type either assigns a dedicated code or
inherits — was incomplete. At official `dotnet/runtime` commit
`0eb5481340ea675857c7a7abf18f68a60b52a686`, the 45 rows divide into 12
type-specific assignments, 30 pure inheritance rows, and 3 conditional
propagation rows. The port was already exact for 27 pure controls. Twelve
dedicated types were wrong, and the reduced `Win32Exception` base made its own
value plus the represented NetworkInformation, Socket and WebSocket families
wrong. Those exact constant/inheritance results are remediated without changing
a declaration. The conditional inner-HResult mismatches in
`HttpRequestException` and `WebException` are separable and retained as
inactive ticket #1932 rather than broadened into this sweep.

The prefix matrix failed 13/15 tests; the postfix matrix passes 15/15 with 70
exact assertions. SR-AUD-157 moves to `remediated`; SR-AUD-158, SR-AUD-159,
SR-AUD-196, SR-AUD-230 and SR-AUD-250 remain at their prior states. No new
`SR-AUD-*` identifier was issued and numbering remains frozen at 364. Ticket
#1875 is complete.
