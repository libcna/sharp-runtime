<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `AppContext` named-data configuration — design record

Ticket #2255. One frozen audit finding in
`modules/core/include/System/AppContext.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-102 | medium | `AppContext` named data cannot configure `BaseDirectory` or compatibility switches |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is **design-first by instruction**: the inherited
ranking said "verify the approval boundary before committing to it", and the
verdict below is that the boundary is real and covers **both** halves of the
finding.

---

## 1. What the finding actually asks for

Two distinct .NET behaviours, both reached through the *named data store*:

1. **`BaseDirectory` override.** .NET's `AppContext.BaseDirectory` first looks for
   named data key `APP_CONTEXT_BASE_DIRECTORY` and, if it holds a string, returns
   that instead of the computed directory.
2. **String-valued switch.** .NET's `TryGetSwitch` falls back to the named data
   store when no explicit switch entry exists, and parses a **string** value there
   as the switch's boolean.

Both are reachable in .NET because `SetData(string, object)` stores a *boxed
object* whose runtime type can be interrogated. This port stores `void*`.

The finding's own sentence is exact and worth quoting, because it names the cause
rather than the symptom: *"The public `void*` data adaptation supplies no runtime
type tag or ownership, so it cannot implement .NET's string-only `BaseDirectory`
override or safely recognize a string switch value."*

---

## 2. Verified current state

- `AppContext::GetData(const std::string&) -> void*` and
  `AppContext::SetData(const std::string&, void*)`, both `public`, both taking the
  one process-wide `std::mutex`, both storing into
  `std::unordered_map<std::string, void*>`.
- `AppContext::getBaseDirectoryProperty() -> const std::string&`, which delegates
  **directly** to `AppDomain::CurrentDomain().getBaseDirectoryProperty()`. It never
  consults the data store, so premise 1 reproduces by inspection.
- `AppContext::TryGetSwitch(const std::string&, bool&)` and `SetSwitch` use a
  **separate** `std::unordered_map<std::string, bool>`. `TryGetSwitch` never
  consults the data store, so premise 2 reproduces by inspection.
- `AppContext::getTargetFrameworkNameProperty()` returns `{}` unconditionally — a
  declared reflection deviation, not part of this finding.

**Both premises are confirmed exactly as filed.** No correction is needed, which is
itself worth recording: the previous four Core units each corrected something.

### Consumers, and why they matter to the verdict

| Consumer | What it uses |
|---|---|
| `AppDomain::SetData` / `GetData` (`AppDomain.cpp`, added by **#2249**) | forwards straight through, so any signature change here changes `AppDomain`'s public signature too |
| `AppDomainSetup::getApplicationBaseProperty()` | returns `AppContext::getBaseDirectoryProperty()`, i.e. a `const std::string&` |
| `AppDomainSetup::getTargetFrameworkNameProperty()` | forwards |
| `AppDomainTests`, `AppDomainSetupTests`, `AppContextExtraTests` | pin the current round-trip behaviour |

---

## 3. Verdict: approval-sensitive in **both** halves; no compatible behaviour subset

Three routes exist to either behaviour. All three are blocked, and the reasons are
different, so all three are recorded rather than one being chosen by default.

### Route A — give the data store a runtime type

Change the stored value from `void*` to something type-tagged (`std::any`, a
variant, or `std::shared_ptr<void>` plus `std::type_index`). This is the **only**
route that implements .NET's semantics faithfully.

It is a **public signature change on four public members across two classes** —
`AppContext::SetData`, `AppContext::GetData`, `AppDomain::SetData`,
`AppDomain::GetData` — the last two only because **#2249** deliberately made them
forwarders eleven days ago. It also retires the `SetData_GetData_RoundTrip` and
`GetData_UnknownKey_ReturnsNullptr` pins and `AppDomainTests`'
`SetGetData_ForwardsToAppContext`. This is the public-representation family this
repository has declined four times; it needs approval, not inference.

### Route B — `reinterpret_cast` the `void*` for the two special keys

Assume that whatever is stored under `APP_CONTEXT_BASE_DIRECTORY` is a
`std::string`, and that a switch key's value is a `std::string`, and cast.

**Rejected as undefined behaviour by construction**, not as a style preference. A
`void*` carries no type, so the assumption is unfalsifiable at the point of use: a
caller who stored an `int*` under that key gets a `std::string` lvalue formed over
four bytes of `int`. Nothing in the type system, no sanitizer and no test can
distinguish the correct case from the corrupting one, because both are the same
instruction sequence. A public API whose safety depends on an undocumented
convention is worse than the gap it closes.

### Route C — add a separate typed string channel

Add e.g. `SetStringData(name, value)` beside the `void*` store, and have
`BaseDirectory` and `TryGetSwitch` consult only that.

**Rejected as inventing public API.** .NET has one named data store, not two; a
second one would diverge from the type being ported in order to emulate it, and
callers would have to know which door a given key lives behind. It is also not what
the finding asks for.

### The reference-lifetime problem, which Route A does not by itself solve

`getBaseDirectoryProperty()` returns `const std::string&`. Today that reference
binds to storage `AppDomain` owns for the process lifetime. If the value came from
the data store, the reference would alias **caller-owned** storage whose lifetime
the callee cannot see — a borrowed reference with no liveness boundary, which is
exactly the **CCF-019** shape. Returning by value instead is a second public
signature change, and it propagates to
`AppDomainSetup::getApplicationBaseProperty()`.

**CCF-019 is deliberately not extended here.** This is recorded as an adjacency, in
the same way #2243 recorded CCF-011 for `Property` and #2248 recorded CCF-019 for
this very `void*` store. CCF-019 stays open with its existing membership.

---

## 4. What #2255 asks the user

One decision, with the alternatives priced:

> May `AppContext`'s named data store carry a runtime type, and may
> `getBaseDirectoryProperty()` return by value?
>
> - **(a) Yes to both.** SR-AUD-102 is implementable faithfully. Cost: four public
>   member signatures change (`AppContext::SetData`/`GetData`,
>   `AppDomain::SetData`/`GetData`), one public return type changes
>   (`AppContext::getBaseDirectoryProperty`, propagating to
>   `AppDomainSetup::getApplicationBaseProperty`), and three existing tests are
>   retired and rewritten. No object layout moves — every member is static, and the
>   maps are function-local statics.
> - **(b) Yes to the store, no to the return type.** The string-valued switch
>   becomes implementable; the `BaseDirectory` override does **not**, because it
>   cannot be returned safely by reference. SR-AUD-102 would then split, exactly as
>   SR-AUD-103 did, into a remediated switch half and a still-`confirmed`
>   `BaseDirectory` half.
> - **(c) No.** SR-AUD-102 stays `confirmed` permanently and the two .NET
>   behaviours are recorded as accepted deviations in the class doc-comment, in the
>   same register as the other declared deviations in `CLAUDE.md`'s parity
>   philosophy.

**No option is applied on inference.** Nothing in the repository decides this, and
guessing would be inventing public policy.

---

## 5. What #2256 does now, compatibly

Route A/B/C are all blocked, but the audit report's "other missing assertions"
section lists five things that are pure test and documentation work, and every one
of them is a *baseline* a future approved repair will need:

- the raw-pointer lifetime/ownership adaptation is nowhere documented — a caller
  cannot currently tell from the header that `SetData` stores a **borrowed**
  pointer it does not own and will not keep alive;
- no test covers a null data value, replacement of an existing key, or concurrent
  use of the two maps;
- no test records that `getBaseDirectoryProperty()` returns a reference to storage
  that outlives the call;
- no test distinguishes the always-empty `TargetFrameworkName` from an unavailable
  entry assembly.

#2256 adds those tests and the ownership/lifetime doc-comments, and changes **no
behaviour**. It deliberately does **not** add string-switch parsing or a
`BaseDirectory` override, because those are §4's decision.

**#2250 is not touched and cannot be reached from here.**
`AppDomain::IsCompatibilitySwitchSet` remains the stub it is; nothing in #2256
changes what `AppContext::TryGetSwitch` returns, so nothing changes what a future
approved #2250 forwarding would observe.

---

## 6. Disposition

**SR-AUD-102 stays `confirmed`** — the split convention SR-AUD-103 and SR-AUD-259
already use. Neither of its two behaviours is implemented, and the compatible work
in #2256 pins the current contract rather than changing it.
