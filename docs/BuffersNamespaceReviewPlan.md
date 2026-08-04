<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Buffers` namespace review plan — ticket #2048

*Written 2026-08-04 on branch `feature/remediation-batch-buffers-review`, cut from the clean tip
`27061bf`. This is the **eighth** namespace review in the #1950 / #1964 / #1972 / #1987 / #2006 /
#2023 / #2034 series and follows the same shape: measure the selection, reproduce every premise
against the **shipped** bodies before classifying it, correct the audit where measurement
disagrees, group by root cause rather than by file, split compatible work from
approval-sensitive work, and leave a bounded queue.*

**No `SR-AUD-*` identifier is issued by this review.** Audit numbering stays frozen at **364**.
Defects found while reviewing get ordinary ticket numbers.

---

## 1. Scope, and why `System::Buffers` is next — the selection, re-derived

Re-derived by measurement over `audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-04, **not** inherited
from the previous handoff (which named `modules/buffers` and `modules/net-http`). The index was
re-parsed in full: **364 findings, 117 `remediated`, 204 `confirmed`, 43
`confirmed (design-complete)`** — 247 open, matching the previous batch's claim exactly. Every
un-reviewed module with ≥ 6 open findings, plus every module above a 25 % high ratio:

| Module / namespace | Open | high | med | low | high % | Existing plan? | Reviewed? | Cohesion |
|---|---|---|---|---|---|---|---|---|
| `modules/core` | 72 | 9 | 59 | 4 | 13 % | many family plans | partly, by family | **poor** — not a namespace |
| `modules/threading` | 17 | 6 | 11 | 0 | 35 % | yes | **yes** (#1950) | — |
| `modules/runtime` | 14 | 1 | 12 | 1 | 7 % | yes | **yes** (#1972) | — |
| **`modules/buffers`** | **11** | **3** | **8** | 0 | **27 %** | **partial** — `Base64FamilyPlan.md` | **no** | **good** — one module, one namespace |
| `modules/io` | 11 | **0** | 11 | 0 | **0 %** | partial ×2 | no | medium |
| `modules/text` | 11 | 1 | 10 | 0 | 9 % | yes | **yes** (#2006) | — |
| `modules/uri` | 10 | 0 | 10 | 0 | 0 % | yes | **yes** (#1987) | — |
| `modules/net` | 5 | 1 | 4 | 0 | 20 % | yes | **yes** (#2034) | — |
| `modules/net-http` | 9 | 2 | 7 | 0 | 22 % | no | no | separate namespace |
| `modules/xml` | 8 | 2 | 6 | 0 | 25 % | no | no | medium (+4 in `xml-linq`) |
| `modules/time-zone` | 7 | **0** | 7 | 0 | 0 % | no | no | good |
| `modules/globalization` | 7 | 1 | 6 | 0 | 14 % | no | no | needs ICU data absent here |
| `modules/text-json` | 7 | 1 | 6 | 0 | 14 % | no | no | good |
| `modules/net-websockets` | 6 | 2 | 4 | 0 | 33 % | no | no | only 6 findings |

`System::Buffers` wins on the same rule the previous seven reviews used:

- **Count and severity together.** It is tied with `modules/io` for the largest open count of any
  un-reviewed unit (11) and `io` has **zero** high findings, so the tie breaks decisively. Its
  27 % high ratio is the highest of any un-reviewed unit with more than six findings —
  `net-websockets` is higher at 33 % but has only six.
- **The severity's character.** All three highs are **memory safety on a public door**, not
  formatting parity: a raw-pointer constructor that dereferences a null it never checks
  (SR-AUD-072), a `TryGet` that forms a view from an unvalidated caller-supplied position and
  reads before the allocation (SR-AUD-073), and an owner with no terminal disposed state whose
  retained view outlives its storage (SR-AUD-071). Two are reproduced below under ASan in this
  context.
- **Dependency readiness.** `modules/buffers` declares exactly one public component dependency,
  `Core.Base`, and — measured, not assumed — **no module outside `modules/buffers` includes any
  of the headers this review changes.** The complete cross-module use of `System/Buffers/*` is
  `MemoryHandle.hpp` (from `Memory.hpp`/`ReadOnlyMemory.hpp` in core), `OperationStatus.hpp`
  (io-compression ×2, text ×2) and `Binary/BinaryPrimitives.hpp` (numerics). The blast radius of
  §18's compatible queue is therefore the module itself.
- **Not blocked.** No open blocked ticket names a `modules/buffers` type. `#1962`, `#1773` and the
  twelve decisions in `docs/ConsolidatedApprovalPackage.md` are all in other modules.
- **A useful compatible queue.** Measured, **six** of the eleven repairs plus two post-audit
  defects need no approval at all (§18.1).

### 1.1 The review unit is the namespace, which is slightly wider than the module

`System::Buffers` is **not** exactly `modules/buffers`. One type in the namespace,
`System::Buffers::MemoryHandle`, lives in `modules/core/include/System/Buffers/MemoryHandle.hpp`
because `Memory<T>`/`ReadOnlyMemory<T>` need it and `Core.Base` cannot depend on `Buffers`. Its
finding **SR-AUD-088** is therefore indexed under `modules/core` in the table above, and the
finding's own `Source` column names `IPinnable.hpp` — which *is* in `modules/buffers`. This
review covers it.

**The measured open-finding count for the namespace is therefore 12, not 11.** The selection
above is unaffected (it used the module figure, which is the conservative one).

`System::SequencePosition` (SR-AUD-069, `confirmed`, medium) is a **`System`-namespace** type in
`modules/core` that this namespace's API is built on; SR-AUD-073's audit text names it as
context. It is **not** claimed by this review — §21 records why and what it costs.

### 1.2 File inventory

Twenty-two public headers, no `.cpp` (the module is header-only; `src/` holds only `.gitkeep`),
sixteen test files. Line counts as of `27061bf`:

| Header | Lines | Findings |
|---|---|---|
| `Text/Base64.hpp` | 725 | SR-AUD-078 ✔, 079 ✔, 080 ✔, **081 (false positive, §4.10)** |
| `Text/Base64Url.hpp` | 671 | SR-AUD-078 ✔, 079 ✔, 082 ✔ |
| `Binary/BinaryPrimitives.hpp` | 643 | — |
| `Text/Utf8Parser.hpp` | 425 | SR-AUD-084 ✔, 085 ✔, **086** |
| `ReadOnlySequence.hpp` | 311 | **072**, **073**, **074** |
| `Text/Utf8Formatter.hpp` | 297 | — |
| `SequenceReader.hpp` | 249 | SR-AUD-075 ✔ |
| `ArrayBufferWriter.hpp` | 160 | **070** |
| `StandardFormat.hpp` | 130 | **083** |
| `SequenceReaderExtensions.hpp` | 122 | — |
| `BuffersExtensions.hpp` | 114 | — (post-audit, §5.6) |
| `MemoryManager.hpp` | 104 | — |
| `MemoryPool.hpp` | 100 | **071**, 070 (extended) |
| `SearchValues.hpp` | 98 | **077** |
| `ArrayPool.hpp` | 85 | **076**, 070 (extended) |
| `ReadOnlySequenceSegment.hpp` | 62 | **087** |
| `IPinnable.hpp` | 52 | **088** (with core's `MemoryHandle.hpp`) |
| `IBufferWriter.hpp` | 49 | — |
| `IMemoryOwner.hpp` | 25 | 071 (surface) |
| `ReadOnlySpanAction.hpp` / `SpanAction.hpp` | 23 each | — |
| `OperationStatus.hpp` | 20 | — |
| `modules/core/include/System/Buffers/MemoryHandle.hpp` | 47 | **088** |

✔ = already `remediated`.

---

## 2. Complete public-surface inventory

Recorded because §4 must distinguish *the named site* from *the complete public surface*, and
because five of the twelve findings turn on a surface that is **absent** rather than wrong.

**Present and implemented**

- `ArrayBufferWriter<T>` — ctor(), ctor(intcs), `getWrittenMemoryProperty`, `getWrittenSpanProperty`,
  `getWrittenCountProperty`, `getCapacityProperty`, `getFreeCapacityProperty`, `Clear`,
  `ResetWrittenCount`, `Advance`, `GetMemory`, `GetSpan`, `DefaultInitialBufferSize`.
- `ArrayPool<T>` — `Rent`, `Return`, `Shared`, `Create()`, `Create(intcs,intcs)`; `SharedArrayPool<T>`.
- `MemoryPool<T>` — `getMaxBufferSizeProperty`, `Rent`, `Shared`, `MaxArrayLength`;
  `MemoryPoolHeapOwner_<T>`, `DefaultMemoryPool_<T>`.
- `IBufferWriter<T>`, `IMemoryOwner<T>`, `IPinnable`, `MemoryManager<T>` (interfaces).
- `ReadOnlySequence<T>` — ctor(), ctor(vector), ctor(const T*, intcs), `getEmpty`,
  `getStartProperty`, `getEndProperty`, `getLengthProperty`, `getIsEmptyProperty`, `First`,
  six `Slice` overloads, two `GetPosition` overloads, `TryGet`, `getIsSingleSegmentProperty`,
  `ToArray`, `CopyTo`, `Enumerator`, `GetEnumerator`.
- `ReadOnlySequenceSegment<T>` — three getters, three protected setters.
- `SequenceReader<T>` — ctor, `getConsumedProperty`, `getRemainingProperty`, `getEndProperty`,
  `getPositionProperty`, `getLengthProperty`, `getSequenceProperty`, `TryRead`, `TryPeek`,
  `IsNext(T)`, `IsNext(vector,bool)`, `Advance`, `TryAdvancePast`, `Rewind`, `TryReadTo`,
  `AdvancePast`.
- `SequenceReaderExtensions` — six `TryRead{Little,Big}Endian` overloads.
- `SearchValues<T>` — ctor(initializer_list), ctor(vector), `Contains`, `GetValues`;
  `SearchValuesFactory::Create` ×3.
- `StandardFormat` — ctor(), ctor(char,uint8_t), four getters, `==`, `!=`, `Equals`,
  `GetHashCode`, `ToString`, `TryParse`, `Parse`, `NoPrecision`, `MaxPrecision`.
- `BuffersExtensions` — `PositionOf`, `CopyTo`, `Write(span)`, `Write(sequence)`, `ToArray`.
- `OperationStatus`, `SpanAction`, `ReadOnlySpanAction`.
- `Binary::BinaryPrimitives`, `Text::Base64`, `Text::Base64Url`, `Text::Utf8Parser`,
  `Text::Utf8Formatter` — all reviewed by the Base64 family (`docs/Base64FamilyPlan.md`) or
  untouched by an open finding, except SR-AUD-086.

**Absent, and named by a finding**

- `ReadOnlySequence<T>(ReadOnlySequenceSegment<T>* start, int, ReadOnlySequenceSegment<T>* end, int)`
  — the segment-chain constructor. **SR-AUD-087.**
- `ReadOnlySequence<T>::FirstSpan`, `TryCopyTo`, `Equals`, `GetHashCode`, and any multi-segment
  state. Recorded here so §4.5's disposition covers the whole surface, not one constructor.
- `MemoryPool<T>::Dispose`, `MemoryPool<T>::MaxBufferSize` as a settable pool limit.
- `SearchValues` byte/char/string *optimised* factories and the `IndexOfAny` family.
- `Utf8Parser` `Guid`/`DateTime`/`TimeSpan`/floating/`Decimal` overloads — a documented baseline
  gap, deliberately not reopened here (§21).

---

## 3. Method

Every finding below was processed in the same seven steps, in order, and no classification was
written before step 6 produced output:

1. read the full historical finding text in `audit/modules/buffers/**`;
2. read the current production body at `27061bf`, not the body the audit read;
3. enumerate **all** structurally equivalent overloads and doors, independently of the finding's
   own site list;
4. read the owning tests to establish what is already pinned;
5. check `git log` for a prior remediation that already moved the path;
6. **reproduce** — `build-probe/2048_probe1_buffers_defects.cpp` (ten modes, ASan+UBSan),
   `build-probe/2048_probe2_positionof_cost.cpp` (allocation counting),
   `build-probe/2048_probe3_layout.cpp` (`sizeof`/`alignof`); logs
   `2048_probe1_before_safe.log`, `2048_probe1_before_unsafe.log`, `2048_probe2_before.log`,
   `2048_probe3_layout_before.log`, all retained;
7. correct severity, consequence, site count or premise where the measurement disagrees (§6).

**The .NET reference tree is absent from this container** — `/rv/tmp/runtime/src/libraries/`
does not exist. Reference behaviour therefore comes only from repository-contained evidence: the
per-file audit reports (written when the tree *was* present and quoting it), the committed design
records (`docs/Base64FamilyPlan.md`, `docs/TryOutputFailureContractPlan.md`), the existing tests,
and the headers' own quoted .NET citations. **Where that evidence does not settle a question, the
finding becomes a deferred-verification ticket rather than a guess** — §4.9 is the one case.

---

## 4. Finding-by-finding disposition

Twelve open findings. Every one has exactly one disposition and none disappears.

| Finding | Sev | Reproduced? | Disposition | Ticket |
|---|---|---|---|---|
| SR-AUD-072 | high | **yes** — UBSan null load + ASan SEGV | compatible implementation | **#2049** |
| SR-AUD-073 | high | **yes** — data leak + ASan heap-buffer-overflow | compatible implementation | **#2050** |
| SR-AUD-071 | high | **yes** — no throw, stale retained view | split: disclosure now, design blocked | **#2061** / **#2056** |
| SR-AUD-070 | medium | yes (compile) | compatible implementation | **#2054** |
| SR-AUD-074 | medium | **yes** — 1 segment for `default` | approval-sensitive design | **#2057** |
| SR-AUD-076 | medium | **yes** — `Create(0,1)` returns a pool | compatible implementation | **#2053** |
| SR-AUD-077 | medium | yes (compile) | compatible implementation | **#2054** |
| SR-AUD-081 | medium | n/a | **already corrected — no action** | — |
| SR-AUD-083 | medium | **yes** — `2,0,48` / `1,0` / `2,0,48` | compatible implementation | **#2052** |
| SR-AUD-086 | medium | **yes** — `+42` rejected on D/G, accepted on N | **deferred verification** | **#2060** |
| SR-AUD-087 | medium | yes (absence) | split: disclosure now, design blocked | **#2061** / **#2058** |
| SR-AUD-088 | medium | yes (absence) | split: disclosure now, design blocked | **#2061** / **#2059** |

### 4.1 SR-AUD-072 — the raw pointer/length constructor (high) → **#2049, compatible**

```cpp
ReadOnlySequence(const T* ptr, intcs length)
    : data_(ptr, ptr + length), start_(0), end_(length) {}
```

Nothing is checked. Measured in this context, `-fsanitize=address,undefined`:

| Call | Measured |
|---|---|
| `ReadOnlySequence<int>(nullptr, 1)` | UBSan `load of null pointer of type 'const int'` at `stl_algobase.h:398`, then **ASan `SEGV` on address 0x0**, inside `ReadOnlySequence<int>::ReadOnlySequence(int const*, int)` |
| `ReadOnlySequence<int>(data, -1)` | `std::length_error: cannot create std::vector larger than max_size()` — a **native** exception escaping a public door |
| `ReadOnlySequence<int>(nullptr, 0)` | constructs, length 0, `IsEmpty` true — **valid and already pinned by two tests** |

**Repair.** Validate before the member-initialiser runs: `length < 0` →
`ArgumentOutOfRangeException("length")`; `ptr == nullptr && length > 0` →
`ArgumentNullException("ptr")`; `ptr == nullptr && length == 0` → the existing empty sequence,
unchanged. Because the vector is a member built in the initialiser list, the check must be a
static helper called from the initialiser (the same shape ticket #1805 used for
`MemoryStream`'s raw constructor, SR-AUD-341).

**Not a widening or a narrowing of any defined behaviour**: both rejected shapes are currently
undefined or a native-exception escape, so no program with defined behaviour changes.

### 4.2 SR-AUD-073 — `TryGet` position provenance (high) → **#2050, compatible**

```cpp
intcs pos = position.GetInteger();
if (pos >= end_) { memory = {}; return false; }
memory = System::ReadOnlyMemory<T>(data_.data() + pos, end_ - pos);
```

`pos < start_` is never tested. Measured:

| Sequence | Forged position | Measured |
|---|---|---|
| `{10,20,30}` sliced to `[1,3)` (length 2, first element 20) | `SequencePosition(nullptr, 0)` | returns **true**, a **3**-element memory whose first element is **10** — an element the slice does not contain |
| `{10,20,30}` | `SequencePosition(nullptr, -1)` | returns true, then reading `[0]` is an **ASan `heap-buffer-overflow` READ of size 4**, *4 bytes before* the 12-byte region |

Both halves of the audit's claim reproduce exactly. The forged position is trivially
constructible because `System::SequencePosition`'s representation is public and mutable
(SR-AUD-069) — but note that **the leak does not need SR-AUD-069**: `getStartProperty()` of the
*unsliced* sequence is a perfectly ordinary, legitimately obtained position that a caller may
hold across a `Slice`, and passing it to the slice's `TryGet` produces the same out-of-slice
view. That is a genuine caller mistake this API must reject, not an exotic forgery, and it is
recorded here because it changes how severe the finding is (§6.2).

**Repair.** Enforce `start_ <= pos <= end_` before forming any view; outside that range throw
`ArgumentOutOfRangeException("position")`. `pos == end_` keeps returning `false` with an empty
memory — that is the already-pinned end-of-sequence contract
(`Batch6BuffersTests.cpp:620-627`, `Batch17BuffersTests.cpp:284-296`).

**This narrows the accepted input set**, in the direction of memory safety: a call that used to
return out-of-slice data or read out of bounds now throws. Nothing in this repository passes a
position from one sequence to another's `TryGet`.

### 4.3 SR-AUD-071 — no terminal disposed state (high) → **#2056 blocked + #2061 disclosure**

Measured: `Rent(16)` → `Memory` length 16; `Dispose()`; `getMemoryProperty()` returns length
**0** and does **not** throw; the `Memory` obtained *before* disposal still reports length **16**
over storage `shrink_to_fit` has released.

The finding has two halves with different costs and they must not be conflated:

- **(a) the post-dispose getter.** .NET's `ArrayMemoryPool<T>.ArrayMemoryPoolBuffer.Memory`
  throws `ObjectDisposedException` when its array field is null (quoted by the audit report).
  Reproducing that in C++ needs a terminal flag: a live `Rent(0)` and a disposed owner are
  **indistinguishable** from the vector alone — both are empty with zero capacity — so there is
  no state to overload. Measured, `sizeof(MemoryPoolHeapOwner_<int>)` is **32** (vptr 8 +
  `std::vector` 24) with `alignof` 8 and **no padding to reuse**; a `bool` takes it to 40. That
  is an object-layout change to a class template declared in a public header. **Blocked.**
- **(b) the retained view.** `System::Memory<T>` stores a pointer and a length with no owner
  liveness; a view taken before `Dispose` keeps both. Repairing that is a `Memory<T>` ownership
  change in `Core.Base` — the **CCF-019** shape — with a blast radius far outside this module.
  **Blocked**, and deliberately not scoped by this review.

**Compatible now:** `IMemoryOwner<T>`, `MemoryPool<T>` and `MemoryPoolHeapOwner_<T>` currently
document neither behaviour. #2061 makes the contract true and pins both halves so a future
approved option cannot land silently.

### 4.4 SR-AUD-070 + SR-AUD-077 — silent generic requirements (medium ×2) → **#2054, compatible**

Four sites, independently counted rather than taken from the finding:

| Site | Implicit requirement | Where it bites |
|---|---|---|
| `ArrayBufferWriter<T>::checkAndResizeBuffer` (`resize`) | `std::is_default_constructible_v<T>` | `GetSpan`/`GetMemory` |
| `ArrayBufferWriter<T>::Clear` (`T{}`) | same | `Clear` |
| `MemoryPoolHeapOwner_<T>` ctor (`buf_(size)`) | same | `MemoryPool<T>::Rent` |
| `SharedArrayPool<T>::Rent` + `ArrayPool<T>::Return(clearArray=true)` | same | `Rent`, `Return` |
| `SearchValues<T>` (`std::unordered_set<T>`) | usable `std::hash<T>` **and** `operator==` | both constructors and `Contains` |

The finding named the first and the last; the middle three are the "extended" notes in the
`MemoryPool` and `ArrayPool` reports, which this review promotes to first-class sites.

**Repair.** Two parts, both compatible: (i) state the requirement in each type's Doxygen block;
(ii) add a `static_assert` **at the point where the requirement is already enforced**, not at
class scope, so that *exactly the same set of programs compiles* and only the diagnostic
improves. A class-scope assert would reject a mere declaration of `ArrayBufferWriter<NonDefault>`
that compiles today; that would be a source break and is explicitly rejected.

### 4.5 SR-AUD-074 — `default` enumerates one segment (medium) → **#2057, approval-sensitive**

Measured: a default-constructed `ReadOnlySequence<int>` enumerates **1** segment; `getEmpty()`
also enumerates **1**. .NET yields **0** and **1** respectively.

Distinguishing them needs state `ReadOnlySequence<T>` does not have. Measured
`sizeof(ReadOnlySequence<int>)` is **32** (`std::vector` 24 + two `intcs` 8) — fully packed, no
padding. A discriminator is a **public object-layout change**. Additionally the enumerator's
`MoveNext` contract changes for one input. **Blocked**; behaviour pinned by #2061.

### 4.6 SR-AUD-076 — `Create(intcs, intcs)` ignores and accepts anything (medium) → **#2053, compatible**

Measured: `Create(0,1)`, `Create(1,0)` and `Create(-5,-7)` all return a usable pool, and
`Create(1024,10)->Rent(4000)` returns **4000** elements — the declared maximum is not applied.

Two separable claims. **Validation** is compatible: the audit records .NET's
`ConfigurableArrayPool` throwing `ArgumentOutOfRangeException` for a non-positive
`maxArrayLength` or `maxArraysPerBucket`, and adding the same rejection needs no new type, no
new member and no signature change. **Honouring** the limits is not: this pool has no buckets to
build, so a configured pool would be a new public class with new state, i.e. a public-surface
addition. #2053 implements validation and makes the header say plainly that the limits are
validated but not applied; the honouring half is recorded in §21 as out of scope for this batch
rather than smuggled in.

Both existing call sites (`ArrayPoolTests.cpp:38` → `(1024,10)`, `Batch16BuffersTests.cpp:90` →
`(1024,50)`) pass positive values and are unaffected.

### 4.7 SR-AUD-083 — `ToString` of a zero symbol (medium) → **#2052, compatible**

Measured, byte for byte, and **identical to the audit's own probe output**:

| Value | `size()` | bytes | Expected |
|---|---|---|---|
| `StandardFormat()` | 2 | `0, 48` | `""` |
| `StandardFormat('\0')` | 1 | `0` | `""` |
| `StandardFormat('\0', 0)` | 2 | `0, 48` | `""` |
| `Parse("")` | 2 | `0, 48` | `""` |
| `StandardFormat('G')` | 1 | `"G"` | unchanged |
| `StandardFormat('D',3)` | 2 | `"D3"` | unchanged |

**Repair.** `if (format_ == 0) return std::string();` at the head of `ToString`, matching .NET's
internal `Format` which tests `symbol != default` first. `sizeof(StandardFormat)` is **2** and
does not change.

### 4.8 SR-AUD-087 — the segment chain cannot build a sequence (medium) → **#2058 blocked + #2061 disclosure**

Re-verified by search: `ReadOnlySequenceSegment<T>` appears **only** in its own header and its
four node-local tests. `ReadOnlySequence<T>` has no segment constructor and no multi-segment
state, and `getIsSingleSegmentProperty()` returns a hard-coded `true`. The header's class
comment — *"A ReadOnlySequence&lt;T&gt; can be constructed from a chain of these segments"* — is
**false today**.

Implementing it is a redesign: `ReadOnlySequence<T>` would gain segment pointers, `TryGet`,
`Slice`, `GetPosition`, `First`, `CopyTo`, `ToArray` and the enumerator would all become
multi-segment, and `SequenceReader<T>`'s single-`First()` snapshot would have to follow.
**Blocked.** The compatible half is #2061: say what the type can actually do today.

### 4.9 SR-AUD-086 — leading `+` on the D/G integer grammar (medium) → **#2060, deferred verification**

Reproduced, and the **internal** inconsistency is real: `tryParseUInt` requires `p[0]` to be a
digit and `tryParseInt` recognises only `-`, while `tryParseGrouped` (the `N` grammar) accepts
`+` **unconditionally — for signed and unsigned alike** (`else if (c == '+')` is not gated on
`allowMinus`). So `+42` parses under `'N'` and fails under `'G'`/`'D'`/default, for the same type.

**But the finding's .NET premise cannot be settled from repository-contained evidence, and it is
two claims, not one.** That .NET's *signed* decimal parser accepts `+` and that its *unsigned*
decimal parser accepts `+` are independent facts; the reference tree is absent, no archived
vector in this repository exercises either, and the audit report asserts both in one sentence
without quoting the unsigned path. Widening an accepted input set on an unverified premise is
exactly what §3 forbids. **#2060 defers**, states precisely which two files must be read
(`Utf8Parser.Integer.Signed.D.cs` and `Utf8Parser.Integer.Unsigned.D.cs`), and #2061 pins the
current behaviour of all four combinations so the answer, when it arrives, lands against a
measured baseline.

### 4.10 SR-AUD-081 — already corrected — **no action**

Ticket #1819 established on 2026-07-29 that the premise is **inverted**: .NET's own
`BasicDecodingWithExtraWhitespaceShouldBeCountedInConsumedBytes` expects `4 + i` consumed for
`"AQ==" + whitespace(i)`, 27 of 27 replayed vectors already match this port, and four permanent
regressions pin it. It keeps `confirmed` only because the index vocabulary has no false-positive
value. **This review does not reopen it and creates no ticket for it.** Recorded here so it
cannot be mistaken for an un-investigated defect at the next review.

### 4.11 SR-AUD-088 — `MemoryHandle`'s RAII claim (medium) → **#2059 blocked + #2061 disclosure**

`MemoryHandle`'s comment says callers *"should call Dispose() explicitly (**or let the destructor
do it**)"*. There is no destructor that calls `Dispose`; the inherited `~IDisposable` does
nothing. Scope exit does **not** unpin.

Adding `~MemoryHandle() { Dispose(); }` is not a small fix: `MemoryHandle` is a 24-byte
aggregate with **public** `pointer_`/`pinnable_` members and an implicit copy constructor, so a
destructor that unpins would make every copy double-unpin. A correct repair needs move-only
semantics or a refcount — a public semantic change to a type included by `Memory.hpp` and
`ReadOnlyMemory.hpp`, i.e. reaching all of `Core.Base`. **Blocked (CCF-019).** #2061 deletes the
false half of the sentence.

---

## 5. Structural root-cause families

Grouped by cause, not by file. Two map onto existing CCF families and must not spawn duplicates;
one is new to this module and is deliberately **not** minted as a CCF yet.

### 5.1 B-A — caller-supplied metadata reaches memory before it is validated → **CCF-005**

Members: SR-AUD-072 (`ReadOnlySequence(const T*, intcs)`), SR-AUD-073
(`ReadOnlySequence::TryGet`), and the post-audit `ArrayBufferWriter` growth defect of §5.2 in its
"before validation" aspect. The shape is CCF-005's exactly — *"BitConverter's typed vector
decoders omit all index/remaining-width validation and reach ASan-confirmed out-of-bounds
reads; Span permits a negative public length"* — a public door that turns a number or a pointer
into a memory region before asking whether it may. **No new CCF.** Both repairs are the same
policy applied twice: *validate, then form the view*.

### 5.2 B-B — signed C++ overflow at a public boundary → **CCF-004**

One member, post-audit, not in the index. `ArrayBufferWriter<T>::checkAndResizeBuffer` computes

```cpp
buffer_.resize(static_cast<std::size_t>(currentLength + growBy));
```

in `intcs`. Measured, with a one-element writer and `GetSpan(2147483647)`:

```
ArrayBufferWriter.hpp:42:71: runtime error: signed integer overflow:
    1 + 2147483647 cannot be represented in type 'int'
terminate called after throwing an instance of 'std::length_error'
```

.NET's `ArrayBufferWriter` performs the same addition but C#'s unchecked `int` **wraps by
definition**, and .NET immediately catches the wrap with `if ((uint)newSize > MaxArrayLength)`
and throws `OutOfMemoryException`. In C++ the wrap is undefined behaviour, so the port cannot
copy the idiom — it must widen or check first. The observable consequence today is doubly wrong:
UB, and then a **native** `std::length_error` escaping instead of `System::OutOfMemoryException`.
This is CCF-004's shape (*"a public .NET-shaped operation that needs well-defined
two's-complement or checked arithmetic, but performs a signed C++ operation first"*) and the
audit's own "Other missing assertions" note for this file predicted it. **Ticket #2051.**

### 5.3 B-C — a public generic surface silently requires more of `T` than it documents (new, module-local)

Members: SR-AUD-070 (four sites), SR-AUD-077 (one site). Root cause: the port implements a .NET
generic with an *unconstrained* `T` on top of a C++ standard container that constrains it, and no
public text says so. This is **not** an existing CCF and this review does **not** mint one: two
findings inside one module is not yet a cross-cutting pattern. §22 records the promotion rule —
if a second module's review finds the same shape (a plausible candidate is
`Collections.Core`'s `T{}` use), it should become CCF-021 then, with both modules' evidence.

### 5.4 B-D — no terminal disposed state, and a borrowed view with no liveness → **CCF-019**

Members: SR-AUD-071 (both halves), SR-AUD-088. Both are *"a copyable public handle retaining a
raw pointer with no owner liveness"*, which is CCF-019 verbatim. **No new CCF**; both blocked
tickets cite it, and neither is claimed to close it.

### 5.5 B-E — the advertised surface is absent, and the documentation says otherwise

Members: SR-AUD-087 (segment chain), SR-AUD-074 (default vs `Empty`), SR-AUD-076's second half
(pool limits), `MemoryManager<T>`'s `Memory`/`CreateMemory` (already honestly documented as
`NotSupportedException` — the model the others should follow), and SR-AUD-088's RAII sentence.
The family's policy: **a header may document a gap, but it may not document a capability it does
not have.** #2061 applies that policy to the four false statements; #2053, #2057, #2058, #2059
carry the implement-it halves.

### 5.6 B-F — a contiguous shape walked as if it were not (post-audit, not in the index)

`BuffersExtensions::PositionOf` re-slices and materialises the whole sequence once **per
element**:

```cpp
for (long long i = 0; i < len; ++i) {
    auto slice = source.Slice(source.getStartProperty(), source.GetPosition(i + 1));
    auto arr = slice.ToArray();                 // copies i+1 elements
    if (!arr.empty() && arr.back() == value) return source.GetPosition(i);
}
```

`Slice` copies the **entire** backing vector (`result.data_ = data_`), and `ToArray` copies the
prefix. Measured with a counting `operator new`, searching for the last element:

| n | allocations | bytes allocated | ms |
|---|---|---|---|
| 1000 | 2,000 | 6,002,000 | 0.14 |
| 2000 | 4,000 | 24,004,000 | 0.31 |
| 4000 | 8,000 | 96,008,000 | 0.95 |
| 8000 | 16,000 | **384,016,000** | 9.28 |

Bytes quadruple for every doubling of `n` — Θ(n²) exactly. A 32 KB sequence allocates **384 MB**.
The sequence is a single contiguous vector by construction, so the correct implementation is one
linear scan with **zero** allocations and identical return values. **Ticket #2055.**

---

## 6. Corrected premises

Recorded in the plan rather than by rewriting the audit reports; the historical text stays.

| # | The finding says | Measured |
|---|---|---|
| 6.1 | SR-AUD-072: *"A negative length likewise performs pointer arithmetic before any range error can be represented."* | The pointer arithmetic is indeed formed, but the **observable** outcome is not a memory fault: `std::vector`'s range constructor throws `std::length_error("cannot create std::vector larger than max_size()")`. UBSan reports nothing for this mode. The defect is real but its class is **a native exception escaping a public door**, not an out-of-bounds access — which changes the repair's acceptance criterion from "no ASan report" to "the declared `ArgumentOutOfRangeException` is thrown". |
| 6.2 | SR-AUD-073 is framed around a *"forged"* position and cites SR-AUD-069's mutable representation as the enabler. | Forgery is **not required**. `seq.getStartProperty()` is a legitimately obtained position; held across `seq.Slice(...)` and passed to the slice's `TryGet`, it produces the same out-of-slice view. The finding therefore understates reachability — it is an ordinary caller mistake, not an attack — and the repair must validate the *range*, which fixes both paths, rather than merely checking the segment marker. |
| 6.3 | SR-AUD-071 is one finding. | It is **two** with different costs: the post-dispose getter (needs 8 bytes in `MemoryPoolHeapOwner_`, measured 32 → 40, no padding available) and the retained view (needs a `Memory<T>` ownership change in `Core.Base`). Treating them as one would have blocked a disclosure that is available today, or smuggled a layout change in behind a "high severity" label. |
| 6.4 | SR-AUD-086 asserts .NET accepts a leading `+` *"for signed and unsigned values"* in one sentence. | Two independent claims about two different .NET files, neither verifiable here. The **internal** inconsistency (N accepts `+`, D/G does not, for the same type) is confirmed and is the part this repository can act on. Deferred rather than implemented. |
| 6.5 | SR-AUD-070's site list is `ArrayBufferWriter` + `MemoryPool`. | **Four** production sites, counted independently: `checkAndResizeBuffer`, `Clear`, `MemoryPoolHeapOwner_`'s ctor, and `SharedArrayPool<T>::Rent` + `ArrayPool<T>::Return(clearArray=true)`. The `ArrayPool` site is only a cross-reference note in that file's report. |
| 6.6 | The module's open count is 11 (`audit/AUDIT_FINDINGS_INDEX.md`, grouped by path). | The **namespace**'s open count is **12**: SR-AUD-088 owns `System::Buffers::MemoryHandle` but is indexed under `modules/core` by path. The finding's own `Source` column names `IPinnable.hpp`, in this module. |
| 6.7 | The `ArrayBufferWriter` report lists the growth arithmetic only under *"Other missing assertions"*, i.e. as untested rather than as a defect. | It is a **reachable UBSan-confirmed signed overflow** requiring no large allocation: a one-element writer and `GetSpan(INT_MAX)` reach it. Promoted from "untested" to a ticket. |
| 6.8 | Nothing in the audit mentions `BuffersExtensions::PositionOf`'s cost. | Θ(n²) time and bytes, 384 MB allocated for a 32 KB sequence. A post-audit defect with an ordinary ticket number, **no `SR-AUD-*` identifier**. |

---

## 7. Dependency graph between the tickets

```
#2048 (this review)
 ├── #2049  ReadOnlySequence raw ctor validation      ── independent
 ├── #2050  ReadOnlySequence::TryGet validation       ── independent
 ├── #2051  ArrayBufferWriter growth overflow         ── independent
 ├── #2052  StandardFormat::ToString                  ── independent
 ├── #2053  ArrayPool::Create validation              ── independent
 ├── #2054  explicit generic requirements             ── independent
 ├── #2055  BuffersExtensions::PositionOf             ── depends on nothing, but its test
 │                                                        asserts equality with #2049's
 │                                                        contract for an empty sequence
 ├── #2061  disclosure + pins                         ── MUST land after #2049/#2050/#2052/#2053
 │                                                        (it pins what those leave unchanged)
 ├── #2056  disposed IMemoryOwner        [blocked]    ── pinned by #2061
 ├── #2057  default vs Empty             [blocked]    ── pinned by #2061
 ├── #2058  multi-segment sequence       [blocked]    ── supersedes #2057 if approved
 ├── #2059  MemoryHandle RAII            [blocked]    ── CCF-019, touches Core.Base
 └── #2060  leading '+' verification     [todo]       ── pinned by #2061
```

`#2058` genuinely supersedes `#2057`: a real multi-segment representation would carry the
default/`Empty` discriminator for free. They must not be implemented in the other order.

---

## 8. Priority and severity

| Ticket | Priority | Rationale |
|---|---|---|
| #2049, #2050 | **P1** | memory safety on a public door, ASan/UBSan-confirmed |
| #2051 | **P1** | undefined behaviour reachable with a two-line caller |
| #2055 | P2 | no correctness defect, but a 12,000× allocation amplification |
| #2052, #2053, #2054 | P2 | observable parity / contract defects, no memory consequence |
| #2061 | P2 | four false public statements; blocks nothing but unblocks honesty |
| #2056, #2057, #2058 | P2 | high/medium findings whose repair needs approval |
| #2059 | P3 | CCF-019, and the smallest of the three CCF-019 members |
| #2060 | P3 | deferred verification, no evidence available in this container |

---

## 9. Compatible versus approval-sensitive

### 9.1 Compatible (no approval needed)

| Ticket | Why compatible |
|---|---|
| #2049 | adds validation before undefined behaviour; no signature, member, virtual or layout change |
| #2050 | same; `pos == end_` keeps its pinned `false` result |
| #2051 | widens an internal computation to `longcs` and throws the declared `System::OutOfMemoryException`; `sizeof(ArrayBufferWriter<char>)` stays **40** |
| #2052 | one early return inside an existing `const` method; `sizeof(StandardFormat)` stays **2** |
| #2053 | adds argument validation to an existing `static` factory; return type unchanged |
| #2054 | documentation plus `static_assert`s placed where the failure already occurs — **the same set of programs compiles** |
| #2055 | replaces a body with a linear scan returning identical values |
| #2061 | documentation and tests only; **zero executable production change** |

### 9.2 Approval-sensitive (blocked)

| Ticket | What it needs |
|---|---|
| #2056 | +8 bytes in `MemoryPoolHeapOwner_<T>` (32 → 40) **and** a semantic change from "returns empty" to "throws `ObjectDisposedException`" |
| #2057 | a discriminator member in the public `ReadOnlySequence<T>` (32 → 40) and a changed `MoveNext` result for one input |
| #2058 | a new public constructor, new members, and multi-segment rewrites of eight methods plus the enumerator and `SequenceReader<T>` |
| #2059 | a destructor and move-only or refcounted semantics on a copyable public aggregate reachable from all of `Core.Base` |

---

## 10. Source-compatibility impact

| Ticket | Source-compatible? | Note |
|---|---|---|
| #2049 | **yes** | only previously-UB or native-throwing calls change |
| #2050 | **yes** at compile time; a caller relying on the out-of-slice read gets an exception |
| #2051 | **yes** | a caller that reached the UB now gets `OutOfMemoryException` |
| #2052 | **yes** | text of a degenerate value changes; no caller in this repository formats a zero symbol |
| #2053 | **yes** | both in-repository call sites pass positive values |
| #2054 | **yes** — deliberately; a class-scope assert (rejected) would **not** have been |
| #2055 | **yes** | identical return values |
| #2061 | **yes** | documentation and tests |

No migration document is required by this batch. If #2056 or #2057 is ever approved, each needs
one.

---

## 11. ABI, layout, vtable, mangled-symbol and `noexcept` impact

`build-probe/2048_probe3_layout.cpp`, measured before any change:

| Type | `sizeof` | `alignof` |
|---|---|---|
| `ReadOnlySequence<int>` | **32** | 8 |
| `ReadOnlySequence<int>::Enumerator` | **16** | 8 |
| `MemoryPoolHeapOwner_<int>` | **32** | 8 |
| `DefaultMemoryPool_<int>` | 8 | 8 |
| `ArrayBufferWriter<char>` | **40** | 8 |
| `StandardFormat` | **2** | 1 |
| `SequenceReader<uint8_t>` | 32 | 8 |
| `MemoryHandle` | **24** | 8 |
| `System::SequencePosition` | 16 | 8 |

**Every compatible ticket leaves all nine unchanged**, and #2061 adds `static_assert`s pinning the
five that a gated option would have to move. No virtual is added, removed or re-ordered; no
`noexcept` specification changes. `ReadOnlySequence`'s constructors are **not** currently
`noexcept` and #2049 does not make them so — a constructor that throws must not be, and the
existing ones already allocate.

One `noexcept` note, deliberate: `ArrayBufferWriter::Advance` and the getters keep their current
specifications; #2051 touches only the private `checkAndResizeBuffer`, which is not `noexcept`
today.

---

## 12. Ownership and lifetime consequences

- **Nothing in the compatible queue changes who owns anything.** `ReadOnlySequence<T>` keeps
  copying its source into its own vector, `ArrayPool<T>::Rent` keeps returning a
  caller-owned `std::vector<T>` (so `Return` is advisory — recorded in §21), `MemoryPool<T>`
  keeps handing out a `unique_ptr<IMemoryOwner<T>>`.
- **#2049 makes the copy explicit at the door**: rejecting `(nullptr, n>0)` removes the only
  path by which the constructor could read memory the caller does not own.
- **The two real lifetime defects stay open and are pinned, not repaired**: a `Memory<T>`
  retained across `IMemoryOwner::Dispose` (SR-AUD-071b) and a `MemoryHandle` that never unpins
  (SR-AUD-088). Both are CCF-019 and both are blocked.
- `SequenceReader<T>` holds a `const ReadOnlySequence<T>&`, so it dangles if built from a
  temporary. Documented in the header today; **not** changed here, recorded in §21.

---

## 13. Accepted and rejected input consequences

| Ticket | Input previously accepted, now rejected | Input previously rejected, now accepted |
|---|---|---|
| #2049 | `(nullptr, n>0)` → `ArgumentNullException`; `(ptr, n<0)` → `ArgumentOutOfRangeException` (was `std::length_error`) | none |
| #2050 | a position outside `[start_, end_]` → `ArgumentOutOfRangeException` | none |
| #2051 | a growth request whose total exceeds `MaxArrayLength` → `OutOfMemoryException` (was UB) | none |
| #2053 | `maxArrayLength <= 0` or `maxArraysPerBucket <= 0` → `ArgumentOutOfRangeException` | none |
| #2052, #2055, #2054, #2061 | none | none |

**No ticket in this batch widens an accepted input set.** The one candidate that would have —
#2060's leading `+` — is deferred precisely because widening on unverified evidence is the
failure mode §3 exists to prevent.

---

## 14. Overlap and partial-write consequences

`System::Buffers` has three surfaces where a partial write before a failure would be observable,
and all three are checked:

- **`ReadOnlySequence::CopyTo(Span<T>)`** validates the destination length **before** `std::copy`,
  so a too-small destination is untouched. Unchanged by this batch; pinned by #2061.
- **`BuffersExtensions::Write`** advances the writer per chunk by construction; a failure inside
  `Advance` leaves earlier chunks written. That is the API's own contract (the writer *is* the
  destination) and is not changed.
- **`ArrayBufferWriter::GetSpan`/`GetMemory`** must either return a valid span or throw with the
  buffer unchanged. **#2051's acceptance criterion includes exactly this**: after the
  `OutOfMemoryException`, `getCapacityProperty()` and `getWrittenCountProperty()` must be what
  they were before the call — today the UB path can leave the vector resized.

Source/destination overlap does not arise inside this namespace: `ReadOnlySequence` owns a
private copy, and the overlapping-`CopyTo` family (SR-AUD-044) belongs to `Span`/`Memory` in
`modules/core` and is explicitly **not** claimed here (§21).

---

## 15. Pool-clearing and security consequences

- `ArrayPool<T>::Return(array, clearArray)` clears **only** when asked, matching .NET. Because
  `Rent` returns a by-value `std::vector<T>`, the caller still owns the storage after `Return`,
  so clearing is a courtesy rather than a guarantee that the data is gone. **#2053 does not
  change this**, and #2061 says so in the header — the previous text left a reader to infer .NET's
  ownership transfer.
- **No sensitive-data retention change is made by this batch, and none is approved.** Making
  `Return` always clear, or making the shared pool zero on `Rent`, is a broad pool-clearing
  policy change the approval boundary forbids.
- **The one genuine security-adjacent defect is SR-AUD-071b** — a retained `Memory<T>` reading
  storage the owner has released. Blocked as #2056; pinned by #2061 so it cannot be quietly
  reclassified as safe.

---

## 16. Concurrency consequences

- Two process-wide singletons: `ArrayPool<T>::Shared()` and `MemoryPool<T>::Shared()`, both
  Meyers singletons, so **initialisation** is thread-safe by C++11 guarantee.
- Both are also **stateless** — `SharedArrayPool<T>::Rent` allocates a fresh vector and
  `DefaultMemoryPool_<T>::Rent` a fresh owner; neither keeps a free list. Concurrent `Rent` is
  therefore data-race-free today, and this batch keeps it that way: **no ticket adds shared
  mutable state to either singleton.**
- Consequently **TSan has no subject in the compatible queue** and this review does not claim a
  TSan result for it — §17 states that as a non-result rather than reporting a vacuous pass.
  #2056 and #2058 would change that: a real pool with a free list, or a shared segment chain,
  needs TSan before it lands.
- `SearchValues<T>` is immutable after construction and `Contains` is `const`; concurrent reads
  are safe. Pinned by #2061.

---

## 17. Test matrix

Buffers suite baseline: **536 tests** in `SharpRuntimeTests_Buffers` at `27061bf`.

| Ticket | Required cases |
|---|---|
| #2049 | `(nullptr,0)` still empty (2 existing tests must stay green); `(nullptr,1)` throws `ArgumentNullException`; `(nullptr, INTCS_MAX)` throws the same; `(ptr,-1)` throws `ArgumentOutOfRangeException`; `(ptr,INTCS_MIN)` throws; `(ptr,0)` empty; `(ptr,1)` reads the one element; the thrown parameter names are exact |
| #2050 | forged `(nullptr,0)` on a `[1,3)` slice throws; `(nullptr,-1)` throws; `getStartProperty()` of the parent on a slice throws (§6.2's non-forged path); `pos == end_` still returns `false` with an empty memory; `pos == start_` still returns the whole remainder; `advance=true` still lands on `getEndProperty()`; a position **past** `end_` still returns `false` (not a throw — pinned, unchanged) |
| #2051 | `GetSpan(INTCS_MAX)` on a 1-element writer throws `OutOfMemoryException` **and leaves capacity and written count unchanged**; `GetMemory(INTCS_MAX)` likewise; `GetSpan(0)` on an empty writer still gives ≥ 256; ordinary growth 1 → 256 → 512 unchanged; `Advance` past capacity still `InvalidOperationException`; negative hint still `ArgumentException` |
| #2052 | `ToString()` empty for `default`, `('\0')`, `('\0',0)`, `Parse("")`; `size()==0` asserted explicitly, not just equality with `""`; `'G'`, `"D3"`, `"F99"` unchanged; `Parse(ToString())` round-trip still holds for non-zero symbols |
| #2053 | `Create(0,1)`, `Create(1,0)`, `Create(-1,10)`, `Create(10,-1)`, `Create(0,0)` all throw `ArgumentOutOfRangeException` with the right parameter name; `Create(1,1)` and `Create(1024,10)` still return a usable pool; `Create()` unchanged |
| #2054 | a non-default-constructible `T` still fails to compile — **as a negative consumer fixture site**, not a runtime test — with the new diagnostic text; every default-constructible instantiation still compiles |
| #2055 | first element, last element, middle, absent, empty sequence, single element, a sliced sequence (the returned position must be absolute), duplicate values return the **first**; and an allocation-count assertion is **not** used (it is not portable) — the scaling evidence stays in the probe log |
| #2061 | pins for: post-dispose `getMemoryProperty()` returning empty (SR-AUD-071a); retained-view length after `Dispose` (071b); `default` and `Empty` both enumerating 1 (074); `+42` under `'N'` vs `'G'`/`'D'`/default for signed **and** unsigned (086); the segment chain being unusable (087); `MemoryHandle` not unpinning at scope exit (088); `CopyTo` leaving a short destination untouched; the five `sizeof` values as `static_assert`s |

Every pin in #2061 must be **mutation-checked**: apply the change it guards, confirm the pin
fails, revert, confirm `git diff` is empty.

---

## 18. Ticket split

### 18.1 Compatible, ready

| # | P | Size | Scope | Findings | Family |
|---|---|---|---|---|---|
| **#2049** | P1 | S | validate `ReadOnlySequence(const T*, intcs)` | SR-AUD-072 | CCF-005 / B-A |
| **#2050** | P1 | S | validate `ReadOnlySequence::TryGet`'s position | SR-AUD-073 | CCF-005 / B-A |
| **#2051** | P1 | S | `ArrayBufferWriter` growth: no signed overflow, `OutOfMemoryException` | post-audit | CCF-004 / B-B |
| **#2052** | P2 | XS | `StandardFormat::ToString` of a zero symbol | SR-AUD-083 | B-E |
| **#2053** | P2 | S | `ArrayPool<T>::Create(intcs,intcs)` validates its configuration | SR-AUD-076 | B-E |
| **#2054** | P2 | S | make the four implicit `T` requirements explicit and diagnosed | SR-AUD-070, 077 | B-C |
| **#2055** | P2 | S | `BuffersExtensions::PositionOf` linear scan | post-audit | B-F |
| **#2061** | P2 | M | disclosure + behaviour pins for everything gated | 071, 074, 086, 087, 088 | B-D / B-E |

### 18.2 Blocked on approval

| # | P | Scope | Findings |
|---|---|---|---|
| **#2056** | P2 | terminal disposed state for `IMemoryOwner<T>` — layout + semantics | SR-AUD-071 |
| **#2057** | P2 | distinguish `default` from `Empty` — public layout | SR-AUD-074 |
| **#2058** | P2 | multi-segment `ReadOnlySequence` from a segment chain | SR-AUD-087 |
| **#2059** | P3 | `MemoryHandle` RAII and copy semantics — CCF-019 | SR-AUD-088 |

### 18.3 Deferred verification

| # | P | Scope | Findings |
|---|---|---|---|
| **#2060** | P3 | does .NET's D/G integer grammar accept a leading `+`, for signed and for unsigned? | SR-AUD-086 |

---

## 19. Recommended execution order

1. **#2049** — the constructor, because #2050's tests build sequences and #2055's empty-sequence
   case depends on the constructor's settled contract.
2. **#2050** — `TryGet`.
3. **#2051** — the growth overflow; independent, but it is the third P1 and belongs with them.
4. **#2052** — smallest correctness fix, no dependencies.
5. **#2053** — the factory validation.
6. **#2055** — `PositionOf`.
7. **#2054** — the generic requirements; last of the compatible set because it needs a new
   negative-consumer-fixture site, which is the only one that touches `test/consumer/` and the
   fixture checker.
8. **#2061** — must be last: it pins what steps 1–7 deliberately left alone, and pinning before
   they land would pin the wrong baseline.

Then stop. #2056–#2059 need approval; #2060 needs the reference tree.

---

## 20. Sanitizer matrix

| Ticket | ASan | UBSan | LSan | TSan |
|---|---|---|---|---|
| #2049 | **discriminating** — `SEGV` on the null read present before, absent after | **discriminating** — `load of null pointer` present before, absent after | n/a | **no subject** |
| #2050 | **discriminating** — `heap-buffer-overflow` READ, 4 bytes before the region, present before, absent after | not expected to fire (the read is in-type, just out of the allocation) — **stated as a predicted non-result**, to be reported honestly either way | n/a | **no subject** |
| #2051 | not expected | **discriminating** — `signed integer overflow: 1 + 2147483647` at `ArrayBufferWriter.hpp:42` present before, absent after | n/a | **no subject** |
| #2052, #2053, #2055 | expected clean, non-discriminating; reported as such | same | #2055 additionally: LSan over the linear scan should show the allocation count fall to **zero**, and that *is* discriminating | **no subject** |
| #2054 | n/a — compile-time only | n/a | n/a | n/a |
| #2061 | expected clean | expected clean | n/a | **no subject** |

Rules this batch binds itself to, from the prompt and from the previous batch's practice:

- a conclusion may only be drawn from a binary in which the affected body is **compiled from
  source** (these headers are header-only, so any probe that includes them qualifies — but the
  probe must be rebuilt after the change, and the `before` column must come from a
  `build-probe/*_before_include/` tree produced by `git show`);
- a non-discriminating result is reported as a **non-result**, never as evidence;
- **TSan has no subject anywhere in the compatible queue** (§16) and no TSan claim will be made
  for it.

---

## 21. Explicit exclusions

Named so a later reader knows they were considered and declined, not missed.

1. **SR-AUD-069** (`System::SequencePosition`'s public mutable representation) — a `System`
   namespace type in `modules/core`, and making it opaque is a public-surface change to a type
   `ReadOnlySequence`, `SequenceReader` and `BuffersExtensions` all expose. #2050 makes the
   forged-position path safe **without** it, which is the point of validating the range rather
   than the segment marker.
2. **SR-AUD-044** (overlapping `CopyTo` on `Span`/`Memory`) — `modules/core`, a different review
   unit, and not reachable through any `System::Buffers` surface.
3. **The Base64 family** — `docs/Base64FamilyPlan.md`, four of five findings `remediated` and the
   fifth a corrected false positive (§4.10). **Not reopened**; no current buffers finding proves
   a separate remaining defect.
4. **Honouring `ArrayPool`'s configured limits** (SR-AUD-076's second half) — needs a new public
   configured-pool class; §4.6.
5. **`Utf8Parser`'s absent `Guid`/date/float/`Decimal` overloads** — a documented API-baseline gap,
   not a defect; the audit itself asks for it to be retained separately.
6. **`SequenceReader<T>`'s reference-to-sequence lifetime** — documented, and changing it to a
   copy is a semantic and layout change (32 → 56+).
7. **`ArrayPool`'s ownership adaptation** (`Rent` returns a by-value vector, so the caller keeps
   the storage after `Return`) — a deliberate C++ adaptation; #2061 documents it, no ticket
   changes it.
8. **`SequenceReaderExtensions::detail::isBigEndian`'s union type-punning** — UB in ISO C++,
   a documented GCC/Clang extension in practice, and `std::endian::native` is the portable
   replacement. Not reproduced as a misbehaviour in this container; recorded here and left for a
   future batch rather than changed on a theoretical basis.
9. **`modules/net-http`** — the next review unit by §1's table, explicitly out of scope for this
   context.

---

## 22. Completion criteria

This review (#2048) is complete when this document exists, every one of the twelve open findings
has exactly one disposition in §4, and §18's tickets are in `plan.sqlite3`. **It is complete on
those terms and remediates nothing by itself.**

The `System::Buffers` namespace is closed for *compatible* work when:

1. #2049, #2050, #2051, #2052, #2053, #2054, #2055 and #2061 are `done`;
2. SR-AUD-072, 073, 076, 083, 070 and 077 are `remediated` in
   `audit/AUDIT_FINDINGS_INDEX.md` and in their per-file reports, with the historical text
   retained and a dated remediation note appended;
3. SR-AUD-071, 074, 087 and 088 are `confirmed (design-complete)` with a blocked ticket and a
   behaviour pin each;
4. SR-AUD-086 carries a deferred-verification ticket and a behaviour pin;
5. SR-AUD-081 keeps its existing correction and no new ticket;
6. the repository gate shows no new failure and the buffers suite has grown, add-only;
7. the nine `sizeof` values of §11 are unchanged and five are `static_assert`ed.

**Promotion rule for family B-C** (§5.3): if a second module's review finds a public generic
surface with an undocumented implicit `T` requirement, mint CCF-021 then, citing both modules.
Do not mint it from this module's two findings alone.
