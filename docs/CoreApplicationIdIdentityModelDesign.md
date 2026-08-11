<!-- SPDX-License-Identifier: MIT -->

# `ApplicationId` — SR-AUD-124 / SR-AUD-125 review

Tickets: **#2290** (review), **#2291** (design/approval, `needs_user`),
**#2292** (a separate latent defect found while reviewing, no `SR-AUD-*`
identifier). Date: 2026-08-11. The audit numbering stays frozen at 364 and **no
CCF was minted**.

---

## 1. Verdict — a genuine pair, and the dependency runs one way

Unlike the two slices reviewed before it in this batch, this one **is** a real
pair, and not because both findings sit in one header. SR-AUD-125's faithful
repair *requires* SR-AUD-124's decision:

- .NET's `ToString()` writes `publicKeyToken` **as uppercase hex bytes**. That
  sentence only has meaning if the token is a `byte[]`. While the token is a
  `std::string` with no declared encoding — SR-AUD-124's first complaint — there
  is no defensible answer to "what does `ToString` print for it": the raw text,
  hex of the storage bytes, and hex of a decoded value are three different
  strings and nothing in the port says which the caller supplied.
- .NET's `ToString()` **omits components that are null**. That sentence only has
  meaning if Culture and ProcessorArchitecture can *be* null — SR-AUD-124's
  second complaint. This port cannot distinguish absent from empty, so "omit the
  null ones" is not expressible.

So **SR-AUD-125 is not independently compatible**, and splitting it out would
mean either changing the emitted text without reaching parity (a break that buys
nothing) or waiting for #2291 anyway. They are held together, and the review was
told not to split them artificially if the implementation proves inseparable.
It does.

The dependency is one-way: SR-AUD-124 could in principle be decided without
touching `ToString`. That is why #2291 separates the options rather than offering
one all-or-nothing route.

## 2. Both findings reproduce exactly as filed

Read off `ApplicationId.hpp` against the frozen reference basis (local .NET
`System/ApplicationId.cs:9-83`; `/rv` is absent now, so that record is the
authority and nothing below invents .NET text):

| Claim | Verified |
|---|---|
| takes all three as mandatory `std::string` | yes — constructor `:35-42`, five by-const-reference parameters, no `optional` |
| accepts an empty name | yes — no validation of any kind in the constructor body |
| documents neither byte encoding nor a null/empty sentinel | yes — the `@param` line said only "(as a string)" |
| `ToString` never includes the token | yes — `publicKeyToken_` does not appear in the expression |
| emits unquoted capitalized keys, always both optional fields | yes — `name_ + ", Version=" + … + ", Culture=" + … + ", ProcessorArchitecture=" + …` |
| unequal tokens serialize identically | follows directly — the token is not read |

No premise correction was needed on either finding, which is worth recording
because the two slices before it in this batch each corrected something.

## 3. Consumer inventory — measured, not inherited

| Kind | Count | Sites |
|---|---:|---|
| First-party production consumers | **0** | none. `ApplicationIdentity` is a *different* type that neither includes nor mentions `ApplicationId` |
| Test consumers | 2 files, 18 cases | `modules/core/tests/System/ApplicationIdTests.cpp` (16, `SharpRuntimeTests_Core_Base`) and `tests/integration/Task42Tests.cpp` (2, `SharpRuntimeIntegrationTests`) |

The two files share the suite name `ApplicationIdTests` but live in **different
executables**, and the overlapping case was already renamed
(`ToString_ContainsName_New` against `ToString_ContainsName`), so this is not the
same one-binary duplication seen on `Void` and `UnitySerializationHolder`.

Zero production consumers does **not** license a public source break here either:
downstream consumers exist and this batch may not inspect them.

## 4. What each route costs

### 4.1 The token (SR-AUD-124, first half)

| Route | Public effect |
|---|---|
| **A1** `std::vector<bytecs>` (or `std::span`-taking overload) for the token, cloned on construction as .NET clones | **source break**: constructor parameter type and `getPublicKeyTokenProperty()` return type both change; every existing call site changes |
| **A2** keep `std::string`, declare it an **uppercase-hex** contract and validate | no signature change, but **existing values become invalid input** — `"token123"` and `"pubkey"`, the two the tests use, are not hex — so it is a behaviour break dressed as a documentation fix |
| **A3** keep `std::string`, declare it opaque text, and state that binary tokens are unrepresentable | **no break**; does not close the finding (binary key material stays unrepresentable), taken as a partial measure in §6 |

### 4.2 Nullable Culture / ProcessorArchitecture (SR-AUD-124, second half)

| Route | Public effect |
|---|---|
| **B1** `std::optional<std::string>` parameters and getters | **source break** on two getters and the constructor; a caller reading `const std::string&` stops compiling |
| **B2** add overloads/`has…` predicates beside the existing getters | additive, but the constructor still cannot *receive* the distinction, so the model stays incomplete |
| **B3** declare the empty string the single representation of both, explicitly | **no break**; does not close the finding; taken in §6 |

### 4.3 Name validation (SR-AUD-124, third half) — the separable one

Rejecting an empty name needs **no representation decision at all**. The port
already has `ArgumentException::ThrowIfNullOrEmpty(argument, paramName)`, whose
message this repository has already chosen, so nothing would be invented; no
in-repo call site passes an empty name, so there is no migration.

It is nevertheless **not taken here**, deliberately. It converts an input that
currently succeeds into a runtime throw for every consumer, and it closes
nothing on its own — SR-AUD-124 is a conjunction, and the representation halves
would keep it open. Taking a runtime break that buys no closure, without asking,
is the opposite of what this batch is for. It is written up as #2291's first and
cheapest decision, recommended, so that approving it is a one-line answer.

### 4.4 The text (SR-AUD-125)

Any change to `ToString()` breaks a consumer that parses or logs it, and the
faithful grammar needs §4.1's decision first (hex bytes) and §4.2's (omit
nulls). A partial move — appending the token in the current grammar — would
change the text *and* still not be .NET's, which is the worst of both. One
decision, in #2291.

## 5. Not a family with the batch's earlier slices

The "public shape drift with no production consumer" characteristic that runs
through SR-AUD-124/125/126/127/128/129/136/137 is a characteristic, not a cause,
and #2279 already refused to treat it as one. These two are joined by something
much more specific and much narrower — one finding's repair needs the other's
representation — which is a dependency between two findings in one type, not a
recurring mechanism. **No CCF minted**; CCF-021 and CCF-022 remain unminted and
this batch has no authority over them.

## 6. What was done anyway, and what it does not close

The header now carries, on the class, on `ToString()` and on `GetHashCode()`,
statements that are true today and true under every option in §4:

- the token is text, stored verbatim, with **no encoding applied or assumed**,
  so binary key material cannot be round-tripped;
- there is **no null/empty distinction** — the empty string is the only
  representation of both, and they cannot be told apart afterwards;
- the name is **not validated**, where .NET rejects null or empty;
- `ToString()` is a counterpart **in role only**: its exact grammar is spelled
  out, .NET's is spelled out beside it, and the consequence is stated plainly —
  *two ApplicationIds differing only by token produce identical text, so this
  string does not identify an ApplicationId and must not be used as a manifest
  identity or an equality proxy*, while `Equals` does distinguish them;
- `GetHashCode` derives from name and version only, matching .NET, so equal
  instances hash equally and unequal ones may collide.

**Neither finding is closed.** No signature, no stored type, no emitted text and
no validation changed; a caller still cannot represent a binary token, still
cannot distinguish null from empty, still may construct an empty name, and still
gets text that omits the token. The notes make the silent divergence explicit,
which is the half of each finding that costs nothing to fix.

**No test was added**, for the reason #2281 records on `UnitySerializationHolder`:
pinning more of a surface #2291 may reshape would raise the cost of that
decision. The audit's own missing-assertion notes — exact text, token
distinction, optional omission, escaping, byte-to-hex, copy isolation of token
storage — are all assertions about the disputed model, and every one of them
would have to be retired by whichever option is approved. They belong to
#2291's implementation, and are listed there.

## 7. A separate latent defect (#2292, no `SR-AUD-*` identifier)

`GetHashCode()` is declared **`noexcept`** and calls `version_.ToString()`, which
builds a `std::string`. An allocation failure inside a `noexcept` function does
not propagate — it calls `std::terminate`. Verified by inspection of the
declaration and of `Version::ToString()`, which is not `noexcept` and returns by
value; no crash was staged to demonstrate it.

Three routes, none taken here:

| Route | Cost |
|---|---|
| drop `noexcept` | changes the member's exception specification — the class of change #2250 is already parked on for another member |
| hash `Version::GetHashCode()` (pure arithmetic, no allocation) instead of its text | no signature change; **changes emitted hash values**, which is observable |
| leave it, documented | the terminate path stays |

It is not folded into SR-AUD-124 or SR-AUD-125: it is independent of the identity
model, it survives every option in §4, and it would still be there if both
findings were closed tomorrow. Following the #2282 precedent, it gets an ordinary
ticket and no audit identifier. The header states it.

## 8. Compatibility of what landed

| Dimension | Effect |
|---|---|
| Public source | **none** — no member, parameter, return type or qualifier changed |
| ABI / symbols / layout / vtable / `noexcept` | **none** |
| Emitted text, stored values, validation | **none** — no executable statement changed |
| Includes / component graph | **none** |

## 9. Disposition

| Finding | Before | After | Owner |
|---|---|---|---|
| SR-AUD-124 | confirmed | **confirmed** — approval boundary recorded | #2290 review, #2291 `needs_user` |
| SR-AUD-125 | confirmed | **confirmed** — approval boundary recorded | #2290 review, #2291 `needs_user` |

Deliberately **not** marked design-complete. #2291 prices the routes but does not
*select* one: the choice between A1/A2/A3, B1/B2/B3 and the text grammar is the
decision itself, and claiming a completed design while the selection is open
would overstate the state.
