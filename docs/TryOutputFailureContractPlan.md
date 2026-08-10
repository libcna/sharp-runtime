<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Try-style failure-output contract — CCF-014 plan

*Authored 2026-07-30 by the autonomous remediation batch on branch
`feature/remediation-batch-ccf014-ccf016`, immediately after the CCF-011
empty-callable family closed (#1866–#1870). This is the durable, evidence-based
plan for **CCF-014 — "false Try-style calls must not leak stale output into the
next control path"** (`audit/AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-014). Two
findings, both `confirmed`: SR-AUD-075 (`SequenceReader<T>`) and SR-AUD-085
(`Utf8Parser`).*

*Every current-behaviour statement was **measured**, not recalled, by
`build-probe/1871_try_output_probe.cpp` compiled against the shipped headers on
2026-07-30; raw output in `build-probe/1871_prefix.log`. Every reference
statement was read from the current local .NET sources
(`/rv/tmp/runtime/src/libraries/System.Memory/src/System/Buffers/SequenceReader.cs`,
`SequenceReader.Search.cs`, and
`System.Private.CoreLib/src/System/Buffers/Text/Utf8Parser/*.cs`), not from
memory. **Both findings still reproduce; neither is remediated.***

**This document creates no `SR-AUD-*` identifier** (audit numbering is frozen at
364) and **marks no finding remediated**.

---

## 1. Exact family scope

**In scope.** Every public Try-style member of the `buffers` module whose
**value output** is left holding the caller's previous data when the call
returns `false`, where the .NET counterpart assigns `default`. Measured: **11
public entries** across two files.

**In scope but already correct, and therefore only *pinned* by tests, never
edited:** the five structural siblings in the same module that already implement
the intended contract (§4.3). They are the strongest evidence that this is a
localised inconsistency rather than a module-wide design choice.

**Not in scope.** The other ~240 `Try*` methods elsewhere in the repository.
The audit produced no cross-cutting evidence for them, and §15 records why a
blanket sweep is explicitly refused here.

---

## 2. Findings and affected symbols

| Finding | Sev | Status | Owner | Entries | One-line defect |
|---|---|---|---|---|---|
| **SR-AUD-075** | medium | confirmed | `modules/buffers/include/System/Buffers/SequenceReader.hpp` | 2 | `TryRead(T&)` / `TryPeek(T&)` return `false` at end of sequence without touching the caller's value. |
| **SR-AUD-085** | medium | confirmed | `modules/buffers/include/System/Buffers/Text/Utf8Parser.hpp` | 9 | Every `TryParse` overload sets `bytesConsumed = 0` on failure but leaves `value` holding the caller's previous data. |

Tests: `modules/buffers/tests/System/Buffers/Batch6BuffersTests.cpp`
(`SequenceReaderTests`), `modules/buffers/tests/System/Buffers/Utf8ParserTests.cpp`.
Test target: `SharpRuntimeTests_Buffers`.

---

## 3. Complete public-entry inventory

### 3.1 `SequenceReader<T>` — 2 defective entries

| Entry (line) | Outputs | Failure condition | Today, on failure | .NET |
|---|---|---|---|---|
| `bool TryRead(T& value)` (77) | `value`; reader position | `End` | `value` **untouched**; position unchanged | `value = default` (`SequenceReader.cs:192-198`) |
| `bool TryPeek(T& value) const` (157) | `value` | `End` | `value` **untouched** | `value = default` (`SequenceReader.cs:114-126`) |

### 3.2 `Utf8Parser` — 9 defective entries

All nine share the signature shape
`static bool TryParse(ReadOnlySpan<uint8_t> source, T& value, intcs& bytesConsumed, char standardFormat = '\0')`.

| # | `T` | Line | Failure classes reached |
|---|---|---|---|
| 1 | `bool` | 53 | not a boolean token; empty source |
| 2 | `uint8_t` | 86 | core parse failure; **width overflow after a successful core parse** |
| 3 | `int8_t` | 100 | core parse failure; width overflow/underflow |
| 4 | `uint16_t` | 114 | core parse failure; width overflow |
| 5 | `int16_t` | 128 | core parse failure; width overflow/underflow |
| 6 | `uint32_t` | 142 | core parse failure; width overflow |
| 7 | `int32_t` | 156 | core parse failure; width overflow/underflow |
| 8 | `uint64_t` | 170 | core parse failure (incl. 64-bit accumulator overflow) |
| 9 | `int64_t` | 184 | core parse failure (incl. magnitude overflow) |

Every one of the four format grammars reaches a failure exit: `G`/`D`/`R`
(`tryParseInt`/`tryParseUInt`), `X` (`tryParseHex`), `N` (`tryParseGrouped`).
Measured separately (§4.1) — the defect is grammar-independent.

### 3.3 Same-file entries deliberately **not** in the family

| Entry | Why not |
|---|---|
| `SequenceReader::TryReadTo(std::vector<T>&, …)` | Already correct: clears `result` at entry *and* again on the not-found path, and restores the position. Measured `size=0 consumed=0`. .NET sets `span = default` (`SequenceReader.Search.cs:33-43`). Parity. |
| `SequenceReader::TryAdvancePast(const T&)` | No output parameter. |
| `SequenceReader::IsNext` (both overloads) | Not `Try*`-shaped; no output parameter. |

---

## 4. Current behaviour matrix (measured 2026-07-30)

Sentinels chosen so a stale value is distinguishable from a correct default:
value `42` / `99` / `true` / `"stale"`, counter `7`.

### 4.1 Defective — the sentinel survives

| Probe case | Return | `value` after | `bytesConsumed` after |
|---|---|---|---|
| `seqreader.tryread.atend` | false | **42** | n/a (position 0, correct) |
| `seqreader.tryread.exhausted` | false | **42** | n/a (position 1, correctly *not* rewound) |
| `seqreader.trypeek.atend` | false | **99** | n/a |
| `seqreader.tryread.string` | false | **`"stale"`** | n/a |
| `utf8parser.bool.invalid` / `.empty` | false | **true** | 0 ✓ |
| `utf8parser.int32.notdigit` / `.empty` | false | **42** | 0 ✓ |
| `utf8parser.int32.overflow` | false | **42** | 0 ✓ |
| `utf8parser.uint8.overflow`, `int8.underflow`, `uint16.overflow`, `int16.overflow`, `uint32.overflow`, `uint64.overflow`, `int64.overflow` | false | **42** | 0 ✓ |
| `utf8parser.int32.hex.invalid` (`X`) | false | **42** | 0 ✓ |
| `utf8parser.int32.grouped.fraction` (`N`) | false | **42** | 0 ✓ |

### 4.2 The throwing path — **already correct, must not be "fixed"**

| Probe case | Result |
|---|---|
| `utf8parser.int32.badformat.throws` | threw; `value=42`, `bytesConsumed=7` — both untouched |
| `utf8parser.bool.badformat.throws` | threw; `value=true`, `bytesConsumed=7` — both untouched |

.NET does exactly the same. `ParserHelpers.TryParseThrowFormatException<T>` calls
`Unsafe.SkipInit(out value); Unsafe.SkipInit(out bytesConsumed);` **specifically
to bypass C#'s definite-assignment rule before throwing**, so the reference
leaves both outputs unwritten on the `FormatException` path. Normalising them
here would be a *divergence*, not a repair. §8 makes this a binding rule.

### 4.3 Structural siblings — already correct, pinned but not edited

| Probe case | Result | Contract already implemented |
|---|---|---|
| `sibling.seqreaderext.tryreadle32` | false, `value=0` | wrapper assigns `value = 0` on false |
| `sibling.binaryprimitives.tryread32` | false, `value=0` | every `TryRead*Endian` assigns `value = 0` |
| `sibling.utf8formatter.tryformat.short` | false, `bytesWritten=0` | counter normalised; destination deliberately not |
| `sibling.standardformat.tryparse.invalid` | false, default `StandardFormat` | initialised at entry |
| `sibling.readonlysequence.tryget.atend` | false, empty memory | assigns an empty `ReadOnlyMemory<T>` |

---

## 5. Reference behaviour matrix

| API | Failure output | Source |
|---|---|---|
| `SequenceReader<T>.TryRead(out T)` | `value = default` | `SequenceReader.cs:192-198` |
| `SequenceReader<T>.TryPeek(out T)` | `value = default` | `SequenceReader.cs:114-126` |
| `SequenceReader<T>.TryPeek(long, out T)` | `value = default` (also for a negative offset it *throws* `ArgumentOutOfRangeException` first) | `SequenceReader.cs:134-144` |
| `SequenceReader<T>.TryReadTo(out ReadOnlySpan<T>, …)` | `span = default` | `SequenceReader.Search.cs:33-43` |
| `Utf8Parser.TryParse(…, out bool, out int, char)` | `bytesConsumed = 0; value = default` | `Utf8Parser.Boolean.cs:52-54` |
| `Utf8Parser.TryParse(…, out sbyte/short/int/long, …)` `D` | `FalseExit: bytesConsumed = default; value = default` | `Utf8Parser.Integer.Signed.D.cs:79-83` and siblings |
| … `N` grammar | same `FalseExit` block | `Utf8Parser.Integer.Signed.N.cs:89-92, 181-184, 276-279, 371-374` |
| … `X` grammar | `bytesConsumed = 0; value = default` at five exits | `Utf8Parser.Integer.Unsigned.X.cs:12-13, 26-27, 77-78, 94-95, 108-109` |
| … invalid format specifier | **throws**; both outputs left unwritten via `Unsafe.SkipInit` | `ParserHelpers.cs:59-70` |

Answering §"reference comparison" point by point:

- **Result on each failure class:** always `false`; there is no richer status.
- **Output values:** always `default(T)` and `0`.
- **Initialised at entry or committed after success?** *Committed after success.*
  .NET writes the outputs only at `Done:` or at `FalseExit:`, never speculatively.
- **Validation order:** the format specifier is validated **first** and throws;
  everything else returns `false`.
- **Null/invalid arguments:** only the format specifier throws. An empty source
  is an ordinary `false`.
- **Partial consumption:** intentional and preserved — a successful parse
  consumes only the token, leaving trailing bytes (`"42abc"` → `true`, 2). A
  *failed* parse consumes nothing (`bytesConsumed = 0`).
- **Overflow vs format failure:** **not distinguishable.** Both return `false`
  with `bytesConsumed = 0`. The port matches; no new status is introduced.
- **Contractual or incidental?** Contractual. It is stated in the XML doc of
  every overload ("false if the string was not syntactically valid or an
  overflow or underflow occurred") and implemented at a single labelled
  `FalseExit` per parser, which is a deliberate structure, not an accident.

---

## 6. Common root cause

Both files translate a C# `out` parameter into a C++ **reference parameter** and
lose the language guarantee that came with it. In C#, `out T value` is
*definitely assigned on every path that returns*, enforced by the compiler; the
`value = default` lines in the reference are not defensive style, they are what
the compiler requires. A C++ `T&` carries no such rule, so the port kept the
`return false` and silently dropped the assignment that made it safe.

The consequence is that a *checked* failure is indistinguishable from a stale
success for any caller that reuses output storage — the exact loop shape these
APIs are designed for:

```cpp
int v = 0;
while (reader.TryRead(v)) { use(v); }   // fine
// but:
int v;
if (!reader.TryRead(v)) { /* v still holds the PREVIOUS iteration's element */ }
```

Two facts confirm this is a localised omission rather than a considered design:

1. **The wrapper compensates for the core.** `SequenceReaderExtensions::
   TryReadLittleEndian` assigns `value = 0` on false — and reaches
   `SequenceReader::TryRead` internally through `detail::tryReadBytes`, which
   *also* assigns `value = T{}` before returning false. Two layers implement the
   contract that the layer between them does not.
2. **Every other Try-style entry in the same module already implements it**
   (§4.3) — five independent surfaces, three different authors' idioms.

### Root-cause classification against the checklist

| # | Class | Present? |
|---|---|---|
| 1 | output unchanged where .NET writes a default | **Yes — all 11 entries.** The whole family. |
| 2 | output contains a partial value | **No.** `tryParseIntegerCore` accumulates into locals (`sv`/`uv`/`n`) and the overload copies to `value` only after the range check passes, so a failed parse never publishes a partial value. |
| 3 | stale count/index from caller input | **No.** `bytesConsumed = 0` on every non-throwing failure; measured on all 16 failing parser cases. The reader's position is likewise correct (not rewound after a successful earlier read). |
| 4 | one overload clears output while another does not | **Yes, across the module** — `BinaryPrimitives`/`SequenceReaderExtensions` clear, `SequenceReader`/`Utf8Parser` do not. Not *within* either defective file: all 9 parser overloads are uniformly wrong. |
| 5 | wrapper failure differs from core | **Yes, inverted** — the wrapper is correct and the core is not (see 1. above). |
| 6 | validation failure differs from parse failure | **No.** Both leave `value` stale; both set the counter to 0. |
| 7 | success reported with stale secondary outputs | **No.** Every success path writes both outputs. |
| 8 | exception path bypasses output normalisation | **Yes — and this matches .NET** (§4.2). Parity, not a defect. Recorded so a future reader does not "fix" it. |
| 9 | output aliases input | **No.** `value` is a distinct reference; the parser never writes through `source`. |
| 10 | documentation wrong, implementation right | **Partly, inverted.** `Utf8Parser`'s class doc-comment states the `bytesConsumed = 0` half of the contract and is *silent* about `value`; `SequenceReader::TryRead`'s `@param value` says "Receives the next element if successful" and does not state the failure state at all. Both are incomplete rather than wrong, and both must be completed alongside the code. |

---

## 7. Shared helper versus per-method repair

| File | Repair | Why |
|---|---|---|
| `Utf8Parser.hpp` | **One shared private helper.** `template<typename T> static bool fail(T& value, intcs& bytesConsumed) { value = T{}; bytesConsumed = 0; return false; }`, and every one of the 10 `return false` failure exits in the 9 overloads becomes `return fail(value, bytesConsumed);`. | There are ten identical failure exits with an identical obligation. A helper makes the obligation structural: a future overload that writes `return false;` by hand is visibly different from its neighbours, and the counter and the value can never again be normalised independently — which is precisely how this defect arose. |
| `SequenceReader.hpp` | **Per-method, inline.** One failure branch each. | Two sites, one branch apiece, in a class with no other failure exits. A helper would be indirection without a structural gain, and `TryPeek` is `const` while `TryRead` is not, so they cannot share a mutating helper anyway. |

Rejected: initialising `value` at entry in `Utf8Parser`. It would write the
caller's storage even on the throwing path, diverging from .NET's deliberate
`Unsafe.SkipInit`, and would cost a store on every successful parse.

Rejected: a richer internal status distinguishing overflow from malformed input.
.NET does not distinguish them at this boundary (§5), and inventing a
distinction would be a public-contract change with no reference to match.

---

## 8. Validation and exception ordering (binding rules)

1. The format-specifier check runs **first** and throws `FormatException`.
   **On that path neither output may be written** — matching
   `ParserHelpers.TryParseThrowFormatException`'s `Unsafe.SkipInit`. A permanent
   test pins that a caller sentinel survives the throw.
2. Every **non-throwing** failure writes *both* outputs: `value = T{}` and
   `bytesConsumed = 0`.
3. Every success writes both outputs and consumes only the parsed token.
4. `SequenceReader`'s position is **never** modified by a failing `TryRead` /
   `TryPeek`, before or after the change.

---

## 9. Output commit / rollback strategy

Commit-on-success is already the shape of both implementations and is preserved:

- `Utf8Parser` accumulates into locals and publishes to `value` only after the
  width range check. The change adds the *failure* half — publish `T{}` — so the
  output is now written on exactly one of two mutually exclusive paths.
- `SequenceReader::TryRead` reads the element, then advances. The failure branch
  is reached before either, so there is nothing to roll back.
- `TryReadTo` already restores `consumed_` and clears `result` on failure; it is
  the in-file precedent for the rule and is unchanged.

There is no partial-write window to close in either file, which is why this
family needs no rollback machinery — only an assignment.

---

## 10. Aliasing and span behaviour

- `Utf8Parser`'s `source` is a `ReadOnlySpan<uint8_t>`; `value` is a reference to
  a `bool`/integer. They cannot alias in any well-defined program (the span's
  element type is `uint8_t` and the output is written only through the typed
  reference).
- `SequenceReader`'s `value` could in principle alias an element of the
  underlying sequence via a caller-held reference. The failure branch does not
  read the sequence at all, so `value = T{}` is safe regardless.
- **No destination span is ever reset.** `Utf8Formatter::TryFormat` (a sibling,
  §4.3) leaves `destination` untouched on failure and only zeroes `bytesWritten`;
  .NET does the same. This family does not change that and must not be read as
  requiring it.

---

## 11. Source / ABI / layout / `noexcept` matrix

| Type | Source | ABI / mangling | Layout | `noexcept` | Semantics |
|---|---|---|---|---|---|
| `SequenceReader<T>` | compatible **with one recorded new requirement on `T`** (below) | unchanged — member function templates of a class template, no mangled symbol changes | unchanged — no data member added, removed, reordered or retyped | unchanged (neither entry is `noexcept`) | failing `TryRead`/`TryPeek` now write `T{}` |
| `Utf8Parser` | compatible | unchanged — all entries are `static` members of a non-template class, signatures identical | n/a — `Utf8Parser() = delete`, no data members | unchanged (no entry is `noexcept`) | failing `TryParse` now writes `T{}` |

**The one recorded requirement.** `value = T{}` requires `T` to be
value-initialisable, which `SequenceReader<T>::TryRead`/`TryPeek` did not
previously require. This is **not** a narrowing of the reference contract: .NET
declares `SequenceReader<T> where T : unmanaged`, and every `unmanaged` type has
a `default`. Value-initialisability is strictly *weaker* than `unmanaged`, so any
`T` that satisfies the reference's own constraint satisfies this one. The port
does not enforce `unmanaged`, so a hypothetical `SequenceReader<NonDefaultConstructible>`
that calls `TryRead` would newly fail to compile; member functions of a class
template are instantiated only when used, so every other use is unaffected. A
permanent test instantiates `SequenceReader<std::string>` — a type far outside
`unmanaged` — to show the requirement is genuinely weak.

No approval trigger is reached: no signature, `noexcept`, virtual, vtable,
layout, calling convention, grammar, formatted output, exception taxonomy or
numerical behaviour changes. The single observable change is that a `false`
return now leaves a defined value where it previously left the caller's own
data — which is the documented reference behaviour, and which no correct caller
could have depended on (the value was, by construction, whatever the caller last
put there).

---

## 12. Implementation dependency order

The two files are independent; the order is chosen so the shared contract is
written down before the larger surface adopts it.

1. **#1872 — both files, one ticket.** `SequenceReader` first (2 entries, the
   contract in its simplest form), then `Utf8Parser`'s shared `fail()` helper
   (9 entries, 10 exits). Deliberately **not** split into two tickets: CCF-014's
   whole point is that these must not be patched as independent one-offs, and a
   single ticket keeps the contract, the doc-comments and the tests in one
   reviewable change.

CCF-014 closes when SR-AUD-075 and SR-AUD-085 are both `remediated`.

---

## 13. Permanent test matrix

Add-only; no existing assertion weakened, skipped, deleted or recategorised.
Every test prepopulates each output with a sentinel that no correct result can
produce.

| Axis | Cases |
|---|---|
| **Success** | every overload; exact value; exact `bytesConsumed`; trailing bytes left unconsumed |
| **Empty input** | `TryParse("")` for bool and each integer width; `TryRead`/`TryPeek` on an empty sequence |
| **Invalid input** | non-digit; `"no"`; hex non-hex; `N` with a non-zero fraction |
| **Overflow** | each width's first overflowing decimal literal, and the signed widths' underflow — i.e. **failure after a successful core parse**, the case a naive "reset only when the core fails" fix would miss |
| **Insufficient destination** | n/a for parsers; the sibling `Utf8Formatter` case is pinned in §4.3's regression test, not changed |
| **Invalid format specifier** | `FormatException` thrown **and** both outputs still holding the caller's sentinel |
| **Failure before processing** | empty source, `End` reader |
| **Failure after partial processing** | `"9999999999"` into `int32_t`; `"999"` into `uint8_t` |
| **Caller sentinel** | every failing case initialises `value` to 42 / 99 / `true` / `"stale"` and `bytesConsumed` to 7 |
| **Aliasing / overlap** | not supported by these signatures; recorded as N/A rather than silently skipped |
| **Reader position** | unchanged by a failing `TryRead`/`TryPeek`, including after a successful earlier read |
| **Non-trivial `T`** | `SequenceReader<std::string>`: failing `TryRead` yields an empty string, not the previous one |
| **Siblings pinned** | one regression test asserting the five already-correct surfaces (§4.3) keep their contract, so a future edit cannot silently regress them |

**Mutation check.** Reverting any single `value = T{}` / `fail(...)` must fail at
least one permanent test. This is verified by re-running the suite against a
deliberately reverted header before the ticket closes, and the result is recorded
in the ticket notes — assertion coverage that is never mutation-checked is how
`ParseInvalidNotDigit_BytesConsumedIsZero` came to initialise its value to 99 and
then assert only the cursor.

---

## 14. Sanitizer strategy

| Sanitizer | Relevance | Plan |
|---|---|---|
| **ASan** | Moderate. `Utf8Parser` walks a raw `const uint8_t*` from the span, and `SequenceReader` indexes `segment_`. The change adds a store through an existing reference, but the overflow/underflow cases drive the pointer walks to their limits. | Compile the probe with `-fsanitize=address,undefined` and run every failure case, including the 23-digit overflow inputs. |
| **UBSan** | Moderate. The parser's accumulators, the `X` grammar's shifts, and the signed-width casts are exactly the arithmetic CCF-004 already hardened here (SR-AUD-084). A regression would show up on the overflow inputs. | Same run. |
| **LSan** | Low but non-zero: the `SequenceReader<std::string>` case allocates, and a failing `TryRead` now assigns an empty string over it. | Same run (LeakSanitizer is on by default under ASan), with the string case looped. |
| **TSan** | **Not applicable.** No shared mutable state, atomic, lock or cache is introduced or touched. Recorded as an explicit exclusion, not silently skipped. |

**Freshness rule.** Both files are header-only, so the probe must be *compiled
with* the sanitizer flags — that recompiles the changed inline code with
instrumentation and removes any stale-archive risk. `build-asan` is not needed
and must not be assumed fresh.

**A sanitizer-clean result does not establish output semantics.** The contract is
proved only by the exact-sentinel assertions in §13.

---

## 15. Performance implications

One store per failing call: `value = T{}` on a `bool`/integer, or a
`std::string::operator=` on a non-trivial `T`. Failing calls already return
early, so the cost lands only on the path that was doing nothing.

Successful calls are **unchanged**: the helper is on the failure exit only, and
`Utf8Parser` still writes `value` exactly once. Initialising at entry was
rejected in §7 partly for this reason — it would have added a store to every
successful parse.

No allocation is added on any path (`T{}` for a `std::string` constructs an empty
string, which does not allocate in any mainstream implementation). No new header
is included; no module edge changes. The graph stays at 41 modules / 91 edges.

---

## 16. Explicit exclusions

1. **The other ~240 `Try*` methods in the repository** (`Dictionary::TryGetValue`,
   `Int32::TryParse`, `ConcurrentQueue::TryDequeue`, `Base64::TryEncode…`, …).
   The audit produced **no** evidence that they share this defect, and a
   measured spot-check of the five closest structural siblings found all five
   already correct (§4.3). Extending the family on suspicion would be exactly
   the "broaden into every `Try*` API" the cross-cutting record warns against.
   If a future audit finds instances, they belong to a new ticket, not to
   SR-AUD-075/085.
2. **The `FormatException` path's outputs** — .NET deliberately leaves them
   unwritten (§4.2). Changing them would be a divergence.
3. **`Utf8Formatter::TryFormat`'s destination span** — .NET leaves it untouched
   on failure; only `bytesWritten` is normalised, which the port already does.
4. **Distinguishing overflow from malformed input** — .NET does not, at this
   boundary.
5. **`Utf8Parser`'s missing `Guid`/`DateTime`/`TimeSpan`/floating-point
   overloads** — a separately documented gap, not a CCF-014 instance.
6. **`SequenceReader`'s missing `TryPeek(long offset, out T)` overload** — the
   port does not implement it. Adding it would be new API surface, not a
   failure-contract repair. Recorded here so the omission is deliberate.
7. **`SequenceReader<T>`'s absent `unmanaged` constraint** — the port is
   deliberately more permissive than the reference (§11); this plan does not
   introduce a constraint.

---

## 17. Completion criteria

CCF-014 is **closed** when all of the following hold:

1. SR-AUD-075 and SR-AUD-085 are `remediated` in
   `audit/AUDIT_FINDINGS_INDEX.md` and in their per-file reports, with §6/§4
   corrections appended and historical text preserved.
2. All 11 entries in §3 write `T{}` on every non-throwing failure, and the
   throwing path still writes neither output.
3. The post-fix run of `build-probe/1871_try_output_probe.cpp` shows **no
   surviving sentinel** on any non-throwing failure, and the two throwing cases
   still show `value=42 counter=7`.
4. The §13 matrix is landed, add-only, mutation-checked, with no test-count
   regression against the 14,444 floor.
5. ASan + UBSan + LSan clean per §14; TSan recorded as not applicable.
6. `cmake --build build --parallel 3` clean — zero errors, zero warnings.
7. `scripts/local_ci_check.sh build` passes; Doxygen within the 1,942 ceiling;
   graph 41/91; negative fixtures 9/66; version seams 2/18 — this family adds
   none of any.
8. `AUDIT_CROSS_CUTTING_FINDINGS.md` §CCF-014 gains a closure paragraph in the
   established form.

---

## 18. Ticket breakdown and status

| Ticket | Findings | Scope | Size | Status |
|---|---|---|---|---|
| **#1871** | — | This plan. Design only; no production change; no finding remediated. | S | todo → done |
| **#1872** | SR-AUD-075, SR-AUD-085 | `SequenceReader::TryRead`/`TryPeek` assign `T{}` on the end-of-sequence branch; `Utf8Parser` gains a shared private `fail(value, bytesConsumed)` used by all ten failure exits of its nine overloads. Doc-comments completed. Throwing path untouched. | M | todo |

### Implementation status

| Finding | Ticket | Status |
|---|---|---|
| SR-AUD-075 | #1872 | **`remediated`** (2026-07-30) |
| SR-AUD-085 | #1872 | **`remediated`** (2026-07-30) |

**CCF-014 is CLOSED (2026-07-30).** Both findings are `remediated` and every
completion criterion in §17 is met. The post-fix probe run
(`build-probe/1872_postfix_asan.log`) shows **no surviving sentinel** on any of
the 19 non-throwing failure cases, and the two throwing cases still report
`value=42 counter=7` — the parity §4.2 requires.

---

## 19. Post-audit application to date/time parsing — #1880 (2026-08-01)

This plan's closed CCF-014 convention resolves the recommendation that
`docs/DateTimeValidationBoundaryPlan.md` §20.2 intentionally left open. The
four date/time `TryParse` methods had lost the same C# `out` definite-assignment
rule: all returned `false` while retaining caller sentinels, whereas current
.NET assigns each value type's `MinValue`. Ticket #1880 applies the existing
failure-only commit rule to DateTime, DateTimeOffset, DateOnly and TimeOnly.

The repair is deliberately not a repository-wide `Try*` sweep. It covers the
four measured CCF2-E doors only; `Parse` wrappers keep their exception contract,
and successful parsers still publish one final value. Exact evidence, tests,
performance and compatibility consequences are recorded in the date-time plan
§23. No audit identifier is issued, and the frozen total remains 364.
