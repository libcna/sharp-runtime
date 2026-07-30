<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
<!-- Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors) -->

# Conversion / memory-safety boundary family — CCF-005 plan

*Authored 2026-07-30 by the batch on branch
`feature/remediation-batch-ccf003-ccf005-plan`, immediately after CCF-003 closed.
This is the durable, evidence-based plan for the **memory-safety slice of
CCF-005** — the highest-value remaining audit family, five `high`-severity
findings, three of them ASan-confirmed out-of-bounds. It is the analogue of
[`docs/Base64FamilyPlan.md`](Base64FamilyPlan.md) and
[`docs/NumericWrapperBoundaryPlan.md`](NumericWrapperBoundaryPlan.md), and is
written to the same standard: every finding re-verified against **current
source** and the .NET reference (`/rv/tmp/runtime/src/libraries/`), premises
corrected in place, work split into bounded dependency-ordered tickets, and the
implementation-vs-approval boundary drawn explicitly.*

**This document creates no `SR-AUD-*` identifier** (audit numbering is frozen at
364). It maps the existing findings to files, corrects premises, groups the work
into tickets, and records what must be approved before it can be done.

Current-state verification was performed 2026-07-30 by four parallel read-only
agents over the production sources, the per-file audit reports, and the .NET
reference. **All five findings still reproduce in the current tree; none is
remediated.**

---

## 1. Complete file and symbol inventory

| File | Symbols in scope | Finding |
|---|---|---|
| `modules/core/include/System/Convert.hpp` | `ToChar(intcs)` L91, `ToByte(longcs)` L152, `ToUInt32(intcs)` L373, `ToUInt32(longcs)` L375, `ToUInt64(intcs)` L406, `ToUInt64(longcs)` L408; `ToUInt32(double)` L385, `ToUInt64(double)` L425 | SR-AUD-026, 027 |
| `modules/core/src/System/Convert.cpp` | `ToInt32(double)` L66-73, `ToInt64(double)` L91-105 | SR-AUD-027 |
| `modules/core/include/System/BitConverter.hpp` | 14 vector decoders `To{Boolean,Char,Int16,UInt16,Int32,UInt32,Int64,UInt64,Single,Double,Half,BFloat16,Int128,UInt128}(const std::vector<bytecs>&, intcs)` L151-177; raw-pointer overloads L118-144 | SR-AUD-041 |
| `modules/core/include/System/Span.hpp` | `Span(T*,intcs)` L54, `Span(vector&)` L60, `ReadOnlySpan(const T*,intcs)` L301, `ReadOnlySpan(vector&)` L307 | SR-AUD-043 |
| `modules/core/include/System/Memory.hpp` | `Memory(vector&)` L57-59 | SR-AUD-043 |
| `modules/core/include/System/ReadOnlyMemory.hpp` | `ReadOnlyMemory(const T*,intcs)` L49-50 *(constexpr noexcept)*, `ReadOnlyMemory(vector&)` L58-59 *(noexcept)*, `ReadOnlyMemory(ArraySegment)` L67-69 *(noexcept)* | SR-AUD-043 |
| `modules/core/include/System/ArraySegment.hpp` | `ArraySegment(vector&)` L51-52 | SR-AUD-043 |
| `modules/core/include/System/HashCode.hpp` | `AddBytes(const ReadOnlySpan<uint8_t>&)` L92-94 *(noexcept)*, raw `AddBytes(const uint8_t*, size_t)` L74-77 | SR-AUD-043 |
| `modules/core/include/System/MemoryExtensions.hpp` | static `CopyTo(ReadOnlySpan<T>, Span<T>)` L425-428, `CopyTo(Span<T>, Span<T>)` L430-434 | SR-AUD-047 |

The five findings are all in `Core.Base` and `Collections`-adjacent core headers;
none has a downstream migration prerequisite for *adding validation* (the
exception types — `OverflowException`, `ArgumentOutOfRangeException`,
`ArgumentException` — are already in `Core.Base`).

---

## 2. Public-surface inventory

- **`Convert` static converters** — stateless utility class (`= delete` ctor, no
  members, no vtable). Six integral overloads (026) + four float→int overloads
  (027). All `[[nodiscard]] static`, **none `noexcept`**.
- **`BitConverter` typed decoders** — 14 `static` vector overloads (041). **None
  `noexcept`.** The raw-pointer overloads they forward to have no length and are
  documented-precondition APIs (like the existing `ToString(const bytecs*, …)`).
- **View constructors** — `Span`, `ReadOnlySpan`, `Memory`, `ArraySegment`
  pointer/length and vector ctors (043). Storage field is **signed `intcs`
  (`int32_t`)** in every case, so a negative value survives and only becomes a
  huge unsigned quantity at a later `size_t` use. `ReadOnlyMemory`'s three ctors
  are `noexcept` (one also `constexpr`); the rest are not.
- **`HashCode::AddBytes`** — `noexcept`; casts the signed span length straight to
  `size_t` (043).
- **`MemoryExtensions::CopyTo`** — two `static` templated overloads (047). **Not
  `noexcept`.** (The member `Span::CopyTo`/`ReadOnlySpan::CopyTo` already
  validate; only the static helper does not.)

No virtual functions, no iterator layout, no object layout in any of these
surfaces. The single non-layout signature concern is `noexcept` (see §12/§13).

---

## 3. Finding-by-finding current-state verification (measured 2026-07-30)

| Finding | Sev | Measured current behaviour | .NET reference | Reproduces? |
|---|---|---|---|---|
| **SR-AUD-026** | high | Six `Convert` overloads `static_cast` with no range guard: `ToChar(-1)=255`, `ToByte(256L)=0`/`ToByte(-1L)=255`, `ToUInt32(-1)=4294967295`, `ToUInt64(-1)=…615`; sibling overloads for the same target types *do* guard | `if (out of range) Throw…OverflowException()` (`Convert.cs`) | **yes** |
| **SR-AUD-027** | high | Four float→int converters use `value < min \|\| value > max`, which **NaN passes**, then cast: `ToInt32(NaN)=INT_MIN`, `ToInt64(NaN)=LLONG_MIN`, `ToUInt32(NaN)=0`, `ToUInt64(NaN)=2^63`. No `isfinite` guard | .NET's `checked((long)Round)` / explicit `throw OverflowException` catches NaN | **yes** |
| **SR-AUD-041** | high | 14 vector decoders forward `v.data()+i` to a raw `memcpy(&r, v.data()+i, sizeof T)` with attacker-controlled signed `i` and **no** `i>=0` / `i+W<=size` check → heap OOB read (`ToInt32(vec4,-1)`, `ToInt32(vec3,0)`) | `ArgumentOutOfRangeException(startIndex)` then `ArgumentException(value)` | **yes (ASan)** |
| **SR-AUD-043** | high | `Span`/`ReadOnlySpan`/`Memory`/`ArraySegment` ctors store a public `intcs` length with no `>=0` check (and vector ctors narrow `size()` unguarded); `HashCode::AddBytes` casts a negative span length to `size_t` → unbounded raw read (`ReadOnlySpan<uint8_t>(oneByte,-1)`) | `if (length < 0) ThrowArgumentOutOfRangeException()` at construction; span stays signed `int` | **yes (ASan)** |
| **SR-AUD-047** | high | static `CopyTo` does `std::copy(source.begin(), source.end(), destination.begin())` with **no** `source.length<=destination.length` check → heap-buffer-overflow **write** (2→1 element) | `if (src.Length <= dst.Length) Memmove; else ThrowArgumentException_DestinationTooShort()` — throw before any write | **yes (ASan)** |

---

## 4. Premise corrections (measured vs. audit text)

Preserve the audit narrative; append these corrections.

1. **SR-AUD-027 spans two files, not one.** The audit filed it against
   `Convert.cpp`, but only `ToInt32(double)`/`ToInt64(double)` live there;
   `ToUInt32(double)` (`Convert.hpp:385`) and `ToUInt64(double)` (`Convert.hpp:425`)
   are inline in the header. All four need the `isfinite` guard.
2. **SR-AUD-041's OOB is in the raw-pointer overloads, but the fix belongs in the
   vector overloads.** The 14 vector decoders discard `v.size()` by forwarding to
   the length-less raw-pointer overloads; the vector overloads are the only ones
   that *can* validate. The raw-pointer overloads stay documented-precondition
   APIs (the file already does this for `ToString(const bytecs*,…)`).
3. **SR-AUD-041 has a `ToBoolean` quirk.** .NET's `ToBoolean(byte[],int)` throws
   only `ArgumentOutOfRangeException` (width 1, no `ArgumentException`); the other
   13 throw `ArgumentOutOfRangeException` then `ArgumentException`.
4. **SR-AUD-043 needs no layout change and .NET agrees.** .NET `Span<T>` stores a
   **signed `int _length`** and validates at construction — so the port must keep
   `intcs length_` and add the construction check, *not* switch to unsigned
   storage (which would break both layout and the `getLengthProperty()→intcs`
   contract). The audit's "narrowing" language is right about the bug but must not
   be read as licence to change the field type.
5. **SR-AUD-043 partially crosses the `noexcept` line.** `Span`/`ReadOnlySpan`/
   `Memory`/`ArraySegment` ctors are **not** `noexcept` → autonomous. But
   `ReadOnlyMemory`'s three ctors (one `constexpr`) and `HashCode::AddBytes` are
   `noexcept`; throwing requires dropping that specifier — an
   exception-specification change that warrants sign-off (§13). Crucially, the
   **reachable exploit closes without touching them**: once `Span`/`ReadOnlySpan`
   reject a negative length, `HashCode::AddBytes` can no longer receive one, so
   the `noexcept` members become defense-in-depth, not the primary fix.
6. **SR-AUD-047 is isolated and the member versions are already correct.**
   `Span::CopyTo`/`ReadOnlySpan::CopyTo` validate; only the static
   `MemoryExtensions::CopyTo` helper does not. The `Fill`/`Reverse`/`Sort`/
   `Replace` bulk ops are in-place (single span) and have no source/destination
   mismatch — not part of this finding.
7. **Not folded in (recorded, separate):** `MemoryExtensions::CopyTo` uses
   forward-only `std::copy`, which mishandles right-overlapping copies — that is
   **SR-AUD-044** (overlap-safety), a distinct finding, out of scope here.

---

## 5. Shared root causes

1. **A native primitive is used without .NET's boundary rule.** A `static_cast`
   (026), a comparison that NaN silently passes (027), a raw `memcpy` at an
   unvalidated index (041), a signed length stored then reinterpreted unsigned
   (043), or a `std::copy` without a destination-fits check (047) — each is a C++
   operation used where .NET first validates.
2. **A green normal-path test suite hides the invalid domain.** Every finding has
   passing tests for ordinary inputs and *none* for the negative/NaN/short-buffer
   input. This is CCF-005's thesis, verified here: **no existing test pins the
   buggy behaviour in any of the five**, so each fix only *adds* the missing
   assertion — nothing is corrected.
3. **`noexcept`/`constexpr` on a boundary function blocks the correct throw** —
   for the `ReadOnlyMemory` ctors and `HashCode::AddBytes` only (§13).

---

## 6. Memory-safety taxonomy

| Finding | Class | Direction | Confirmed by |
|---|---|---|---|
| SR-AUD-041 | out-of-bounds **read** (heap) | read past/before a `std::vector<bytecs>` | ASan (audit probe) |
| SR-AUD-043 | out-of-bounds **read** (heap/stack), unbounded | negative length → `size_t` huge → raw byte loop / index | ASan (audit probe) |
| SR-AUD-047 | out-of-bounds **write** (heap) | `std::copy` past a shorter destination | ASan (audit probe) |
| SR-AUD-026 | value corruption (silent wrap), no memory unsafety | — | value probe |
| SR-AUD-027 | value corruption (spurious integer from NaN), no memory unsafety | — | value probe |

041/043/047 are the memory-corruption core (two reads, one write). 026/027 are
silent-wrong-value defects sharing the same missing-assertion root cause; they
carry no UB but are `high` because they let invalid data flow on silently.

---

## 7. Ownership and lifetime issues

None of the five is an ownership/lifetime defect. The views (`Span`, `Memory`,
`ArraySegment`, `ReadOnlyMemory`) are non-owning by design and their lifetime
contract is unchanged by this work — the fixes only reject invalid *length*
metadata at construction, never alter who owns the storage. `HashCode` and
`Convert`/`BitConverter`/`MemoryExtensions` are stateless/utility. No
`shared_ptr`/RAII ownership edge is touched. (Ownership-family defects are the
CCF-018/020 collection-copy work, already closed under #1771/#1774.)

---

## 8. Bounds, overflow, alignment, aliasing, endianness

- **Bounds** — the whole family (041 index/width; 043 negative length; 047
  destination width). Fix = validate before the memory op.
- **Overflow** — 026 (narrowing/sign wrap); 043 vector ctors narrow `size_t`→
  `intcs` (a `> INT32_MAX` vector silently becomes negative). Fix = range guard
  before the cast.
- **NaN / special value** — 027 (NaN is neither `<min` nor `>max`). Fix =
  explicit `!std::isfinite`.
- **Alignment** — `BitConverter` decoders use `std::memcpy` into a local, so they
  are already alignment-safe; **no alignment defect** and the fix must keep the
  `memcpy` (not switch to a `reinterpret_cast` deref). Recorded so a future
  change does not regress it.
- **Aliasing** — `memcpy` into a distinct local is strict-aliasing-safe; no
  aliasing defect. `MemoryExtensions::CopyTo`'s forward-only `std::copy` has an
  **overlap** hazard, but that is SR-AUD-044, explicitly excluded (§17).
- **Endianness** — `BitConverter` is host-endian by design (matches the port's
  existing contract and `BitConverter.IsLittleEndian`); not in scope, not changed.

---

## 9. Actual .NET / reference behaviour (exact)

- **`Convert` (026):** `OverflowException`. .NET messages via `SR.Overflow_*`
  ("Value was either too large or too small for a <type>."). The port's existing
  sibling guards use "Value is out of <Type> range." — keep the port's style for
  internal consistency (behaviour parity only requires the `OverflowException`).
  **Port nuance:** this port backs `char` with a **1-byte** type, and the existing
  `ToChar(longcs)` sibling uses `[0,255]` — so `ToChar(intcs)`'s new guard must be
  `[0,255]`, not .NET's `[0,65535]`.
- **`Convert` (027):** `OverflowException`. .NET reaches it for NaN via
  `checked((long)Math.Round(value))` (Int/UInt64) or an explicit
  `throw new OverflowException(SR.Overflow_Int32/UInt32)` (Int/UInt32).
- **`BitConverter` (041):** `ArgumentOutOfRangeException` (paramName `startIndex`,
  `SR.ArgumentOutOfRange_IndexMustBeLess`) when `(uint)startIndex >= (uint)length`
  — one unsigned compare catches negative *and* over-large; then
  `ArgumentException` (paramName `value`, `SR.Arg_ByteArrayTooSmallForValue` =
  "The array starting from the specified index is not long enough to read a value
  of the specified type.") when `startIndex > length - sizeof(T)`. `ToBoolean`
  throws only the `ArgumentOutOfRangeException` pair.
- **Span/views (043):** `if (length < 0) ThrowHelper.ThrowArgumentOutOfRangeException()`
  (parameterless in .NET) at construction; `ArraySegment` throws
  `ArgumentOutOfRangeException(nameof(count)/nameof(offset), NeedNonNegNum)` or
  `ArgumentException(Argument_InvalidOffLen)`. .NET `Span._length` is signed `int`.
  `HashCode.AddBytes(ReadOnlySpan<byte>)` delegates safety to the span (cannot
  receive a negative length).
- **`MemoryExtensions::CopyTo` (047):** `ArgumentException`,
  `SR.Argument_DestinationTooShort` = "Destination is too short.", paramName
  `destination`, **thrown before any element is written** (no partial copy). The
  port's member `Span::CopyTo` already throws this exact string.

---

## 10. Compatibility matrix

| Finding | Behaviour change | Who breaks | Class |
|---|---|---|---|
| SR-AUD-026 | invalid narrowing now throws (was silent wrap) | only callers that fed out-of-range values and consumed the wrapped result — already wrong | C (compatible narrowing) |
| SR-AUD-027 | NaN now throws (was a spurious integer) | only callers feeding NaN — already wrong | C |
| SR-AUD-041 | OOB index now throws (was UB read) | only callers with an OOB index — already UB | C |
| SR-AUD-043 (Span/RO-Span/Memory/ArraySegment) | negative/oversized length now throws (was UB) | only callers constructing an invalid-length view — already UB | C |
| SR-AUD-043 (ReadOnlyMemory ctors, HashCode) | to throw, `noexcept`/`constexpr` must drop | a caller relying on `noexcept(ReadOnlyMemory-ctor)` / `constexpr` use | **needs approval** |
| SR-AUD-047 | short destination now throws (was UB write) | only callers overflowing the destination — already UB | C |

Every value/UB change is a compatible narrowing toward .NET parity — the same
category the standing CCF-004/CCF-003 class-C approvals covered — **except** the
`ReadOnlyMemory`/`HashCode` `noexcept` drop, which is a distinct API-contract
change requiring its own sign-off.

---

## 11. Dependency graph

```
SR-AUD-026 (Convert integral)      independent
SR-AUD-027 (Convert float NaN)     independent; SAME FILE as 026 → one ticket
SR-AUD-041 (BitConverter bounds)   independent
SR-AUD-047 (MemoryExtensions copy) independent
SR-AUD-043a (Span/RO-Span/Memory/  independent; CLOSES the reachable HashCode
            ArraySegment ctors)    exploit by itself
SR-AUD-043b (ReadOnlyMemory        depends on 043a landing first (043a proves the
            noexcept ctors +       exploit is already closed, making 043b pure
            HashCode::AddBytes)     defense-in-depth) — NEEDS APPROVAL
```

The only ordering constraint is **043a → 043b**: land the span-ctor validation
first so 043b is demonstrably defense-in-depth, then take the `noexcept` decision.
Everything else is independent.

---

## 12. Implementation vs. design-first split

| Slice | Class | Autonomous? |
|---|---|---|
| SR-AUD-026 + 027 (Convert) | compatible implementation | **yes** |
| SR-AUD-041 (BitConverter) | compatible implementation | **yes** |
| SR-AUD-047 (MemoryExtensions) | compatible implementation | **yes** |
| SR-AUD-043a (Span/RO-Span/Memory/ArraySegment ctors) | compatible implementation | **yes** |
| SR-AUD-043b (ReadOnlyMemory `noexcept`/`constexpr` ctors + `HashCode::AddBytes noexcept`) | **design-first / approval-blocked** | **no** — needs user sign-off to drop `noexcept` |

Four of five findings, and the reachable-exploit half of the fifth, are
implementation-ready with an in-ticket compatibility argument (the CCF-004/CCF-003
precedent). Only 043b crosses into approval territory.

---

## 13. Source / ABI / layout approval boundaries

- **No object layout changes anywhere.** Every `length_`/`count_` field stays
  `intcs`; every parameter list stays identical. The ABI-sensitive `Span`/`Memory`
  layout is untouched.
- **No vtable, no return-convention, no iterator-layout change.**
- **The one approval item is `noexcept`/`constexpr`** on:
  `ReadOnlyMemory(const T*,intcs) constexpr noexcept` (L49),
  `ReadOnlyMemory(vector&) noexcept` (L58),
  `ReadOnlyMemory(ArraySegment) noexcept` (L67), and
  `HashCode::AddBytes(const ReadOnlySpan<uint8_t>&) noexcept` (L92).
  A throw cannot be added under `noexcept` without `std::terminate`; dropping it
  is observable to a caller relying on the specifier (mangled name unchanged;
  parameter list unchanged). **Options for 043b:** (a) drop `noexcept`/`constexpr`
  and throw as .NET does — needs approval; (b) keep `noexcept` and clamp
  negative/oversized to empty — preserves the specifier but diverges from .NET's
  throw contract. Recommend approval for (a), since 043a already closes the
  exploit and (a) gives full .NET parity.

---

## 14. Permanent test matrix

Each fix adds the invalid-domain case the audit found missing; **no existing
assertion is weakened** (none pins the buggy behaviour).

| Ticket | Must add |
|---|---|
| Convert (026/027) | `ToChar(-1)`, `ToChar(256)`, `ToByte(256L)`, `ToByte(-1L)`, `ToUInt32(-1)`, `ToUInt32(oversized long)`, `ToUInt64(-1)` all throw `OverflowException`; `ToInt32/ToInt64/ToUInt32/ToUInt64(NaN)` and `(+Inf)` throw `OverflowException`; a valid in-range value per overload still returns its exact value |
| BitConverter (041) | per decoder: negative `startIndex` throws `ArgumentOutOfRangeException`; `startIndex == size` and `> size` throw `ArgumentOutOfRangeException`; `startIndex` valid but `+sizeof(T) > size` throws `ArgumentException`; the exact-fit index-0 case still returns the round-trip value; `ToBoolean` short case throws `ArgumentOutOfRangeException` (not `ArgumentException`) |
| MemoryExtensions (047) | 2→1 static `CopyTo` throws `ArgumentException` before any write; equal-length still copies; the delegating `Span` overload throws too |
| Span/views (043a) | `Span<int>(ptr,-1)`, `ReadOnlySpan<uint8_t>(ptr,-1)` throw `ArgumentOutOfRangeException`; an oversized-`size()` vector ctor for `Span`/`Memory`/`ArraySegment` throws; valid construction unchanged; a `HashCode::AddBytes` over a now-rejected negative span can no longer be constructed (test the ctor rejection) |
| ReadOnlyMemory/HashCode (043b, post-approval) | `ReadOnlyMemory<int>(ptr,-1)` throws; `HashCode::AddBytes` defense-in-depth path asserted |

---

## 15. Sanitizer matrix

| Ticket | Sanitizer | Why |
|---|---|---|
| SR-AUD-041 | **ASan** | reproduce the OOB read pre-fix (`ToInt32(vec,-1)` and short vec) in the real body, clean post-fix |
| SR-AUD-043a | **ASan** | reproduce the negative-length OOB (`ReadOnlySpan<uint8_t>(oneByte,-1)` → `HashCode::AddBytes`) pre-fix, clean post-fix |
| SR-AUD-047 | **ASan** | reproduce the 2→1 heap-buffer-overflow **write** pre-fix, clean post-fix |
| SR-AUD-026, 027 | none required | pure value/throw changes, no memory or arithmetic-UB component (NaN→int is implementation-defined, not UB) |
| SR-AUD-043b | ASan (regression) | confirm the defense-in-depth path is clean; primary exploit already closed by 043a |

Reuse `build-asan/` (already `-fsanitize=address,undefined`) or a one-TU probe
with `-fsanitize=address`; **never more than three jobs**. Verify the changed
header/source object is rebuilt before drawing a sanitizer conclusion.

---

## 16. Recommended ticket order

1. **CCF5-A — SR-AUD-047** (MemoryExtensions static `CopyTo`) — smallest, fully
   isolated, ASan-confirmed **write** overflow; ideal first.
2. **CCF5-B — SR-AUD-041** (BitConverter 14 vector decoders) — ASan-confirmed
   read; mechanical, one validation block per decoder + the `ToBoolean` quirk.
3. **CCF5-C — SR-AUD-043a** (Span/ReadOnlySpan/Memory/ArraySegment ctors) —
   ASan-confirmed read; closes the reachable `HashCode` exploit.
4. **CCF5-D — SR-AUD-026 + 027** (Convert integral + float NaN) — value-only, one
   file pair; no sanitizer.
5. **CCF5-E — SR-AUD-043b** (ReadOnlyMemory `noexcept` ctors + `HashCode::AddBytes`)
   — **blocked on user approval** to drop `noexcept`/`constexpr`; defense-in-depth
   after CCF5-C.

Memory-corruption first (A/B/C), then value (D), then the approval-gated tail (E).

---

## 17. Explicit exclusions

- **SR-AUD-044** — `MemoryExtensions::CopyTo`'s forward-only `std::copy` overlap
  hazard (`std::copy` vs `Memmove`). Separate finding; not folded into 047.
- **SR-AUD-045/046** — `Sort` NaN ordering and related `MemoryExtensions`
  semantics. Separate.
- **`BitConverter` raw-pointer overloads** — stay documented-precondition APIs
  (matching `ToString(const bytecs*,…)`); not given a length parameter (that would
  be a signature change).
- **Endianness / alignment** of `BitConverter` — host-endian by design; unchanged.
- **The Decimal slice of CCF-005** (SR-AUD-035 parser, 036 `MidpointRounding`, 038
  negative-zero) — Decimal-specific, medium severity, larger; a separate review.
  036 also touches `Math`/`MathF` and overlaps **CCF-008**.
- **SR-AUD-021/022/023/024** — CCF-003, already remediated.
- **Changing any `length_` field to unsigned** — explicitly rejected: it is the
  only layout-breaking option, is unnecessary (.NET keeps it signed), and would
  break the `getLengthProperty()→intcs` contract.

---

## 18. Family completion criteria

The CCF-005 memory-safety slice is complete when SR-AUD-026, 027, 041, 043, 047
are all `remediated`, each with: a measured reproduction (ASan for 041/043/047), a
fix matching the .NET reference, a permanent invalid-domain test, the applicable
sanitizer clean, and `scripts/local_ci_check.sh build` green with no test-count
regression and Doxygen inside 1,942. **SR-AUD-043 may be split** — 043a
(autonomous) closes the reachable exploit and can be marked substantially
remediated; 043b (the `noexcept` drop) stays open until the user approves the
exception-specification change, tracked as a separate blocked ticket. The Decimal
slice (035/036/038) is **not** part of this completion criterion.

---

## 19. Implementation status

Appended as tickets land.

### 19.1 CCF5-A / #1850 — SR-AUD-047 — MemoryExtensions CopyTo — **DONE (2026-07-30)**

The static `MemoryExtensions::CopyTo(ReadOnlySpan<T>, Span<T>)`
(`MemoryExtensions.hpp:425`) now throws
`System::ArgumentException("Destination is too short.")` when
`source.getLengthProperty() > destination.getLengthProperty()`, **before** the
`std::copy` — the message and throw-before-copy contract the member
`Span<T>::CopyTo` already uses and .NET's `Argument_DestinationTooShort`. The
`Span<T>` source overload delegates. ASan reproduced the OOB write in the real
helper (pre-fix `heap-buffer-overflow WRITE of size 8` at
`MemoryExtensions.hpp:427`, `build-probe/1850_copyto_prefix.log`; post-fix clean
throw, `build-probe/1850_copyto_postfix.log`). +3 tests
(`SharpRuntimeTests_Core_Base` 5087 → 5090). Not `noexcept`; no signature/layout
change. `SR-AUD-047 → remediated`. **SR-AUD-044 (overlap) stays open** — out of
scope. Remaining CCF-005 memory-safety queue: #1851 (041), #1852 (043a), #1853
(026/027), #1854 (043b, `needs_user`).

### 19.2 CCF5-B / #1851 — SR-AUD-041 — BitConverter vector decoders — **DONE (2026-07-30)**

All 14 typed vector decoders
(`To{Boolean,Char,Int16,UInt16,Int32,UInt32,Int64,UInt64,Single,Double,Half,BFloat16,Int128,UInt128}(const std::vector<bytecs>&, intcs)`)
now call a shared private `validateDecodeRange(v.size(), i, width)` **before**
forwarding to the length-less raw-pointer read. It reproduces .NET
BitConverter's `byte[]` decoders exactly: `(uint)startIndex >= (uint)size`
throws `ArgumentOutOfRangeException("startIndex")` (one unsigned compare catching
both a negative and an over-large index), then `startIndex > size - width` throws
`ArgumentException("The array starting from the specified index is not long
enough to read a value of the specified type.", "value")` (the
`Arg_ByteArrayTooSmallForValue` string). Per plan premise-corrections §4#2/§4#3:
the fix lives in the **vector** overloads (they hold `v.size()`); the raw-pointer
overloads stay documented-precondition; and `ToBoolean` (width 1) throws only
`ArgumentOutOfRangeException` — its `ArgumentException` branch is provably
unreachable once the index passes the first check. The `memcpy`-into-a-local read
is kept, so the alignment/aliasing safety noted in §8 is preserved.

ASan reproduced the OOB in the real (inline, header-only) decoders, one fault
shape per process (ASan aborts on first error): pre-fix `heap-buffer-overflow
READ of size 4` at `BitConverter.hpp:126 in ToInt32` for both `ToInt32(vec4,-1)`
(read before the buffer, `build-probe/1851_bitconverter_prefix_neg.log`) and
`ToInt32(vec3,0)` (read past the buffer,
`build-probe/1851_bitconverter_prefix_short.log`); post-fix both throw the
correct exception with ASan clean (`…_postfix_neg.log`, `…_postfix_short.log`).
+46 tests (`SharpRuntimeTests_Core_Base` 5090 → 5136): per decoder a negative
index, an index == size, an insufficient-width case, and an exact-fit index-0
round-trip, plus the `ToBoolean` width-1 quirk. Not `noexcept`; no
signature/layout change. `SR-AUD-041 → remediated`. Remaining CCF-005
memory-safety queue: #1852 (043a), #1853 (026/027), #1854 (043b, `needs_user`).

### 19.3 CCF5-C / #1852 — SR-AUD-043a — Span/view ctor length validation — **DONE (2026-07-30)**

`Span(T*, intcs)` and `ReadOnlySpan(const T*, intcs)` now throw
`ArgumentOutOfRangeException("length")` when `length < 0` (the ticket's chosen
paramName; .NET uses a parameterless throw but documents the `length`
parameter). The four vector ctors — `Span(vector&)`, `ReadOnlySpan(vector&)`,
`Memory(vector&)`, `ArraySegment(vector&)` — route `size()` through a new shared
`System::detail::checkedSpanLength(n, paramName)`
(`System/detail/SpanLength.hpp`), which throws `ArgumentOutOfRangeException` on a
`size()>INT32_MAX` source rather than silently narrowing to a negative length.
Per plan §4#4 the `length_`/`count_` fields stay signed `intcs` and every
signature/layout is unchanged (no ABI change); .NET agrees (`_length` is signed
`int`, validated at construction).

This **closes the reachable SR-AUD-043 exploit**: a negative-length
`ReadOnlySpan<uint8_t>` can no longer be constructed, so it can never reach
`HashCode::AddBytes` (§4#5, §11's 043a→043b ordering). ASan reproduced the
pre-fix `heap-buffer-overflow READ of size 1` at `HashCode.hpp:76 in AddBytes`
(negative-length span, `build-probe/1852_span_hashcode_prefix.log`) and confirmed
a clean construction-time throw post-fix
(`build-probe/1852_span_hashcode_postfix.log`). +12 tests
(`SharpRuntimeTests_Core_Base` 5136 → 5148): negative-length ptr ctors throw
(`Span`/`ReadOnlySpan`, incl. `INTCS_MIN`), zero-length still constructs, the
`checkedSpanLength` guard tested directly at `INT32_MAX`, `INT32_MAX+1` and
`SIZE_MAX` (no huge allocation), the byte-span exploit-closure asserted in both
`ReadOnlySpanTests` and `HashCodeTests`, and empty-vector ctors still work. Not
`noexcept`; no signature/layout change. `SR-AUD-043a → remediated`; SR-AUD-043
stays **confirmed** overall until 043b lands.

**043b (#1854) stays blocked on approval.** The `ReadOnlyMemory`
`noexcept`/`constexpr` ctors (ReadOnlyMemory.hpp:49/58/67) and
`HashCode::AddBytes(const ReadOnlySpan<uint8_t>&) noexcept` (HashCode.hpp:92)
cannot throw on a bad length without dropping `noexcept`/`constexpr` — an
exception-spec change (§13). Now that 043a closes the reachable path, 043b is
pure defense-in-depth; it remains `needs_user` pending the drop-`noexcept`
(option a) vs clamp-to-empty (option b) decision. Remaining CCF-005 memory-safety
queue: #1853 (026/027), #1854 (043b, `needs_user`).
