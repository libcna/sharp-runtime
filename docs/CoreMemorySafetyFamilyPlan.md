<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Core memory-safety family plan — SR-AUD-044, 045, 051, 054, 067

**Scope:** exactly five audit findings, all owned by `modules/core`, all `high`, none
blocked, none previously claimed by a live ticket. This is **one bounded family of
`modules/core`, not a `modules/core` namespace review.** `modules/core` has **72 open
findings**; the other 67 are explicitly out of scope here and §16 records the measured
shortlist for the next family.

**No `SR-AUD-*` identifier is created by this plan.** Audit numbering stays frozen at
**364** (173 remediated / 136 confirmed / 55 confirmed(design-complete), recounted by
finding identifier on 2026-08-10 — a strict six-column regex under-counts, because
SR-AUD-029 carries a seventh column).

---

## 1. Exact five-finding scope

| Finding | Severity | Owning report | One-line claim (as audited) |
|---|---|---|---|
| **SR-AUD-044** | high | `Span.hpp.audit.md` (+ extensions in `Array.hpp`, `ArraySegment.hpp`) | Forward `std::copy` loses source data when source and destination overlap. |
| **SR-AUD-045** | high | `SpanSplitEnumerator.hpp.audit.md` | An empty exact sequence separator never advances the cursor — infinite enumeration. |
| **SR-AUD-051** | high | `Array.hpp.audit.md` (+ extension in `Buffer.hpp`) | Raw-pointer `Array::Copy` byte-copies arbitrary objects and accepts negative metadata; the generic `Buffer` vector templates do the same. |
| **SR-AUD-054** | high | `ArraySegment.hpp.audit.md` | Default-segment operations silently succeed or dereference null instead of throwing. |
| **SR-AUD-067** | high | `Buffer.hpp.audit.md` | Raw-pointer `BlockCopy` turns negative offsets/count into an out-of-bounds `memmove`. |

---

## 2. All affected files

**Production (7 headers, all header-only — no `.cpp` body exists for any of them):**

- `modules/core/include/System/Span.hpp` (`Span<T>`, `ReadOnlySpan<T>`)
- `modules/core/include/System/Memory.hpp` (`Memory<T>`)
- `modules/core/include/System/ReadOnlyMemory.hpp` (`ReadOnlyMemory<T>`)
- `modules/core/include/System/MemoryExtensions.hpp` (static `CopyTo` ×2)
- `modules/core/include/System/Array.hpp` (`Copy` ×3, `ConstrainedCopy`)
- `modules/core/include/System/ArraySegment.hpp` (`ArraySegment<T>`)
- `modules/core/include/System/Buffer.hpp` (`Buffer`)
- **new:** `modules/core/include/System/detail/OverlapCopy.hpp` (the one shared helper)

**Tests:**
`modules/core/tests/System/CoreMemorySafetyTests.cpp` (new, this family's permanent
regressions), plus existing `ArrayTests.cpp`, `Batch11ArrayTests.cpp`, `BufferTests.cpp`,
`Batch13BufferTests.cpp`, `ArraySegmentNewTests.cpp`, `SpanTests`/`ReadOnlySpanTests`
(in `Batch3TypeTests.cpp` etc.), `SpanSplitEnumeratorTests.cpp`,
`MemoryExtensionsTests.cpp` — all re-run, none rewritten.

**Consumer fixture:** `test/consumer/core_buffer_trivially_copyable_negative.cpp` (new).

**Records:** the five owning audit reports, `audit/AUDIT_FINDINGS_INDEX.md`,
`audit/AUDIT_CROSS_CUTTING_FINDINGS.md`, `NEXT.md`, `plan.md`, `plan.sqlite3`.

---

## 3. Complete public-door inventory

Counted from the source, not from the audit text. **32 public doors** are implicated.

### 3.1 SR-AUD-044 — overlap-unsafe element copy (12 doors)

| # | Door | File:line | Overlap primitive |
|---|---|---|---|
| 1 | `Span<T>::CopyTo(Span<T>)` | `Span.hpp:189` | forward `std::copy` |
| 2 | `Span<T>::TryCopyTo(Span<T>)` | `Span.hpp:201` | forward `std::copy` |
| 3 | `ReadOnlySpan<T>::CopyTo(Span<T>)` | `Span.hpp:428` | forward `std::copy` |
| 4 | `ReadOnlySpan<T>::TryCopyTo(Span<T>)` | `Span.hpp:440` | forward `std::copy` |
| 5 | `Memory<T>::CopyTo(Memory<T>&)` | `Memory.hpp:189` | forward `std::copy` |
| 6 | `Memory<T>::TryCopyTo(Memory<T>&)` | `Memory.hpp:204` | forward `std::copy` |
| 7 | `MemoryExtensions::CopyTo(ReadOnlySpan<T>, Span<T>)` | `MemoryExtensions.hpp:440` | forward `std::copy` |
| 8 | `MemoryExtensions::CopyTo(Span<T>, Span<T>)` | `MemoryExtensions.hpp:446` | delegates to 7 |
| 9 | `Array::Copy(const vector<T>&, vector<T>&, intcs)` | `Array.hpp:121` | forward index loop |
| 10 | `Array::Copy(const vector<T>&, intcs, vector<T>&, intcs, intcs)` | `Array.hpp:136` | forward index loop |
| 11 | `ArraySegment<T>::CopyTo(vector<T>&, intcs)` | `ArraySegment.hpp:238` | forward `std::copy` |
| 12 | `ArraySegment<T>::CopyTo(ArraySegment<T>&)` | `ArraySegment.hpp:252` | forward `std::copy` |

Reached transitively, needing no separate repair: `ReadOnlyMemory<T>::CopyTo` /
`TryCopyTo` (→ 3/4), `ArraySegment<T>::CopyTo(vector<T>&)` (→ 11),
`Array::ConstrainedCopy` (→ 10).

### 3.2 SR-AUD-051 — unchecked byte copy over typed storage (6 doors)

| # | Door | File:line | Defect |
|---|---|---|---|
| 13 | `Array::Copy(const T*, intcs, T*, intcs, intcs)` | `Array.hpp:153` | `memcpy` for **any** `T`; negative `srcIndex`/`dstIndex`/`length` unchecked; `memcpy` is not overlap-safe |
| 14 | `Buffer::BlockCopy(const vector<T>&, …)` | `Buffer.hpp:108` | documented "primitive" but unconstrained |
| 15 | `Buffer::ByteLength(const vector<T>&)` | `Buffer.hpp:132` | ditto |
| 16 | `Buffer::GetByte(const vector<T>&, intcs)` | `Buffer.hpp:156` | ditto |
| 17 | `Buffer::SetByte(vector<T>&, intcs, bytecs)` | `Buffer.hpp:175` | ditto |
| — | `Array::ConstrainedCopy` | `Array.hpp:163` | vector-only; **not** a raw door |

### 3.3 SR-AUD-054 — default (null-owner) segment (12 doors)

| # | Door | Measured default-state behaviour (before) | .NET |
|---|---|---|---|
| 18 | `Slice(intcs)` | **UBSan null-reference bind + ASan SEGV** | `InvalidOperationException` |
| 19 | `Slice(intcs, intcs)` | **UBSan null-reference bind + ASan SEGV** | `InvalidOperationException` |
| 20 | `operator[](intcs)` | `ArgumentOutOfRangeException` | `InvalidOperationException` |
| 21 | `operator[](intcs) const` | `ArgumentOutOfRangeException` | `InvalidOperationException` |
| 22 | `CopyTo(vector<T>&, intcs)` | silent no-op | `InvalidOperationException` |
| 23 | `CopyTo(vector<T>&)` | silent no-op (→ 22) | `InvalidOperationException` |
| 24 | `CopyTo(ArraySegment<T>&)` | silent no-op | `InvalidOperationException` |
| 25 | `ToArray()` | empty vector | `InvalidOperationException` |
| 26 | `Contains(const T&)` | `false` | `InvalidOperationException` |
| 27 | `IndexOf(const T&)` | `-1` | `InvalidOperationException` |
| 28 | `begin()` | `nullptr` | `GetEnumerator()` throws |
| 29 | `end()` | `nullptr` | `GetEnumerator()` throws |

Doors **28/29** are the only ones that cannot be repaired compatibly — see §11 and §14
(ticket #2215).

Deliberately **not** guarded, matching .NET: `getArrayProperty`, `getOffsetProperty`,
`getCountProperty`, `Equals`, `operator==`, `operator!=`, `GetHashCode`, `getEmpty`.
.NET's `Array`/`Offset`/`Count`/`Equals`/`GetHashCode` do not call
`ThrowInvalidOperationIfDefault` either.

### 3.4 SR-AUD-067 — raw `Buffer::BlockCopy` (3 argument doors on 1 entry point)

| # | Argument | Measured before |
|---|---|---|
| 30 | `count < 0` | ASan `stack-buffer-overflow` in `memmove` |
| 31 | `srcOffset < 0` | ASan `stack-buffer-underflow` in `memmove` |
| 32 | `dstOffset < 0` | ASan `stack-buffer-overflow` in `memmove` |

### 3.5 SR-AUD-045 — `SpanSplitEnumerator` (1 entry point, 3 reachable shapes)

`SpanSplitEnumerator(source, std::vector<T>{}, /*treatAsAny=*/false)` — explicit
`MoveNext()` loop, range-`for`, and empty source. All three measured non-terminating.

---

## 4. Before sanitizer evidence (measured 2026-08-10)

Probe `build-probe/2210_probe_before.cpp`, built by `build-probe/2210_san_compile.sh`
with `-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g`, log
`build-probe/2210_before.log`.

**Instrumentation is over the production bodies themselves.** Every implicated body is a
header-only template or header-only static member, so the probe translation unit *is* the
production code; there is no implementation archive that could be stale. Proved by
`nm -C build-probe/2210_probe_before | grep __asan` → 21 `__asan_*` symbols including
`__asan_report_store1/4/8/16/_n` and `__asan_report_load1/4/8/16/_n`.
**Activation control:** `san.control_heap_overflow` reports
`heap-buffer-overflow … in sanControlHeapOverflow` in the same binary.

| Case | Defect class (exact) | Location |
|---|---|---|
| `san.raw_array_copy_negative_length` | **AddressSanitizer: unknown-crash** | `string_fortified.h:29 in memcpy` ← `Array.hpp:154` |
| `san.raw_array_copy_negative_index` | **stack-buffer-underflow** | `memintrinsics.inc:115 in memcpy` ← `Array.hpp:154` |
| `san.raw_array_copy_nontrivial` | **double-free** | `asan_new_delete.cpp:164 in operator delete` |
| `san.buffer_typed_vector_nontrivial` | **double-free** | `asan_new_delete.cpp:164 in operator delete` |
| `san.raw_blockcopy_negative_count` | **stack-buffer-overflow** | `string_fortified.h:36 in memmove` ← `Buffer.hpp:59` |
| `san.raw_blockcopy_negative_src` | **stack-buffer-underflow** | `memintrinsics.inc:98 in memmove` ← `Buffer.hpp:59` |
| `san.raw_blockcopy_negative_dst` | **stack-buffer-overflow** | `string_fortified.h:36 in memmove` ← `Buffer.hpp:59` |
| `san.segment_default_slice1` | **UBSan reference-bind-to-null** (`ArraySegment.hpp:180`) then **ASan SEGV** | `stl_vector.h:993 in vector::size()` |
| `san.segment_default_slice2` | **UBSan reference-bind-to-null** (`ArraySegment.hpp:198`) then **ASan SEGV** | `stl_vector.h:993 in vector::size()` |
| `obs.overlap_array_copy_raw` | **memcpy-param-overlap** | `memintrinsics.inc:115 in memcpy` ← `Array.hpp:154` |

Compile-time evidence recorded alongside: GCC 13.3 emits `-Wstringop-overflow=` /
`-Wstringop-overread` on the three raw `BlockCopy` negative-argument cases and on both raw
`Array::Copy` negative-argument cases, naming the exact production lines
(`Buffer.hpp:59`, `Array.hpp:154`).

### 4.1 Observations no sanitizer can make

| Case | Measured |
|---|---|
| `obs.overlap_*` (11 of the 12 doors) | `aaaa` where .NET yields `aabc` |
| `obs.overlap_array_copy_left` | `bcdd` — **already correct**; left overlap needs no change |
| `obs.overlap_array_copy3` | `abcd` — degenerate (source == destination), no change |
| `obs.split_empty_sequence` / `…_rangefor` / `…_empty_src` | **1000/1000 moves, i.e. non-terminating** |
| `obs.split_controls` | nonempty sequence → 3 moves; **empty any-of → 1 move (already terminating)** |
| `obs.segment_default_others` | `ToArray=0`, `CopyTo(vector)` no-op, `CopyTo(segment)` ok, `Contains=false`, `IndexOf=-1`, range-`for` 0 iterations, indexer `ArgumentOutOfRangeException` |
| `obs.buffer_getbyte_nontrivial` | `ByteLength(vector<string>)=32`, `GetByte(…,0)=80` — a raw byte of a `std::string`'s internal pointer, handed to a caller |

---

## 5. Premise corrections (every one measured)

1. **The brief's framing that all five findings are "ASan-decidable" is wrong for two of
   them.** SR-AUD-044 (for the eleven non-raw doors) and SR-AUD-045 produce **no**
   sanitizer report of any kind. Overlapping element assignment is perfectly
   memory-safe — it is a *data-loss* defect; a non-advancing enumerator is a *liveness*
   defect. Both were measured under ASan **and** UBSan with zero reports. Stating this is
   part of the deliverable: a clean sanitizer is not evidence for semantics it cannot
   observe.
2. **One overlap door *is* ASan-decidable, and the audit did not say so.** The raw-pointer
   `Array::Copy` uses `memcpy`, and ASan's interceptor reports **`memcpy-param-overlap`**
   for overlapping `int` ranges (`Array.hpp:154`). That is a genuinely different defect
   class from the other eleven doors and is repaired by the SR-AUD-051 ticket, not the
   SR-AUD-044 one.
3. **The audit's predicted defect classes are wrong in three places on this toolchain.**
   `Array::Copy(…, -1)` reports **`unknown-crash`**, not `negative-size-param`;
   `BlockCopy(…, -1)` reports **`stack-buffer-overflow`**, not `negative-size-param`
   (GCC 13.3 at `-O1` inlines the `_FORTIFY_SOURCE` `__memmove_chk`, so the libsanitizer
   interceptor that produces `negative-size-param` is bypassed); and the nontrivial
   `Array::Copy` reports **`double-free`**, not "attempting free on address which was not
   `malloc()`-ed".
4. **SR-AUD-067 is three doors, not one.** The audit demonstrated `count < 0` and said
   negative offsets "similarly form invalid pointers". Measured: `srcOffset < 0` is a
   **stack-buffer-underflow** and `dstOffset < 0` a **stack-buffer-overflow** — two
   further, separately reproducible ASan defects with different classes.
5. **SR-AUD-051's raw `Array::Copy` is four doors, not two.** Negative `length`,
   negative `srcIndex`, nontrivial `T`, **and** overlap (`memcpy-param-overlap`).
6. **SR-AUD-054 is twelve doors, of which only two are memory-unsafe.** `Slice(intcs)`
   and `Slice(intcs,intcs)` reach the null dereference; the other ten are parity breaches
   with no memory error. The audit named `Slice(0)` only, and did not enumerate
   `Slice(0,0)`, the two indexers, or `IndexOf`.
7. **The default indexer already throws — with the wrong type.** `ArraySegment<int>()[0]`
   throws `ArgumentOutOfRangeException` because `count_ == 0` makes the range check fire
   before the null dereference. .NET throws `InvalidOperationException`. This is a
   silent-wrong-*exception* case the audit did not record.
8. **Left overlap was already correct.** `Array::Copy(v, 1, v, 0, 3)` yields `bcdd`,
   exactly .NET's answer. Only *right* overlap (destination strictly inside the source
   range) is broken. A repair that unconditionally copies backward would break the
   currently-correct direction; the fix must be direction-*selecting*.
9. **The empty any-of separator list already terminates** (1 move, whole source). Only the
   empty *exact sequence* is non-terminating, so SR-AUD-045's repair must not touch the
   any-of path.
10. **`SpanSplitEnumerator` has no in-repo production consumer** — the only non-header
    reference in the whole repository is its own test file, so its repair cannot regress
    anything but its own suite.
11. **`std::copy` and `std::copy_backward` are indistinguishable for a trivially copyable
    element type**, because libstdc++ lowers both to `__builtin_memmove`, which is correct
    in either direction. Measured while mutation-testing #2213: an unconditional backward
    copy passed every `int` overlap assertion and only failed once an owning element type
    was used (`bcdd` became `dddd`). This is the same masking the audit noticed for the
    forward case and generalises it: **every direction assertion in this family must use a
    non-trivially-copyable element type**, or it proves nothing. Recorded because the
    first version of the #2213 test did exactly that and let a mutant survive.
12. **The raw doors have no in-repo production consumer either.** `Array::Copy(T*, …)`,
    `Buffer::BlockCopy(const void*, …)` and the generic `Buffer` vector templates are
    referenced only from `modules/core/tests`. Every in-repo call site uses a
    trivially-copyable element type.

---

## 6. Root-cause grouping — four subfamilies, not one

The brief asked whether these five share a structural cause. They do **not** share one;
they share a *shape* ("a public door acts on state it never validated"). Forcing one
family would misstate the repairs, which are genuinely different. Measured grouping:

| Subfamily | Findings | Root cause | Sanitizer-decidable |
|---|---|---|---|
| **CMS-A — raw byte copy over typed storage** | SR-AUD-051, SR-AUD-067 | A public door casts signed public metadata straight to `size_t` and hands it to `memcpy`/`memmove`, and applies an object-representation copy to a type whose lifetime it does not own. | **yes** (ASan) |
| **CMS-B — the invalid/default state is not a guarded state** | SR-AUD-054 | The default value deliberately holds a null owner, and the operations form references/pointers from it without asking whether an owner exists. | **yes** (ASan + UBSan) — for 2 of 12 doors |
| **CMS-C — forward-only element copy is not the overlap-safe copy contract** | SR-AUD-044 | The copy *direction* is fixed at "forward" rather than selected from the operands' relative addresses. | **no** |
| **CMS-D — a zero-width match does not advance the cursor** | SR-AUD-045 | The separator length is taken from the separator itself with no zero-length mode, so `startNext_` is a fixed point. | **no** |

CMS-A and CMS-B do share one true sub-cause — *unvalidated public argument reaches a
pointer/size computation* — and that is precisely the cause **CCF-005** already names
(`audit/AUDIT_CROSS_CUTTING_FINDINGS.md:339`). See §8.

---

## 7. Ownership and lifetime model

Established by reading the storage, not assumed:

- `Span<T>`/`ReadOnlySpan<T>`: `T* ptr_ + intcs length_`. **Borrowed**, non-owning, no
  liveness link to the storage.
- `Memory<T>`/`ArraySegment<T>`: `std::vector<T>* + intcs offset_ + intcs count_`.
  **Borrowed** pointer to a container the view does not own.
- `ReadOnlyMemory<T>`: `const T* + intcs length_`. **Borrowed.**
- `Buffer`, `Array`: static-only classes, `= delete`d constructors, **no state at all**.
- `SpanSplitEnumerator<T>`: holds a `ReadOnlySpan<T>` by value plus two `std::vector<T>`
  separator copies. **Borrowed** source, **owned** separators.

**No repair in this family changes any of that, and none introduces shared ownership.**
The brief's warning ("do not automatically solve a dangling-view finding by introducing
shared ownership") is honoured trivially, because **not one of these five findings is a
dangling-view finding**. There is no use-after-free, no stale cached pointer, no
moved-from hazard and no null-owner *outliving* problem here; SR-AUD-054's null owner is
a *never-set* owner, not a *dead* one. The correct repair for it is a state guard, not
ownership.

Consequently: no `shared_ptr`, no reference counting, no pimpl, no new member of any kind.
Every repair is a guard, a direction selection, a type constraint or a mode.

---

## 8. Relationship to CCF-019 and CCF-005

**CCF-019 — "borrowed native handles outlive the owner without a liveness boundary" — has
no member in this family.** Checked against its definition and its six recorded sites: it
covers async members that capture a raw `this` and borrowed handles handed to callers that
outlive the owner. None of the five findings is of that shape (see §7). **CCF-019 is not
consumed, not extended, and not closed by this family**, and no competing local ownership
policy is created.

**CCF-005 — "high-value conversion APIs need explicit boundary and special-value
validation" — is the governing family for CMS-A, and it is already closed.**
`docs/ConversionBoundaryFamilyPlan.md` §18 completes CCF-005 on
SR-AUD-026/027/041/043/047, and §17 **explicitly excludes SR-AUD-044 and SR-AUD-045**.
SR-AUD-051 and SR-AUD-067 are *the same cause as CCF-005* applied to the raw-pointer
doors CCF-005 deliberately left as "documented-precondition APIs". This plan records that
adjacency and repairs them here **without minting a new CCF identifier**: CCF-005's own
completion criterion is already met and does not reopen, and CCF-021/CCF-022 stay
unminted. The precedent is `ConversionBoundaryFamilyPlan.md` §17's own bullet — a raw
overload may stay a documented-precondition API for *capacity*, but that "does not justify
accepting invalid signed metadata" (`Buffer.hpp.audit.md`).

---

## 9. Dependency order

```
#2210 (this plan)
  ├─ #2211  SR-AUD-045   SpanSplitEnumerator empty sequence      (independent)
  ├─ #2212  SR-AUD-067   raw Buffer::BlockCopy argument guard    (independent)
  ├─ #2213  SR-AUD-051   raw Array::Copy + Buffer generics       (introduces detail/OverlapCopy.hpp)
  ├─ #2214  SR-AUD-054   ArraySegment default-state guard        (independent of the above)
  ├─ #2216  SR-AUD-044   overlap-safe copy, 11 remaining doors   (DEPENDS on #2213's helper
  │                                                               and on #2214's guard for the
  │                                                               two ArraySegment doors)
  └─ #2215  SR-AUD-054 residual: enumeration door                (BLOCKED — needs approval)
```

---

## 10. Compatible / blocked / deferred classification

| Ticket | Finding | Class | Why |
|---|---|---|---|
| #2211 | SR-AUD-045 | **compatible-ready** | One `else if` in one inline body. No signature, layout, vtable or `noexcept` change. Turns non-termination into .NET's single-segment result. |
| #2212 | SR-AUD-067 | **compatible-ready** | Three `ThrowIfNegative` calls in one inline body. Turns UB into a deterministic exception. |
| #2213 | SR-AUD-051 | **compatible-ready** | Argument validation + element-wise copy in `Array::Copy`; `static_assert` in four `Buffer` templates. The `static_assert` is a **compile-time source break for nontrivial `T`** — and every such call was already memory-corrupting (§4). |
| #2214 | SR-AUD-054 | **compatible-ready** | One private guard, called from ten doors. Adds no member; a non-virtual member function does not change layout or vtable. |
| #2216 | SR-AUD-044 | **compatible-ready** | Direction-selecting copy in one shared helper, routed through eleven doors. No signature or layout change. |
| **#2215** | SR-AUD-054 residual | **BLOCKED — needs user approval** | `begin()`/`end()` are the port's `GetEnumerator()` counterpart and are `noexcept`. Making a default segment throw there requires **dropping `noexcept`**, which this repository treats as approval-gated (precedent: ticket #1854, `ReadOnlyMemory`'s constructors). Current behaviour is pinned by a test so the day approval lands, the pin **inverts** rather than passing silently. |

---

## 11. Source / ABI / layout / vtable / `noexcept` consequences

| Change | Signature | Data member | `sizeof`/`alignof` | vtable | `noexcept` | Mangled symbol |
|---|---|---|---|---|---|---|
| #2211 mode guard | — | — | — | — | — | — |
| #2212 argument guard | — | — | — | — | — | — |
| #2213 `Array::Copy` raw body | — | — | — | — | — | — |
| #2213 `Buffer` `static_assert` | — | — | — | — | — | — (rejects at compile time) |
| #2214 private `throwIfDefault()` | new **private, non-virtual** member function | none | unchanged | none (no virtuals in `ArraySegment`) | — | new inline symbol, no existing one changed |
| #2216 `detail::copyOverlapAware` | new free function template in `System::detail` | — | — | — | — | new inline symbol |

**No class in this family has a virtual function**, so no vtable can change. No repair
adds, removes or reorders a data member, so no `sizeof`/`alignof` can change; §15 lists
the measurements that verify it. Every implicated entity is a template or an `inline`
member, so no out-of-line symbol's mangled name is touched.

**Observable behaviour changes, all deliberate, all .NET parity:**

| # | Was | Becomes |
|---|---|---|
| B1 | empty exact sequence enumerates forever | yields the whole source once, then `false` |
| B2 | raw `BlockCopy`/`Array::Copy` with negative metadata: UB | `ArgumentOutOfRangeException` |
| B3 | `Buffer::BlockCopy`/`ByteLength`/`GetByte`/`SetByte` with nontrivial `T`: memory corruption | **does not compile** |
| B4 | default-segment `Slice`: SEGV | `InvalidOperationException` |
| B5 | default-segment `ToArray`/`CopyTo`/`Contains`/`IndexOf`: silent success | `InvalidOperationException` |
| B6 | default-segment indexer: `ArgumentOutOfRangeException` | `InvalidOperationException` |
| B7 | right-overlapping copies: source data lost | source preserved, .NET's answer |

B3 is the only **compile-time** break. Migration: use the `std::vector<bytecs>` overload
or copy elements with `Array::Copy`. Recorded here rather than in a separate
`docs/Migration-*.md` because the affected spelling has no in-repo caller (§5.11) and the
compiler names the exact requirement at the call site.

---

## 12. Exact exception contracts pinned

| Door | Type | `paramName` | Message | Order |
|---|---|---|---|---|
| raw `Buffer::BlockCopy` | `ArgumentOutOfRangeException` | `srcOffset`, `dstOffset`, `count` | `ThrowIfNegative`'s standard message | `srcOffset` → `dstOffset` → `count`, **identical to the sibling vector overload's `requireValidBlockCopyRange`** |
| raw `Array::Copy` | `ArgumentOutOfRangeException` | `srcIndex`, `dstIndex`, `length` | `ThrowIfNegative`'s standard message | `srcIndex` → `dstIndex` → `length`; then null-pointer rejection |
| raw `Array::Copy` null with `length > 0` | `ArgumentNullException` | `src` / `dst` | default | after the three negative checks |
| `ArraySegment` default state | `InvalidOperationException` | — | `"The underlying array is null."` (.NET `SR.InvalidOperation_NullArray`) | **first**, before every other validation in the door |
| `ArraySegment::CopyTo(ArraySegment&)` | as above | — | as above | **source first, then destination**, then the length check — .NET's own order |

**Deliberate naming decision.** The raw `Array::Copy` names its own C++ parameters
(`srcIndex`/`dstIndex`/`length`); .NET's `Array.Copy` names them
`sourceIndex`/`destinationIndex`/`length`. The port's *vector* overloads already report
`"index"`/`"length"` through the shared `requireValidRange`, so no single choice makes all
overloads agree. Naming the real parameter is the least surprising and matches #1869's
principle (the observable message should name the argument the caller passed). Aligning
**both** forms to .NET's names is a separate, unscoped question and is **not** done here.

**Deliberate null decision.** .NET's `Array.Copy` rejects a null array even for
`length == 0`; a null pointer with a zero length is the ordinary C++ empty-range idiom, so
this port rejects null only when `length > 0`. Recorded as a stated deviation.

No `std::` exception is introduced at any public boundary by this family; every new throw
derives from `System::Exception`.

---

## 13. No-partial-state rule

- `TryCopyTo` (all four) keeps its **no-write-on-false** rule: the length check precedes
  every write, and the direction selection happens after it.
- Rejected raw `Array::Copy` / `BlockCopy` calls perform **no pointer arithmetic** — the
  validation runs before `src + srcIndex` is formed, which is exactly the difference
  between "throws" and "already computed an invalid pointer".
- A rejected `ArraySegment` door mutates nothing: `throwIfDefault()` is the first statement,
  before the destination `resize` in `CopyTo(vector&, intcs)`.
- No repair leaves a partially copied range: `std::copy`/`std::copy_backward` over the
  selected direction either completes or propagates the element type's own exception, the
  same as before.

---

## 14. Permanent test matrix

New suite `modules/core/tests/System/CoreMemorySafetyTests.cpp`.

**Overlap (CMS-C) — for every one of the 12 doors:**
left overlap, right overlap, exact self (source == destination), adjacent (destination ==
source end), zero length, one element, `int` **and** `std::string` **and** an observable
copy-counting type, and a non-overlapping control that must keep its existing result.

**Span/view/index safety (as the brief enumerates):** empty owner, empty view, one
element, exact end, one-past-end, negative index where representable, maximum index,
zero-length slice at the end, nested slices, const and mutable variants, aliasing source
and destination. *(Owner destroyed / moved-from / reallocated before use are **excluded** —
see §17.)*

**Raw doors (CMS-A):** negative `srcOffset`/`dstOffset`/`count`, negative
`srcIndex`/`dstIndex`/`length`, each asserting the exact type **and** `paramName`;
validation order; `count == 0` still succeeds; a valid copy still produces its previous
result; right-overlapping raw copy now yields .NET's answer; null with `length > 0`
rejected; null with `length == 0` accepted.

**Compile-time rejection (B3):** `test/consumer/core_buffer_trivially_copyable_negative.cpp`,
four sites (`BlockCopy`, `ByteLength`, `GetByte`, `SetByte`), each with the `static_assert`
text as its expected diagnostic; `#else` branches carry a trivially-copyable `T` that must
still compile; the no-site baseline compiles with zero diagnostics.

**Default segment (CMS-B):** all ten guarded doors assert `InvalidOperationException` and
the exact message; the source-before-destination order for `CopyTo(ArraySegment&)`; a
default destination rejected by a **valid** source; the unguarded doors
(`getArrayProperty`/`getOffsetProperty`/`getCountProperty`/`Equals`/`GetHashCode`) still
answer; a non-default segment still produces every previous result; and **the #2215 pin** —
range-`for` over a default segment still performs zero iterations and `begin() == end() ==
nullptr`, with a comment saying the assertion must be inverted when #2215 ships.

**Split (CMS-D):** empty exact sequence over a nonempty source yields exactly one segment
equal to the whole source and then `false`; over an **empty** source likewise; bounded
range-`for` terminates; `getCurrentProperty()`'s `Range` is inspected; `MoveNext()` after
completion stays `false`; nonempty sequence at start / end / adjacent positions; the
empty any-of list keeps its existing (different, deliberate) semantics.

**Move/copy matrix.** `Span`, `ReadOnlySpan`, `Memory`, `ReadOnlyMemory` and
`ArraySegment` are all trivially copyable value views with implicit copy/move and no
user-declared destructor; `Array` and `Buffer` have no state. The matrix therefore
reduces to: default → move, populated → move, moved-from reuse, move-assign over a
populated target, self-move-assign, copy-construct, copy-assign — each asserted to leave
the view describing the same `(pointer/owner, offset, length)` triple, i.e. **no operation
on a view invalidates another view**. `static_assert(std::is_trivially_copyable_v<…>)` pins
the property that makes that true, so a future member addition that breaks it fails the
build rather than the reasoning.

---

## 15. Sanitizer matrix

| Tool | Applies to | What it can decide here |
|---|---|---|
| **ASan** | CMS-A (all 6 doors), CMS-B (`Slice` ×2), the raw overlap door | before: 8 distinct reports (§4); after: **absent**, with an independent control still firing in the same build |
| **UBSan** | CMS-B (`Slice` ×2: reference-bind-to-null, member-call-on-null); CMS-A (pointer arithmetic, signed→unsigned casts) | before: 3 reports at `ArraySegment.hpp:180/198` and `:73`; after: absent |
| **LSan** | the rejection paths — a throw must not strand an allocation | run on every after-case; **honest limit:** the family allocates nothing that a guard could strand, so a clean LSan here is confirmation, not discovery |
| **TSan** | **not applicable, and stated as such** | nothing in this family has shared mutable state: `Array` and `Buffer` are stateless, and every view is a caller-owned value. Running TSan would produce a clean result that means nothing. |
| **none** | CMS-C (11 doors), CMS-D | **no sanitizer can observe either.** Their evidence is the measured values and the bounded iteration counter. |

`-fno-sanitize-recover=undefined` is used for the after-run so a surviving UBSan site
aborts instead of printing and continuing.

---

## 16. Bounded ticket split

| # | Title | Finding | P | Size | Status target |
|---|---|---|---|---|---|
| 2210 | REVIEW — Core memory-safety family | all five | P1 | M | done |
| 2211 | Empty exact sequence never advances | SR-AUD-045 | P1 | XS | done → remediated |
| 2212 | Raw `Buffer::BlockCopy` negative metadata | SR-AUD-067 | P1 | XS | done → remediated |
| 2213 | Raw `Array::Copy` + generic `Buffer` templates | SR-AUD-051 | P1 | S | done → remediated |
| 2214 | Default `ArraySegment` state guard (10 doors) | SR-AUD-054 | P1 | S | done → remediated |
| 2215 | Enumeration door needs a `noexcept` drop | SR-AUD-054 residual | P2 | XS | **blocked** |
| 2216 | Overlap-safe copy across 11 doors | SR-AUD-044 | P1 | M | done → remediated |

Findings SR-AUD-044/045/051/067 flip to `remediated` in full. **SR-AUD-054 flips to
`remediated` for the memory-safety claim and the ten compatible doors, with #2215 recorded
as its named residual** — the audit index entry states the residual explicitly rather than
implying completeness.

---

## 17. Explicit exclusions

- **SR-AUD-043b / #1854** — already done; the `noexcept` question it settled is *not*
  reopened, and #2215 is deliberately modelled on its precedent rather than pre-empting it.
- **SR-AUD-046, SR-AUD-052, SR-AUD-053** — same reports, different causes; 046 and 052 are
  already remediated, **053 (`Array::MaxLengthProperty`) stays `confirmed` and is not
  touched.**
- **SR-AUD-055** — `ArraySegment::CopyTo(vector&, …)` *resizing* an undersized destination.
  Medium, a different cause (a deliberate vector adaptation), and repairing it would change
  a behaviour an existing test explicitly pins
  (`CopyTo_VectorWithOffset_ExpandsDest`). The resize is preserved byte-for-byte by #2216.
- **SR-AUD-018 (extended)** — `ArraySegment::GetHashCode`'s sign mask. Different cause.
- **Owner-destroyed / owner-moved-from / owner-reallocated view tests.** The brief lists
  them; they are excluded **with a reason**: every one is *documented, intentional* C++
  behaviour for a non-owning view (`Span.hpp:27`, `ArraySegment.hpp:21`,
  `Memory.hpp:220-222` all state the precondition), so a test asserting a crash would pin
  undefined behaviour as a contract. `docs/CoreOwnership.md` owns that question; changing
  it is a CCF-019-shaped ownership decision this family must not take unilaterally (§8).
- **Changing any `length_`/`count_` field to unsigned** — rejected for the same reason
  CCF-005 §17 rejected it: it is the only layout-breaking option and .NET keeps the field
  signed.
- **Adding a length parameter to any raw-pointer overload** — a public signature change,
  and the capacity limitation is genuine and documented.
- **`Half::TryFormat` and the two `Guid` span constructors** — inspected, both copy from a
  local into a caller buffer with no reachable aliasing; not overlap doors.

---

## 18. Family completion criteria

This family is complete when **all** of the following hold:

1. SR-AUD-044, 045, 051, 067 are `remediated` in `audit/AUDIT_FINDINGS_INDEX.md`;
   SR-AUD-054 is `remediated` with #2215 named as its residual in the owning report.
2. Every one of the 32 doors in §3 has either a repair or a recorded, reasoned exclusion.
3. Every before-report in §4 is **absent** after, re-measured in a build proved to be
   instrumented, with an independent control still firing.
4. The two non-sanitizer-decidable subfamilies have measured value/termination evidence
   before and after.
5. Permanent regressions exist for every door and every pinned exception contract, and the
   #2215 pin is present and marked for inversion.
6. Whole-repository gate shows **no test-count regression** and 0 warnings / 0 errors.
7. `scripts/local_ci_check.sh build` green apart from its known #1962 `Ping` stop; module
   graph, seams and negative-fixture counts re-measured.
8. No `SR-AUD-*` identifier created; CCF-019 untouched; CCF-021/CCF-022 unminted.

**Not** a completion criterion: closing `modules/core`. 67 findings in that module are
outside this family and stay open.

---

## 19. Implementation status — the family is CLOSED except for one named residual (2026-08-10)

| Ticket | Finding | Status | Disposition of the finding |
|---|---|---|---|
| #2210 | all five | **done** | this plan |
| #2211 | SR-AUD-045 | **done** | **remediated** |
| #2212 | SR-AUD-067 | **done** | **remediated** |
| #2213 | SR-AUD-051 | **done** | **remediated** (incl. the `Buffer.hpp` extension) |
| #2214 | SR-AUD-054 | **done** | **remediated**, with residual #2215 named in the index row |
| #2216 | SR-AUD-044 | **done** | **remediated** (all three owning reports) |
| **#2215** | SR-AUD-054 residual | **needs_user** | the `noexcept` question, unanswered |

**Every one of §18's eight completion criteria is met**, with criterion 1 met in the qualified form
it specifies: SR-AUD-054 is `remediated` **and** its residual is stated in the index row rather than
implied away. All 32 doors of §3 have a repair or a recorded exclusion; all ten before-reports of §4
are absent afterwards in a build proved instrumented with a control still firing; the two
non-sanitizer-decidable subfamilies have measured value and termination evidence on both sides; and
the #2215 pin exists and is marked for inversion.

**Verdict, stated exactly as the brief requires: this bounded Core memory-safety family is closed
except for one exact residual, ticket #2215.** It is **not** a statement that `modules/core` is
closed — 67 findings in that module remain open, and §16 of `NEXT.md` selects the next bounded
family (SR-AUD-131, SR-AUD-135, SR-AUD-180, review ticket #2217).

### 19.1 Aggregate evidence

- **Permanent regressions:** +42 across the five tickets (7 / 8 / 9 / 8 / 10), all in
  `modules/core/tests/System/CoreMemorySafetyTests.cpp`, plus **4 negative consumer sites** in
  `test/consumer/core_buffer_trivially_copyable_negative.cpp`.
- **Mutations: 25 raised, 25 killed.** Two kill signals are labelled as weaker than the rest rather
  than counted as equivalent: #2212's "delete all three guards" kills by **process abort**, and
  #2213's "drop one `static_assert`" kills through the **negative-fixture checker** because the
  claim it removes is a compile-time one. One mutation (#2213's M5) **survived its first run** and
  produced premise correction 11.
- **Sanitizers:** ASan and UBSan over the instrumented production bodies, before and after; LSan on
  the after-cases (confirmation, not discovery); **TSan not applicable and recorded as such**.
- **Gate:** 16,605 tests across 38 executables, 16,597 passing, 2 skipped, 6 failing for the two
  pre-existing measured causes. Zero warnings, zero errors.
- **Boundaries unchanged:** module graph 41 / 92, seams 3 / 20. Negative fixtures 13 → 14 files,
  116 → 120 sites.
- **ABI:** no `sizeof`, `alignof`, vtable, `noexcept` specification, default argument, data member
  or mangled symbol changed anywhere in the family. One new header,
  `System/detail/OverlapCopy.hpp`, inside the existing `Core.Base` module.
