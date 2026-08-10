<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `AppDomain` compatible slice — plan

Ticket #2248. Two frozen audit findings in
`modules/core/include/System/AppDomain.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-103 | medium | `AppDomain` discards public data and switch state instead of forwarding it to `AppContext` |
| SR-AUD-104 | medium | `ApplyPolicy` accepts invalid assembly identity strings that .NET rejects |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **slice of one header**, not a `System::AppDomain`
namespace review and not a `modules/core` review.

---

## 1. Exact scope, and what is deliberately left out

In scope: `AppDomain::SetData`, `AppDomain::GetData`,
`AppDomain::IsCompatibilitySwitchSet`, `AppDomain::ApplyPolicy`, the out-of-line
half in `modules/core/src/System/AppDomain.cpp`, and a new
`modules/core/tests/System/AppDomainTests.cpp` — the audit records that **no
dedicated `AppDomain` fixture existed** and that no test invoked any of these
public members.

Out of scope, by decision rather than omission:

- **SR-AUD-102** (`AppContext.hpp`), which is a *different* finding about
  `AppContext`'s own named-data behaviours — the `APP_CONTEXT_BASE_DIRECTORY`
  override and string-valued switches. This slice makes `AppDomain` forward *to*
  `AppContext`; it does not change what `AppContext` does, and forwarding neither
  fixes nor worsens SR-AUD-102. It stays `confirmed` and unclaimed.
- **The four event add/remove pairs**, listed under the audit report's "other
  missing assertions" rather than as a finding. Making them functional is an
  event-dispatch design (who raises `UnhandledException`, and from where), not a
  bounded repair; four adjacent report references
  (`UnhandledExceptionEventHandler`, `UnhandledExceptionEventArgs`,
  `ResolveEventArgs`, `ResolveEventHandler`) point at SR-AUD-103 for the same
  no-op, which is a sign of how wide that would go. Untouched.
- **`RelativeSearchPath`, `DynamicDirectory`, shadow-copy and the obsolete path
  stubs**, which the report itself says need "an API-baseline decision".

## 2. Are these one family? — a slice, not a cause family

**They are not a cause family**, and SR-AUD-103 is not even internally one thing:

- **SR-AUD-104 is a missing argument validation.** One method, no state, no
  collaborator; .NET's own body is two guards and a `return`.
- **SR-AUD-103 is a missing delegation** — three methods that should be
  forwarding calls to `AppContext` and are constants instead. It splits cleanly
  into a **data half** and a **switch half**, and the two are in different
  compatibility classes (§5). Only the data half is ordinary work.

So: one bounded review (#2248), two compatible implementation tickets (#2249,
#2251), one approval request (#2250) and one deferred verification (#2252).

## 3. Before evidence, measured 2026-08-10

`build-probe/2248_probe1_before.cpp` against the shipped
`build/libsharp_runtime_core.a`: 14 rows, **8 OK / 6 BAD**
(`build-probe/2248_probe1_before.log`).

```
BAD  103 domain_reads_context  GetData=nullptr
BAD  103 domain_roundtrip_context  AppContext::GetData=nullptr
BAD  103 domain_roundtrip_self  AppDomain::GetData=nullptr
BAD  103 domain_switch_true  got false
BAD  104 empty_rejected  returned len=0 []
BAD  104 leading_nul_rejected  returned len=2 [\0x]
```

Both findings reproduce exactly as filed, including SR-AUD-104's own
`nul_policy_length=2` — the probe's `len=2 [\0x]` is the same measurement.

The controls hold: an absent key already returned `nullptr`, an unset switch
already returned `false`, and a valid assembly name was already returned
unchanged.

Two rows are not verdicts but **inputs to the design**:

- `AppContext::TryGetSwitch("")` **throws** `System::ArgumentException` today —
  measured, not assumed. That is the door `IsCompatibilitySwitchSet` would have
  to forward through.
- `IsCompatibilitySwitchSet` **is** declared `noexcept` (the probe measures the
  call with the reference and the `std::string` argument bound *outside* the
  `noexcept` operand; measuring
  `noexcept(domain.IsCompatibilitySwitchSet("x"))` instead answers a different
  question, because the `const char*` → `std::string` conversion is itself
  potentially-throwing, and the first version of this probe got `0` for exactly
  that reason).

## 4. The compatible members

### 4.1 SR-AUD-103, data half — `SetData`/`GetData` forward to `AppContext` (#2249)

.NET implements both as direct `AppContext` forwarding calls. This port has
exactly one domain, so the domain's data store and the context's data store are
not merely similar, they are the same store; the stub made them two.

The bodies move **out of line** into `modules/core/src/System/AppDomain.cpp`.
This is forced, not stylistic: `System/AppContext.hpp` includes
`System/AppDomain.hpp` for `BaseDirectory`, so the include cannot run both ways.
The consequence is that two symbols that were previously emitted into every
consumer TU are now emitted once into `libsharp_runtime_core.a` — an **additive**
ABI change (two new symbols), with no signature, layout or `noexcept` change.

The pointer-ownership contract is `AppContext`'s existing one and does not move:
the store holds a `void*` and owns nothing. This is now stated in the
doc-comment, because a stub that ignored its argument had no lifetime contract to
state.

### 4.2 SR-AUD-104 — `ApplyPolicy` validates before applying the identity route (#2251)

Empty → `System::ArgumentException("The value cannot be an empty string.",
"assemblyName")`, which is the message this repository already uses at seven
other empty-string doors (`AppContext`, `WebHeaderCollection`, `XmlConvert`,
`StringBuilder`, `Path`, …) and is .NET's `SR.Argument_EmptyString`.

Leading NUL → `System::ArgumentException("String cannot be of zero length.",
"assemblyName")`, .NET's `SR.Argument_StringZeroLength`. The `/rv` reference tree
is absent in this environment, so the exact resource *text* could not be
re-verified from source; the audit report's reading of that reference ("rejects
null-or-empty `assemblyName` and a leading NUL") is the basis, the **behaviour**
(which inputs throw, the exception type, and the parameter name) is what the
tests pin, and **#2252** carries the text as a deferred verification rather than
claiming a parity it cannot show.

**The repair deliberately does not over-reach.** .NET checks `assemblyName[0]`
only, so `"a\0b"` is a legal argument and is returned unchanged. The finding's
phrase "embedded/leading NUL" reads as though both should be rejected; rejecting
an interior NUL would be a *stricter-than-.NET* port. `ApplyPolicy_InteriorNul_
ReturnedUnchanged` pins the narrower rule, and mutation M3 exists to prove the
pin bites.

The null case stays unrepresentable: the parameter is a `const std::string&`.
That adaptation is unchanged and is now stated in the doc-comment.

## 5. The approval boundary this slice found and did not cross (#2250)

`IsCompatibilitySwitchSet` **cannot** be forwarded compatibly. .NET's body is

```csharp
public bool? IsCompatibilitySwitchSet(string value)
    => AppContext.TryGetSwitch(value, out bool result) ? result : default(bool?);
```

and following it needs **two** changes this repository treats as approval-bound:

1. **A public return-type change**, `bool` → a nullable equivalent. The audit
   states this half of the finding directly: a `bool` "cannot expose the .NET
   distinction between an unset switch and a switch explicitly set false". No
   amount of forwarding fixes that; the type has to change.
2. **Dropping `noexcept`.** `AppContext::TryGetSwitch("")` throws
   `System::ArgumentException` — measured in §3, not assumed — and it also takes
   a `std::mutex`, whose `lock()` can throw `std::system_error`. A `noexcept`
   function that forwards into either becomes `std::terminate`. The repository
   already treats a `noexcept` drop as needing a user decision: **#2215** is
   `needs_user` for exactly that on `ArraySegment`'s enumeration door.

A third route was considered and rejected: keep both, pre-check for empty, and
return `false` for it. That keeps the signature but invents a policy — it makes a
`noexcept` door silently swallow an argument error that .NET reports — and it
still leaves the mutex inside a `noexcept` function. Inventing policy is what
this repository's own rules say not to do at an approval boundary.

So the member is **left exactly as it is, documented and pinned**: the
doc-comment now says it does not consult `AppContext`, why, and which ticket
carries the request; `AppDomainTests.IsCompatibilitySwitchSet_DoesNotYetConsult
AppContext` pins the divergence with a positive control proving `AppContext`
itself does report the switch; and `..._IsStillNoexcept` pins the specifier, so
the test that must change when #2250 is approved is identified in advance.

SR-AUD-103 therefore stays **`confirmed`** with its data half recorded as
remediated — the same convention SR-AUD-259 already uses in the index.

## 6. CCF relationships — none minted, none extended

- **CCF-011** (empty callables crossing public boundaries) — not applicable.
  Nothing here takes a callable.
- **CCF-005** (high-value conversion APIs need explicit boundary and
  special-value validation) — SR-AUD-104 is *shaped* like a CCF-005 member and is
  **not** one. CCF-005 is about conversion APIs whose *result* is wrong for
  unvalidated input; `ApplyPolicy` performs no conversion, it is an identity
  function with an argument contract. Recorded as an adjacency.
- **CCF-019** (borrowed views with no liveness bound) — the `void*` data store
  hands out a raw pointer the caller supplied and the container never owned. That
  is the caller's own pointer coming back, not a view into container-owned
  storage, and the contract is `AppContext`'s pre-existing one, which this slice
  does not change. Adjacency, not a member; CCF-019 stays open and untouched.
- **CCF-021 / #2131** and **CCF-022 / #2109** remain unminted and untouched.

## 7. Compatibility, ABI, layout and `noexcept`

| Property | Before | After |
|---|---|---|
| `sizeof(AppDomain)`, members, order | 2 `std::string` | unchanged |
| Virtual functions, vtable | inherited from `MarshalByRefObject`, none added | unchanged |
| `noexcept` on any member | `getIdProperty`, `getIsFullyTrustedProperty`, `getIsHomogenousProperty`, `IsDefaultAppDomain`, `IsFinalizingForUnload`, `getShadowCopyFilesProperty`, `IsCompatibilitySwitchSet` | **all unchanged** |
| Public signatures | — | **unchanged**; `SetData`/`GetData` keep their parameter and return types and only move out of line |
| Exported symbols | — | **+2** (`AppDomain::SetData`, `AppDomain::GetData`), additive |
| New header include | — | `System/ArgumentException.hpp` (which includes only `SystemException.hpp` and `<string>` — no cycle) |

**Two intended behaviour changes**, both matching .NET and neither reachable
before without the same call being wrong:

- `GetData`/`SetData` now observe and mutate real state. Code that relied on
  `GetData` always returning `nullptr` was relying on the stub, which the
  doc-comment described as a stub.
- `ApplyPolicy("")` and `ApplyPolicy("\0…")` now throw where they returned. Both
  are inputs .NET rejects.

## 8. Test matrix

Fourteen permanent tests in the new
`modules/core/tests/System/AppDomainTests.cpp`, in the existing
`SharpRuntimeTests_Core_Base` executable (the module's test glob picks the file
up; no CMake edit, no component or module boundary change).

| # | Test | Pins |
|---|---|---|
| 1 | `GetData_ReadsWhatAppContextStored` | §3 `domain_reads_context` |
| 2 | `SetData_IsVisibleThroughAppContext` | §3 `domain_roundtrip_context` |
| 3 | `SetData_RoundTripsThroughTheDomainItself` | round trip and overwrite |
| 4 | `GetData_UnknownName_ReturnsNullptr` | the control that must not move |
| 5 | `SetData_NullValue_IsStoredAndReadBack` | a null value is a value |
| 6 | `IsCompatibilitySwitchSet_DoesNotYetConsultAppContext` | §5, with an `AppContext` positive control |
| 7 | `IsCompatibilitySwitchSet_IsStillNoexcept` | §5, the specifier #2250 must change |
| 8 | `ApplyPolicy_ValidName_ReturnedUnchanged` | the identity route survives |
| 9–10 | `ApplyPolicy_EmptyName_*` | throw, parameter name, **and message** |
| 11–12 | `ApplyPolicy_LeadingNul_*` | throw, parameter name, **and message** |
| 13 | `ApplyPolicy_InteriorNul_ReturnedUnchanged` | §4.2, the repair does not over-reach |
| 14 | `ApplyPolicy_SingleCharacterName_ReturnedUnchanged` | the one-character boundary |

Keys are per-test: `AppContext`'s store is process-wide and has no public
removal, so sharing a name between tests would couple them.

## 9. Sanitizer matrix

**Not run, deliberately.** Both findings are wrong-value / missing-validation
classes: a stub returning a constant, and an absent argument check. Nothing here
reads uninitialised memory, indexes out of bounds or races. The one pointer
involved (`void*` in the data store) is stored and returned without being
dereferenced by any code in this slice. A sanitizer run would be clean and would
discriminate nothing.

## 10. Slice completion criteria

1. SR-AUD-104 `remediated`; SR-AUD-103 `confirmed` with its data half recorded as
   remediated and its switch half ticketed.
2. Zero errors and zero warnings from `cmake --build build --parallel 2`.
3. No test-count regression; the fourteen new tests accounted for exactly.
4. No signature, layout or `noexcept` change.
5. Every deliberately-not-done item carries a ticket: #2250 (approval), #2252
   (deferred verification).
## 11. Outcome, measured 2026-08-10

`build-probe/2248_probe2_after.log`, the identical probe source recompiled
against the repaired library: **13 OK / 1 BAD**, down from 8/6. The single
remaining `BAD` is `103 domain_switch_true` — the half #2250 carries, still
failing on purpose.

```
OK   103 domain_reads_context  GetData=ptr
OK   103 domain_roundtrip_context  AppContext::GetData=ptr
OK   103 domain_roundtrip_self  AppDomain::GetData=ptr
BAD  103 domain_switch_true  got false
OK   104 empty_rejected  threw: The value cannot be an empty string. (Parameter 'assemblyName')
OK   104 leading_nul_rejected  threw: String cannot be of zero length. (Parameter 'assemblyName')
OK   104 interior_nul_accepted_unchanged  returned len=3 [a\0b]
```

`SharpRuntimeTests_Core_Base`: 5,742 → 5,756 tests (5,741 → 5,755 passing, the
same 1 skipped throughout, 0 failing) — exactly the fourteen added here.

### 11.1 Mutation checks, and the one that first survived

Every mutation was applied to production source, rebuilt with
`cmake --build build --parallel 2`, and re-run
(`build-probe/2248_mutations.log`).

- **M1** — `SetData`/`GetData` restored to the stub bodies. **3 tests fail**
  (matrix rows 1–3). Rows 4–5 correctly survive: `nullptr` for an unknown key and
  for a stored null value is what a stub also returns, so those rows are controls
  by construction, not blind spots.
- **M2** — `ApplyPolicy` drops the empty-string guard. **This mutation SURVIVED
  the first version of the test matrix**, and the reason is worth recording,
  because it is a property of `std::string` rather than of the repair: for a
  `const std::string&`, `assemblyName[0]` with `size() == 0` is **well-defined**
  and yields the null character, so the *leading-NUL* branch alone already throws
  for an empty name — same exception type, same parameter name, different
  message. Two tests were added to assert the **message** of each branch; M2 now
  fails exactly one test. The matrix was wrong, not the mutation.
- **M3** — `ApplyPolicy` over-reaches to `find('\0') != npos`, i.e. rejects an
  interior NUL as the finding's wording could be read to ask. **1 test fails**
  (row 13). This is the mutation that proves the port stayed as strict as .NET
  and no stricter.

No mutation was skipped as unsafe, and none is equivalent.

### 11.2 What did not change

`AppContext.hpp` (SR-AUD-102 untouched), the six other `noexcept` members, the
event stubs, the obsolete path stubs, `sizeof(AppDomain)`, the component graph,
the component catalogue and the module boundaries. The new test file is a
`modules/core` test source discovered by the module's existing glob, so no CMake
file changed either.
