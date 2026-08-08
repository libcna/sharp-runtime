<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::IO::Hashing` (`modules/io-hashing`) namespace review — ticket #2140

Owning ticket **#2140**. This document is the durable record; it **remediates nothing by itself**.
Every claim is measured against the tree at `d01601f`.

`/rv/tmp/runtime/src/libraries/` is **absent** — re-verified 2026-08-08. Every statement about .NET
comes from repository-contained evidence: the per-file audit reports, doc-comments transcribed from
.NET when the module was written, and this module's own tests. **This unit is unusual in needing
almost none of it**: hashing has *published* check values, and this review measures the module
against those rather than against a reference implementation it cannot open.

**No `SR-AUD-*` identifier is issued. Audit numbering stays frozen at 364.** Post-audit defects
carry ordinary ticket numbers only.

CNA and mobile-eggbert were not inspected. Ticket **#1773 stays blocked**.

Primary evidence: `build-probe/2140_probe1_hashing.cpp`, log `build-probe/2140_probe1_before.log`
(**102 measured cases**, one forked child each).

---

## 1. Why this unit was selected — measured at this tip, after `modules/net-sockets` closed

Re-parsed from `audit/AUDIT_FINDINGS_INDEX.md` **after** #2135/#2136/#2137 moved SR-AUD-264/265/267
to `remediated`. Units with **no existing review plan** and ≥3 open findings — the only candidates,
since every other unit with open findings has been reviewed:

| Unit | Open | High | Med | Low | Actionable high | `/rv`-dependent | Existing CCF cover | Expected compatible |
|---|---:|---:|---:|---:|---:|---|---|---|
| `modules/core` | 72 | 9 | 59 | 4 | some | mixed | family plans | **not a coherent unit** — 47 files, a dozen subsystems; excluded by every previous review for the same reason |
| `modules/time-zone` | 7 | 0 | 7 | 0 | **0** | **7 of 7** | none | ~0 — a queue of deferred-verification tickets |
| `modules/globalization` | 7 | 1 | 6 | 0 | 1 | **5 of 7** | none | ~2 of 7 |
| `modules/numerics` | 4 | 0 | 4 | 0 | **0** | 2 of 4 | none | 2 of 4, and one is a **public signature break** (`Complex::Abs`) |
| `modules/xml-linq` | 4 | 1 | 3 | 0 | **0** — its `high` is `design-complete` behind #1899, whose layout approval was **declined** | 1 of 4 | CCF-019 | 2 of 4 |
| **`modules/io-hashing`** | **3** | **1** | **2** | **0** | **1** | **0 of 3** | none needed | **3 of 3** |
| `modules/io-compression` | 3 | 1 | 2 | 0 | 1 | 0 of 3 | CCF-022 candidate | 2 of 3 — SR-AUD-259 adds public constructors |
| `modules/net-network-information` | 3 | 0 | 3 | 0 | 0 | 1 of 3 | — | ~1 of 3 — the module is dominated by **#1962**, `blocked` |

**Selected: `modules/io-hashing`.** The case, in the order it decided:

1. **Every finding is compatible work. Three of three.** The batch instruction says to prefer a
   unit that produces useful compatible work over one that produces only deferred tickets. This is
   the only candidate where **nothing** is blocked, nothing needs a layout decision, nothing is a
   CCF-019 or CCF-022 member, and nothing waits on `/rv`. `time-zone` is the opposite extreme
   (7 of 7 reference questions) and `numerics` has no `high` and a signature break.
2. **An actionable `high`, and it is a memory-safety defect.** SR-AUD-260 is a **null dereference
   reachable from public API with ordinary arguments**. `xml-linq` shows a higher *nominal* high
   ratio, but its only `high` is design-complete behind a **declined** approval, so its actionable
   count is zero.
3. **Decidability without `/rv` is not merely adequate here — it is better than usual.** Hashing
   has published check values. This review verified the module's output against the **xxHash,
   CRC-32, CRC-64/ECMA-182 and Adler-32 published values** (§6.2), which is evidence no reference
   checkout could improve on.
4. **The repair target already exists inside the module.** SR-AUD-261's guard is implemented in
   XXH32/64/3/128 and missing in Adler32/CRC32/CRC64. This codebase's cleanest repairs are the ones
   that delete a divergence rather than invent a rule, and this is one of them.
5. **Module cohesion.** One component (`IO.Hashing`), one namespace, one directory: 11 headers,
   11 sources, 1 test file, 2,639 lines total, 96 tests.
6. **A dimension this unit has and the alternatives do not: portability of a published algorithm.**
   SR-AUD-262 is not a bug on this host at all — it is a defect that only appears on a big-endian
   target, which makes it exactly the kind of thing a test suite full of host-only vectors cannot
   see. It is still decidable here, structurally (§4.3).

**The honest cost, stated up front:** SR-AUD-262 **cannot be verified by running on a big-endian
host**, because there is not one. §4.3 records what *can* be proved here and what cannot, and the
ticket is scoped to the provable half rather than pretending otherwise.

---

## 2. Scope and file inventory

| Kind | Files | Lines |
|---|---:|---:|
| public headers | 11 | 1,024 |
| implementation | 11 | 1,615 |
| tests | 1 | 762 (96 tests) |

One component: `IO.Hashing`, `TYPE STATIC`. **In scope:** everything under `modules/io-hashing/`.

**Out of scope, and why:**

- `System::Security::Cryptography`'s hashes (`MD5`/`SHA*`/`HMAC`) — a different component with a
  different contract (they are in scope for the project, but not for this review).
- The **absent** async `AppendAsync` surface — recorded by the audit as a deliberate adaptation,
  not a defect; there is no `Task`-returning stream append in this port.
- `miniz`/`zlib` — `modules/io-compression`'s dependency, not this module's. Adler-32 here is a
  from-scratch implementation.
- CNA and mobile-eggbert — not inspected; **#1773 stays blocked**.

---

## 3. Complete public-surface inventory — and the one structural fact behind all three findings

| Type | Raw-pointer doors |
|---|---|
| `NonCryptographicHashAlgorithm` (base) | `Append(const bytecs*, intcs)`, `TryGetCurrentHash`, `GetCurrentHash`, `TryGetHashAndReset`, `GetHashAndReset` |
| `Adler32`, `Crc32`, `Crc64` | `Append`, `Hash` (vector), `Hash` (destination), `TryHash`, `HashToUInt32`/`HashToUInt64`, plus parameter-set overloads |
| `XxHash32`, `XxHash64`, `XxHash3`, `XxHash128` | the same set, plus a seed |
| `Crc32ParameterSet`, `Crc64ParameterSet` | `Update(value, const bytecs*, intcs)` |

**The structural fact:** .NET's surface is `ReadOnlySpan<byte>` / `Span<byte>`, and **a span cannot
represent a positive-length null buffer**. This port replaced every span with a **raw pointer plus
a signed length** and never wrote down the contract that replacement needs. All three findings are
consequences of that single substitution:

- a **null pointer** with a positive length is representable, and nothing rejects it (SR-AUD-260);
- a **negative length** is representable, and only four of seven types reject it (SR-AUD-261);
- and the byte order of a raw memory load became the implementation's business rather than the
  algorithm's (SR-AUD-262).

---

## 4. Every open finding, with its measured disposition

All three reproduced against `d01601f`. **102 cases, 25 accepted, 19 threw, 58 crashed.**

| Finding | Sev | Measured | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-260 | **high** | **confirmed, and far wider than filed** — §4.1 | compatible | **#2141** |
| SR-AUD-261 | med | **confirmed exactly as filed** — §4.2 | compatible | **#2142** |
| SR-AUD-262 | med | **confirmed; provable here only structurally** — §4.3 | compatible, scoped | **#2143** |

### 4.1 SR-AUD-260 — confirmed, and the destination half is the larger one

The finding names *"raw-pointer overloads dereference positive null buffers"* and cites the source
side. Measured, **58 of 102 cases crash with SIGSEGV**, and they decompose like this:

| Door class | Types affected | Crashes |
|---|---:|---:|
| **null source**, positive length — `oneShot`, `Hash` (vector), `Append` | **7 of 7** | 21 |
| **null destination**, claimed-sufficient length — `Hash(…, dst, n)`, `TryHash`, `GetCurrentHash`, `TryGetCurrentHash`, `GetHashAndReset` | **7 of 7** | 35 |
| `ParameterSet::Update(value, nullptr, 1)` | 2 of 2 | 2 |

**Three corrections the finding does not contain:**

1. **The destination half is bigger than the source half** — 35 crashes against 21 — and it lives
   in **one place**: `NonCryptographicHashAlgorithm`, the base class every hasher inherits. The
   source half is spread across seven implementations; the destination half is one file.
2. **The four XXH types are *not* exempt.** The per-file reports describe them as carrying "the
   repair"; that repair is the **negative-length** guard of SR-AUD-261. On the null-pointer axis
   XXH32/64/3/128 crash exactly like Adler32/CRC32/CRC64 — **all seven types, all doors.**
3. **The base class already validates the destination *length* and not the destination *pointer*.**
   The control proves it: `GetCurrentHash(dst, 1)` throws
   *"Destination is too short. (Parameter 'destination')"* on every one of the seven types, while
   `GetCurrentHash(nullptr, 16)` segfaults. The parameter name for the diagnostic already exists;
   only the pointer check is missing. **This is a one-line-per-door repair with the message
   convention already chosen by the module itself.**

**Measured positive, recorded so the repair does not over-reach:** `oneShot(nullptr, 0)` and
`Append(nullptr, 0)` are **accepted on all seven types**, which is **correct** and must stay so —
.NET's `default(ReadOnlySpan<byte>)` is an empty span with a null pointer, and appending it is
legal. A repair that rejects *every* null pointer would break the empty case. §11 makes that a
required control.

### 4.2 SR-AUD-261 — confirmed exactly as filed, and the repair already exists in the module

| Type | `oneShot(data, -1)` | `oneShot(data, INT_MIN)` | `Append(data, -1)` |
|---|---|---|---|
| `Adler32` | **accepted** | **accepted** | **accepted** |
| `Crc32` | **accepted** | **accepted** | **accepted** |
| `Crc64` | **accepted** | **accepted** | **accepted** |
| `Crc32ParameterSet::Update` | **accepted** | — | — |
| `Crc64ParameterSet::Update` | **accepted** | — | — |
| `XxHash32` / `XxHash64` / `XxHash3` / `XxHash128` | `ArgumentOutOfRangeException` *"Non-negative number required. (Parameter 'length')"* | same | same |

A negative length is not "an error that goes unreported" — it is **a successful empty operation**:
`HashToUInt32(data, -1)` returns the hash of *nothing* and the caller cannot tell. The loops simply
never execute, because `for (intcs i = 0; i < length; ++i)` is false at once.

**The repair target already exists**, with the right exception type, the right message and the right
parameter name, in the four XXH types. This ticket does not invent a rule; it deletes a divergence.

### 4.3 SR-AUD-262 — confirmed, and this review states exactly what it can and cannot prove

`ReadUInt32LE`, `ReadUInt64LE` and `WriteUInt64LE` in `XxHash3Shared.cpp` are `memcpy` of a native
integer, and XxHash32/64 load their lanes the same way. On a big-endian target every one of them is
byte-swapped relative to the xxHash specification, so the module would compute **different hashes
from the published algorithm** — silently, and only there.

**What this review CAN prove, here, today:**

- The module's current output **is** the published value on this little-endian host. Measured
  against the algorithms' own published check values (§6.2), not against a reference checkout.
- The helpers are name/behaviour divergent by inspection: a function called `ReadUInt32LE` that
  performs a native-order copy is wrong on some target by construction.
- A portable rewrite can be proved **value-preserving on this host** by the same published vectors,
  and **byte-order-correct** by testing the helper directly against a fixed byte pattern — a
  host-independent assertion, which is precisely what the per-file report asks for
  (*"a portable byte-swapped helper seam rather than host-only vectors"*).

**What it CANNOT prove:** that the repaired module produces correct hashes when *executed* on a
big-endian machine. There is no such machine here, and there is no cross-run in this repository's
CI. **#2143 is scoped to the provable half** and says so; it does not claim a big-endian guarantee
it cannot demonstrate. This is a limitation of the environment, not a deferred `/rv` question — no
reference checkout would help.

---

## 5. Structural root-cause families

- **IH-A — a span was replaced by a raw pointer and the contract the replacement needs was never
  written.** SR-AUD-260. The parent of the other two.
- **IH-B — a signed length is consumed by a loop condition before any check can see it.** SR-AUD-261.
  **This is NS-D's shape from the `net-sockets` review** (a sign consumed before it can be
  validated), in a different module. Not a promotion — recorded in §8.
- **IH-C — a byte order is implied by a function's name and not implemented by its body.**
  SR-AUD-262.
- **IH-D — the base class validates the length of a destination and not its pointer.** SR-AUD-260's
  destination half. Called out separately because the fix has **one** site, not seven.

---

## 6. Post-audit observations (no `SR-AUD-*` identifier)

### 6.1 Recorded, and deliberately not ticketed

- **`README.md` documents none of the raw-pointer contract.** The audit's README report says so and
  files no finding. Folded into **#2144**'s documentation scope rather than ticketed separately.
- **`GetCurrentHash(dst, 1)` throws while `GetCurrentHash(nullptr, 16)` crashes.** Not a separate
  defect — it is the *evidence* for §4.1's third correction, and it is repaired by #2141.

### 6.2 Measured positives, recorded so they are not re-investigated

**The published check values all match on this host** — the strongest statement available about
this module, and it is independent of `/rv`:

| Input | Measured | Published |
|---|---|---|
| `Adler32("abc")` | `024d0127` | matches |
| `Crc32("123456789")` | `cbf43926` | the canonical CRC-32 check value |
| `Crc64("123456789")` | `6c40df5f0b497347` | the CRC-64/ECMA-182 check value |
| `XxHash32("")` | `02cc5d05` | the official empty-input value |
| `XxHash32("abc")` | `32d153ff` | official |
| `XxHash64("abc")` | `44bc2cf5ad770999` | official |
| `XxHash3("abc")` | `78af5f94892f3950` | official |
| `XxHash128("abc")` | `06b05ab6733a618578af5f94892f3950` | official |

- **Zero-length null input is accepted on all seven types** — correct, and a required control (§4.1).
- **The short-destination path is correct on all seven types**, with the right parameter name.
- **No `std::` exception escaped any door** — every rejection was a `System::` exception.
- **Nothing in this module holds a descriptor, a lock, or a borrowed pointer**, so `/proc/self/fd`
  accounting and CCF-019 have no subject here. Recorded so a future reader does not assume they
  were checked and found clean — they have **no subject**, which is different.

---

## 7. Source / ABI / layout / vtable / `noexcept` consequences

| Ticket | Source | ABI / layout | vtable | `noexcept` | Component graph |
|---|---|---|---|---|---|
| **#2141** | narrows: a null pointer stops being dereferenced and starts throwing | **none** | none | none | none |
| **#2142** | narrows: a negative length stops meaning "empty" and starts throwing | **none** | none | none | none |
| **#2143** | **none observable on a little-endian host** — the published values are unchanged | none | none | none | none |
| **#2144** | documentation and pins | none | none | none | none |

**No ticket in this module needs an object-layout, vtable, signature or component-graph change.**
The graph stays at **41 modules / 92 edges**. This is the first namespace in several reviews with
**no** gated item.

---

## 8. CCF mapping

- **CCF-019** — **no member.** Nothing here borrows a handle or captures an owner pointer.
- **CCF-022** — **no member.** Nothing here has a lifecycle state to record.
- **CCF-021** — **no member.** No protocol field is serialized.
- **CCF-020** (*raw polymorphic output parameters erase the validation information public contracts
  require*) — **worth a reader's attention and NOT claimed as a member.** #2141's destination half
  is a raw output pointer whose validation is incomplete, which rhymes; but CCF-020's subject is
  polymorphic `object`-shaped output, not a byte buffer. Recorded as a resemblance, not a claim.
- **A cross-module shape, recorded and not promoted:** SR-AUD-261's cause is *a sign consumed by a
  loop or ternary before any range check can see it* — the same shape as `net-sockets`' **NS-D**
  (SR-AUD-264, #2135). Two sites in two modules is not a family, and **this review mints nothing**;
  the authority question that blocks CCF-021 (#2131) and CCF-022 (#2109) is unresolved and adding a
  third candidate would not help it. Recorded here so that a future reviewer with a third site has
  the first two written down.

**CCF-012 and CCF-019 are NOT marked closed by this review.**

---

## 9. Parsing, serialization, mutation, resource and thread-safety consequences

- **Parsing.** None — this module consumes bytes, not text.
- **Serialization.** The output byte order of `GetCurrentHash` is part of each algorithm's
  published contract and is **already correct** (§6.2's vectors include the destination forms
  indirectly through the one-shot API). #2143 must not change it; §11 makes that a pin.
- **Resources.** None. No descriptor, no allocation beyond `std::vector` returns, no lock.
- **Mutation.** `Append` mutates hasher state. **A rejected `Append` must not mutate it** — the
  audit report asks for exactly this (*"no-state-change-on-rejected append"*) and it is a required
  test for both #2141 and #2142, because a guard placed after the state update would pass a naive
  "it throws" test.
- **Thread safety.** Nothing here is documented thread-safe and nothing is. The parameter-set
  singletons (`getCrc32Property()` and friends) are shared `shared_ptr`s to **immutable** tables,
  and `Update` is `const`, so they are safe to share; recorded because it is not obvious.

---

## 10. Deferred evidence — what `/rv` would settle

**Almost nothing, and that is why this unit was chosen.** Two small items:

- Whether .NET's byte-array overloads throw `ArgumentNullException` (this review's assumption, from
  the per-file report's *"call `ArgumentNullException.ThrowIfNull`"*) or something else. **#2141
  follows the report and records the exception type as this port's choice.** No deferred ticket is
  created, because the raw-pointer surface does not exist in .NET at all — there is no reference
  behaviour to match, only a contract to choose.
- Whether .NET's `Adler32` is public at all (it is `internal` in some versions). Irrelevant to every
  repair here; recorded only so nobody re-derives it.

**No deferred-verification ticket is created by this review.** That is unusual and is the point.

---

## 11. Test matrix

| Ticket | Required cases |
|---|---|
| **#2141** | For **all seven** types **and** both parameter sets: null source with positive length → throws, `paramName == "source"`; null destination with sufficient length → throws, `paramName == "destination"`; at **every** door (`oneShot`, `Hash` vector, `Hash` destination, `TryHash`, `Append`, `GetCurrentHash`, `TryGetCurrentHash`, `GetHashAndReset`, `TryGetHashAndReset`, `ParameterSet::Update`). **CONTROLS:** null + zero length still **accepted**; a short destination still throws its existing *"Destination is too short."*; the published vectors of §6.2 unchanged. **STATE:** a rejected `Append` leaves the hash unchanged |
| **#2142** | `Adler32`/`Crc32`/`Crc64` and both parameter sets: length ∈ {−1, −2, `INT_MIN`} → `ArgumentOutOfRangeException`, `paramName == "length"`, **the same message the XXH types already use**. **CONTROLS:** length 0 and positive lengths unchanged; the XXH types' existing behaviour unchanged (**pin**); §6.2's vectors unchanged. **STATE:** a rejected `Append` leaves the hash unchanged |
| **#2143** | A **host-independent** helper test: a fixed byte pattern → its little-endian value, asserted by construction rather than by `memcpy`. Every published vector in §6.2 unchanged (**pin**). The destination byte order of `GetCurrentHash` unchanged (**pin**) |
| **#2144** | §6.2's measured positives pinned, including the eight published vectors and the accepted-null-zero-length control; the raw-pointer contract documented in the headers and `README.md` |

## 12. Sanitizer and direct-resource matrix

| Tool | Applicable here? |
|---|---|
| **ASan** | **yes** — the null dereferences of #2141, and the buffer arithmetic of every hash loop |
| **UBSan** | **yes, and it is the finding's own instrument** — the per-file reports cite UBSan-confirmed null loads at `Adler32.cpp:24`, `Crc32ParameterSet.cpp:69` and `Crc64ParameterSet.cpp:76` |
| **LSan** | marginal — this module allocates only `std::vector` returns |
| **TSan** | **no subject** — no shared mutable state |
| **`/proc/self/fd`** | **no subject** — this module owns no descriptor |
| **fork-per-case probing** | **required**, and used: a defect whose symptom is SIGSEGV cannot be enumerated in-process |

---

## 13. Bounded tickets and recommended order

```
#2142  SR-AUD-261  a negative length is a successful empty
                   operation in Adler/CRC                     (P2, S) ── FIRST
#2141  SR-AUD-260  every raw pointer door dereferences a
                   null buffer -- source AND destination      (P1, M) ── SECOND
#2143  SR-AUD-262  the "LE" helpers are native-order          (P2, M) ── THIRD
#2144  documentation and measured-positive pins               (P3, S) ── LAST
```

**Recommended order: #2142, then #2141, then #2143, then #2144.** #2142 first even though #2141
carries the `high`, and the reason is structural rather than a preference: #2142's repair is to
adopt a guard **that already exists in this module**, which is where the shared validation helper
naturally comes from; #2141 then extends that same helper to the pointer axis and to the base
class's five destination doors. Doing them in the other order writes the helper twice.

**They are deliberately NOT one ticket**, despite touching the same lines: different findings,
different exception types (`ArgumentNullException` vs `ArgumentOutOfRangeException`), different door
sets (#2141 includes the destination doors, #2142 does not), and different mutation targets. A
combined ticket would make "which half did the mutation kill" unanswerable.

## 14. Compatible versus blocked or deferred

| Ticket | Compatible? | Why |
|---|---|---|
| #2141 | **yes, with a documented narrowing** | a null pointer stops being dereferenced and starts throwing |
| #2142 | **yes, with a documented narrowing** | a negative length stops meaning "empty" |
| #2143 | **yes** — no observable change on a little-endian host | scoped to what is provable here |
| #2144 | **yes** — documentation and pins only |

**All four are compatible. Nothing in this module is blocked, gated, or deferred.**

## 15. Exclusions

- `System::Security::Cryptography`'s hash algorithms — different component, different contract.
- The absent async append surface — a recorded adaptation, not a defect.
- `miniz`/`zlib` — `modules/io-compression`'s dependency.
- Big-endian **runtime** verification — no such host and no cross-run in this repository's CI (§4.3).
- CNA and mobile-eggbert — not inspected; **#1773 stays blocked**.

## 16. Completion criteria

This review (#2140) is complete when this document exists, each of the three open findings has
exactly one disposition in §4, each post-audit observation carries a ticket or an explicit
"recorded, not ticketed", and §13's tickets are in `plan.sqlite3`. **It is complete on those terms
and remediates nothing by itself.**

`modules/io-hashing` is closed when #2141, #2142, #2143 and #2144 are `done` and SR-AUD-260, 261 and
262 are `remediated`. **There is no gated remainder** — when those four land, the module is finished,
not "finished except for".

## 17. Implementation record

Appended as tickets land, so the difference between what this review predicted and what
implementation measured stays visible.
