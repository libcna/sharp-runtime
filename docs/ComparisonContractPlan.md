<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# CCF-010 — the default comparison contract family

Durable plan for **CCF-010**, written by design-only ticket **#1904** on
2026-07-31 before any production file was changed. Owning finding:
**SR-AUD-046** (`medium`, `confirmed`), the only member.

Everything numeric in this document was **measured against the shipped
headers** by `build-probe/1904_ccf010_probe.cpp`, not taken from the audit
record. Where the record and the measurement disagree, section 9 says so
explicitly and the measurement wins.

---

## 0. Candidate-family review — why CCF-010 is next

The previous batch closed **CCF-009** outright and left no ready work in it.
This section is the durable record of how the next family was chosen, per the
batch instruction. Statuses were re-derived from
`audit/AUDIT_FINDINGS_INDEX.md` (364 rows: 58 `remediated`, 303 `confirmed`, 1
`confirmed` split row, 2 `confirmed (design-complete)`) and from
`plan.sqlite3`, not from `audit/AUDIT_CROSS_CUTTING_FINDINGS.md`'s prose.

| CCF | Members | Member status today | Family state | Ready compatible work? |
|---|---|---|---|---|
| CCF-001 | SR-AUD-001 | confirmed | unplanned | yes — one tracked-CI matrix row |
| CCF-002 | 006, 007, 009, 061 | 006 remediated, 007a remediated / 007b open, 009 + 061 confirmed | partially remediated | **no** — remainder is #1879 `needs_user` + inactive #1880 |
| CCF-003 | 019–024 | **all six remediated** | **complete; no closure note in the CCF file** | none left |
| CCF-004 | 008, 019, 025, 049, 057, 060, 062, 084 | **all eight remediated** | complete (#1830–#1837) | none left |
| CCF-005 | 021–027, 035, 036, 038, 041, 043, 047 | all remediated except 035 and 043b | partially remediated | **no** — remainder is #1858 + #1854, both `needs_user` |
| CCF-006 | SR-AUD-021 | **remediated** (both slices, #1847 + #1849) | **complete; no closure note in the CCF file** | none left |
| CCF-007 | 029–033 | 030/031/032 remediated; 029 + 033 confirmed | partially remediated | **no** — remainder is #1862, #1863, #1865, all `needs_user` |
| CCF-008 | SR-AUD-036 | remediated | closed | none |
| CCF-009 | SR-AUD-010 | remediated | closed (previous batch) | none |
| **CCF-010** | **SR-AUD-046** | **confirmed** | **unplanned** | **yes — all of it** |
| CCF-011 | 052, 058, 065, 099, 121, 134 | all remediated | closed | none |
| CCF-012 | SR-AUD-015 | confirmed | partially remediated | **no** — remainder is #1884 `needs_user` |
| CCF-013 | SR-AUD-078 | remediated | closed | none |
| CCF-014 | 075, 085 | both remediated | closed | none |
| CCF-015 | SR-AUD-048 | confirmed | unplanned | yes — but see below |
| CCF-016 | 093–096, 100 | all remediated | closed | none |
| CCF-017 | SR-AUD-114 | confirmed | deferred with reason | **no** — needs runtime reflection |
| CCF-018 | 356, 364 | remediated | closed | none |
| CCF-019 | 327, 333, 357 | 357 remediated; 327 + 333 `confirmed (design-complete)` | compatible-remediation-complete | **no** — #1888/#1889/#1896 declined, #1894/#1897/#1899 blocked |
| CCF-020 | SR-AUD-358 | remediated | closed | none |

Only **three** families have unplanned, compatible, unblocked work: CCF-001,
CCF-010 and CCF-015.

**CCF-010 is selected.** Against the batch instruction's ordering:

1. *Memory safety / undefined behaviour / silent corruption first.* CCF-010 is
   the only one of the three that contains undefined behaviour. Handing
   `std::sort` a range that contains NaN violates `[alg.sort]`'s
   strict-weak-ordering precondition; section 3 measures it corrupting the
   **finite** elements of the array in **64 of 196** shapes, worst case
   **3,874 inversions** — silent data corruption through
   `System::Array::Sort`, the port's most ordinary sorting entry point, from
   nothing more exotic than a float array containing NaN. CCF-001 is a CI
   coverage gap and CCF-015 is a wrong-answer parity gap; neither has UB.
2. *Compatible ready work.* All of CCF-010 is compatible: six header-only
   files, no signature, `noexcept`, layout, vtable or calling-convention
   change (§13).
3. *Dependencies satisfied.* Every affected file is in `Core.Base`, which
   depends on nothing else in the graph. No new component edge (§13.5).
4. *Independently validatable.* Deterministic, single-threaded, exactly
   reproducible; the whole family is exercised by one probe.
5. *One structural repair closing several related surfaces.* One comparison
   policy in `System/detail/`, six consumers. The finding's own text demands
   exactly this: "The repair must centralize or consistently reuse the local
   comparison policy; changing only one surface leaves the others divergent."
6. *No ABI or layout break.* §13.

**Why the two alternatives are deferred, not rejected.**

- **CCF-001** (`SR-AUD-001`, the tracked CI matrix missing the
  `Collections.Blocking` selective consumer) is genuinely ready and genuinely
  small, but it is a *tooling coverage* finding with no defect in shipped
  behaviour: nothing a consumer runs is wrong. It also touches
  `.github/workflows/components.yml`, whose runtime cost lands on a CI account
  rather than on this repository, and CI-matrix growth is the kind of change
  that is better decided together with the selective-components policy than
  bundled into an unrelated batch. Deferred with nothing lost.
- **CCF-015** (`SR-AUD-048`, `std::isspace` on UTF-8 bytes in
  `MemoryExtensions::Trim*` and `ArgumentException::ThrowIfNullOrWhiteSpace`)
  is two sites and a real parity defect, but its repair requires choosing and
  implementing a **UTF-8 decode plus Unicode whitespace policy** — a new
  classification table with its own correctness surface, which the port does
  not have today. That is a larger design commitment for two entry points than
  CCF-010's is for seventy-plus, and it has no UB. Deferred; nothing in
  CCF-010 changes its premises.

**Two documentation defects found while doing this review, recorded here and
not silently fixed:** `audit/AUDIT_CROSS_CUTTING_FINDINGS.md` carries **no
closure note for CCF-003 or CCF-006** even though every member finding of both
is `remediated` in the findings index. A reader choosing the next family from
that file alone would believe both are still open. This is a record defect, not
a code defect; it is ticket **#1911** and carries no `SR-AUD-*` identifier.

---

## 1. Scope

**In scope.** The *default* comparison and equality path of six header-only
`Core.Base` files, wherever a raw C++ `<`, `>` or `==` on a **value of the
element/component/key type** stands where .NET uses `Comparer<T>.Default` or
`EqualityComparer<T>.Default`:

| File | Surfaces |
|---|---|
| `modules/core/include/System/Array.hpp` | `Sort` (2 default overloads), `BinarySearch` (2 default), `IndexOf` (3), `LastIndexOf` (3) |
| `modules/core/include/System/MemoryExtensions.hpp` | `Sort` (1 default), `BinarySearch` (2), `SequenceCompareTo` (2) |
| `modules/core/include/System/Nullable.hpp` | `NullableHelper::Compare`, `NullableHelper::Equals`, `Nullable<T>::Equals`, `Nullable<T>::GetHashCode` |
| `modules/core/include/System/ValueTuple.hpp` | `ValueTuple1..8`: `Equals`, `CompareTo`, `GetHashCode` (and the `==`/`!=`/`<`/`<=`/`>`/`>=` operators that route through them) |
| `modules/core/include/System/Tuple.hpp` | `detail::tupleCompare`, `Tuple1..8` `operator==`/`operator!=`/`GetHashCode` |
| `modules/core/include/System/Linq.hpp` | `Contains`, `Distinct`, `Min`, `Max`, `OrderBy`, `OrderByDescending` |

**Explicitly out of scope** — see §19 for the full list with reasons.

---

## 2. Public entry-point inventory

Counted from the shipped headers on 2026-07-31.

### 2.1 `Array.hpp` — 10 default-path entries

| Line | Entry | Raw operation |
|---|---|---|
| 43 | `Sort(vector<T>&)` | `std::sort` with `operator<` |
| 67 | `Sort(vector<T>&, index, length)` | `std::sort` with `operator<` |
| 160 | `IndexOf(vector<T>&, value)` | `array[i] == value` |
| 172 | `IndexOf(vector<T>&, value, startIndex)` | `== value` |
| 187 | `IndexOf(vector<T>&, value, startIndex, count)` | `== value` |
| 235–236 | `BinarySearch(vector<T>&, value)` | `== value`, `< value` |
| 257–258 | `BinarySearch(vector<T>&, index, length, value)` | `== value`, `< value` |
| 534 | `LastIndexOf(vector<T>&, value)` | `== value` |
| 549 | `LastIndexOf(vector<T>&, value, startIndex)` | `== value` |
| 564 | `LastIndexOf(vector<T>&, value, startIndex, count)` | `== value` |

`Sort(…, comparison)` at 54 and 83 and `BinarySearch(…, comparison)` at 277 and
305 are the caller-supplied-comparer overloads; they are **excluded** (§19.1).

### 2.2 `MemoryExtensions.hpp` — 5 default-path entries

| Line | Entry | Raw operation |
|---|---|---|
| 356 | `SequenceCompareTo(ReadOnlySpan<T>, ReadOnlySpan<T>)` | `a[i] < b[i]`, `a[i] > b[i]` |
| (fwd) | `SequenceCompareTo(Span<T>, Span<T>)` | delegates |
| 455 | `Sort(Span<T>)` | `std::sort` with `operator<` |
| 481 | `BinarySearch(ReadOnlySpan<T>, const T&)` | `== value`, `< value` |
| 494 | `BinarySearch(Span<T>, const T&)` | delegates |

`Sort(Span<T>, TComparer)` at 465 is excluded (§19.1). `Contains`,
`IndexOf`, `LastIndexOf`, `Count`, `Replace`, `SequenceEqual`, `StartsWith`,
`EndsWith` and the `*Any` family already route through
`Detail::MemoryExtensionsElementEquals`, which implements the correct
`float.Equals` rule — see §9.1.

### 2.3 `Nullable.hpp` — 4 entries

`NullableHelper::Compare` (210–211, raw `<` both directions),
`NullableHelper::Equals` (227, delegates to `operator==`),
`Nullable<T>::Equals` (119, `std::optional::operator==`),
`Nullable<T>::GetHashCode` (131, `std::hash<T>`, must stay consistent with the
repaired `Equals`).

`Nullable<T>::operator==`/`operator!=` (172–178) are **deliberately unchanged**
— see §6.3.

### 2.4 `ValueTuple.hpp` — 24 changed bodies, 72 reachable entries

Eight arities. Per arity: one `Equals`, one `CompareTo`, one `GetHashCode`
change; the two equality operators and the four relational operators route
through them. `ValueTuple8` additionally forwards to `Rest`, and the empty
`ValueTuple` is a constant-true control.

### 2.5 `Tuple.hpp` — 17 changed bodies

One `detail::tupleCompare` helper (68–69) reached by **all eight** `CompareTo`
bodies — the single most leveraged site in the family — plus eight
`operator==` bodies and eight `GetHashCode` bodies.

### 2.6 `Linq.hpp` — 6 entries

`Min` (254, `std::min_element`), `Max` (262, `std::max_element`), `OrderBy`
(287, `keySelector(a) < keySelector(b)`), `OrderByDescending` (307, `>`),
`Distinct` (nested loop, `existing == item`), `Contains` (366, `item == value`).

**Family total: 66 changed comparison sites, ~100 reachable public entries.**

---

## 3. Current implementation behaviour — measured

`build-probe/1904_ccf010_probe.cpp`, 36 cases, one forked process per case
under a 120 s watchdog, every operand produced at run time through `volatile`
so no comparison can be constant-folded away. Built four ways from one source:
plain, `-fsanitize=address`, `-fsanitize=undefined -fno-sanitize-recover`, and
`-D_GLIBCXX_ASSERTIONS -D_GLIBCXX_DEBUG`. Logs:
`build-probe/1904_prefix_{plain,asan,ubsan,assertions}.log`.

Every one of the six headers is header-only, so the code under test is compiled
**into the probe translation unit** and is fully instrumented; the prebuilt
`build/libsharp_runtime_core.a` supplies only exception constructors, none of
which is code under test.

### 3.1 Wrong-answer matrix (28 defect cases, 6 controls)

| Case | Surface | Measured | .NET |
|---|---|---|---|
| A1 | `Array::Sort({3,NaN,1})` | `[1,3,NaN]` | `[NaN,1,3]` |
| A2 | `Array::BinarySearch({NaN,1,3}, NaN)` | `-1` | `0` |
| A3 | `Array::BinarySearch({NaN,1,3}, 3)` | `2` | `2` ✔ |
| A4 | `Array::IndexOf({1,NaN,3}, NaN)` | `-1` | `1` |
| A5 | `Array::Sort`, 4096 elems, 1/3 NaN | **191 inversions among finite elements** | 0 |
| A6 | `Array::Sort`, 100 000 elems, 1/2 NaN | **934 inversions** | 0 |
| A7 | `Array::Sort`, 4096 elems, **one** NaN | 0 inversions | 0 ✔ |
| A8 | `Array::Sort` 196-shape sweep | **64 shapes corrupted, worst 3,874 inversions** | 0 |
| M1 | `MemoryExtensions::Sort({3,NaN,1})` | `[1,3,NaN]` | `[NaN,1,3]` |
| M2 | `MemoryExtensions::BinarySearch(…, NaN)` | `-1` | `0` |
| M3 | `SequenceCompareTo({NaN},{1})` | `0` | `-1` |
| M4 | `SequenceCompareTo({NaN},{NaN})` | `0` | `0` ✔ |
| M5 | `SequenceEqual({NaN},{NaN})` | `true` | `true` ✔ |
| M6 | `MemoryExtensions::Sort`, 4096 elems | **191 inversions** | 0 |
| N1 | `NullableHelper::Compare(NaN, 1)` | `0` | `-1` |
| N2 | `NullableHelper::Compare(NaN, NaN)` | `0` | `0` ✔ |
| N3 | `NullableHelper::Equals(NaN, NaN)` | `false` | `true` |
| N4 | `Nullable<double>::Equals(NaN)` | `false` | `true` |
| N5 | `Nullable<double>::operator==` NaN | `false` | `false` ✔ |
| N6 | `NullableHelper::Compare(null, NaN)` | `-1` | `-1` ✔ |
| V1 | `ValueTuple1<float>::CompareTo` NaN vs 1 | `0` | `-1` |
| V2 | `ValueTuple1<float>::operator==` NaN | `false` | `true` |
| V3 | `ValueTuple2<int,float>::CompareTo` | `0` | `-1` |
| T1 | `Tuple1<float>::CompareTo` NaN vs 1 | `0` | `-1` |
| T2 | `Tuple1<float>::operator==` NaN | `false` | `true` |
| T3 | `Tuple2<int,float>::CompareTo` | `0` | `-1` |
| L1 | `Linq::Contains({NaN}, NaN)` | `false` | `true` |
| L2 | `Linq::Distinct({NaN,NaN,NaN})` | `3` | `1` |
| L3 | `Linq::Min({1,2,NaN})` | `1` | `NaN` |
| L4 | `Linq::Max({NaN,1,2})` | `NaN` | `2` |
| L5 | `Linq::OrderBy({3,NaN,1})` | `[3,NaN,1]` | `[NaN,1,3]` |
| L6 | `Linq::OrderBy`, 4096 elems | **389 inversions** | 0 |
| C1 | `Array::Sort<int>({3,1,2})` | `[1,2,3]` | `[1,2,3]` ✔ |
| C2 | `ValueTuple1<string>::CompareTo` | `-1` | `-1` ✔ |
| C3 | `ValueTuple1<double>` signed zero | `eq=true cmp=0` | `eq=true cmp=0` ✔ |
| C4 | `Array::Sort<double>({3,NaN,1})` | `[1,3,NaN]` | `[NaN,1,3]` |

**28 defects, 8 already-correct rows (A3, A7, M4, M5, N2, N5, N6, C1–C3).**

### 3.2 The sanitizer result that matters

| Build | Diagnostics across all 36 cases |
|---|---|
| plain | 0 |
| `-fsanitize=address` | **0** |
| `-fsanitize=undefined -fno-sanitize-recover` | **0** |
| `-D_GLIBCXX_ASSERTIONS -D_GLIBCXX_DEBUG` | **0** |

**No sanitizer and no libstdc++ debug mode sees any of this**, including the
data corruption of A5/A6/A8. This is the single most important operational fact
in the family and it drives §16: the only thing that can catch a wrong repair
here is a test that asserts the *value*, and the only thing that catches the
corruption is comparing the sorted output against a known ordering.

libstdc++'s debug mode is blind for a specific, checkable reason:
`__glibcxx_requires_irreflexive` verifies `!comp(x, x)`, and `NaN < NaN` is
`false`, so irreflexivity **holds**. What NaN breaks is the *transitivity of
equivalence* — NaN is "equivalent" to every finite value while finite values
order among themselves — and libstdc++ checks no such thing.

---

## 4. Reference behaviour — current .NET

Read from `/rv/tmp/runtime/src/libraries` on 2026-07-31.

### 4.1 The two default policies

`Comparer<T>.Default` → `GenericComparer<T>.Compare` → `x.CompareTo(y)`
(`Collections/Generic/Comparer.cs:55-67`). For `float`,
`Single.cs:274-296`:

```csharp
if (m_value < value) return -1;
if (m_value > value) return 1;
if (m_value == value) return 0;
// At least one is NaN
if (IsNaN(m_value)) return IsNaN(value) ? 0 : -1;
return 1;
```

`EqualityComparer<T>.Default` → `GenericEqualityComparer<T>.Equals` →
`x.Equals(y)` (`Collections/Generic/EqualityComparer.cs:140-152`). For `float`,
`Single.cs:329-336`:

```csharp
if (obj == m_value) return true;
return IsNaN(obj) && IsNaN(m_value);
```

So the reference contract is: **NaN is smaller than every value including
negative infinity, and NaN equals NaN**, for ordering *and* equality — but
`operator==` on the raw primitive is untouched IEEE.

### 4.2 `Array.Sort` / `Span.Sort` — the NaN pre-pass

`Collections/Generic/ArraySortHelper.cs:285-305` is decisive:

```csharp
if (comparer == null || comparer == Comparer<T>.Default) {
    if (keys.Length > 1) {
        // For floating-point, do a pre-pass to move all NaNs to the beginning
        // so that we can do an optimized comparison as part of the actual sort
        // on the remainder of the values.
        if (typeof(T) == typeof(double) || typeof(T) == typeof(float) || typeof(T) == typeof(Half)) {
            int nanLeft = SortUtils.MoveNansToFront(keys, default(Span<byte>));
            if (nanLeft == keys.Length) return;
            keys = keys.Slice(nanLeft);
        }
        IntroSort(keys, 2 * (BitOperations.Log2((uint)keys.Length) + 1));
    }
}
```

`SortUtils.MoveNansToFront` (`:1111-1139`) is a single forward pass that swaps
each NaN to the next front slot and returns the count.

The reference therefore does not merely produce a different order — it removes
NaN from the comparator's input entirely, for exactly the reason this plan is
about. The port must do the same, not because the order differs but because
otherwise the comparator is not an ordering.

### 4.3 The remaining reference sites

| Port surface | Reference | Rule |
|---|---|---|
| `MemoryExtensions::SequenceCompareTo` | `MemoryExtensions.cs:3533-3573` | per-element `Comparer<T>.Default.Compare`, tail `span.Length.CompareTo(other.Length)` |
| `Array::IndexOf`/`LastIndexOf` | `Array.cs` → `EqualityComparer<T>.Default` | `float.Equals` |
| `Array::BinarySearch` | `Array.cs` → `Comparer<T>.Default` | `float.CompareTo` |
| `Nullable.Compare<T>` | `Nullable.cs:77-85` | `Comparer<T>.Default.Compare` |
| `Nullable.Equals<T>` | `Nullable.cs:87-95` | `EqualityComparer<T>.Default.Equals` |
| `Nullable<T>.Equals(object)` | `Nullable.cs:59-64` | `value.Equals(other)` → `float.Equals` |
| `Nullable<T>.GetHashCode` | `Nullable.cs:66` | `value.GetHashCode()` → NaN-normalising `Single.GetHashCode` |
| `ValueTuple<T1>.Equals` | `ValueTuple.cs:308-311` | `EqualityComparer<T1>.Default.Equals` |
| `ValueTuple<T1>.CompareTo` | `ValueTuple.cs:317-329` | `Comparer<T1>.Default.Compare` |
| `Tuple<T1>` equality/compare | `Tuple.cs` `IStructuralEquatable`/`IStructuralComparable` | `EqualityComparer<object>.Default` / `Comparer<object>.Default` |
| `Enumerable.Min(float)` | `Linq/Min.cs:78-140` | first NaN encountered wins; NaN short-circuits |
| `Enumerable.Max(float)` | `Linq/Max.cs:96-130` | **skip leading NaNs**, then plain `>`; all-NaN → last element |
| `Enumerable.Contains`/`Distinct` | `EqualityComparer<T>.Default` | `float.Equals` |
| `Enumerable.OrderBy` | `OrderedEnumerable.cs` | `Comparer<TKey>.Default` |

`Min` and `Max` are **not symmetric** in the reference and the port must not
make them so: `Min({1,2,NaN})` is `NaN`, `Max({NaN,1,2})` is `2`. Both come
from the same rule (NaN is the smallest value) applied to opposite ends.
The reference explains itself in a comment at `Min.cs:130-139`.

---

## 5. Reproduction matrix

`build-probe/1904_ccf010_probe.cpp --list` prints the 36 cases; `--case <id>`
runs one. `build-probe/1904_run.sh <plain|asan|ubsan|assertions> <log>` builds
and drives the whole matrix. Compilation is one translation unit, strictly
serial: **aggregate parallelism 1 job**.

Reproduce the whole family:

```bash
cd /rv/data/development/github.com/openeggbert/sharp-runtimervc
TMPDIR=$PWD/build-tmp bash build-probe/1904_run.sh plain build-probe/1904_prefix_plain.log
TMPDIR=$PWD/build-tmp bash build-probe/1904_run.sh asan  build-probe/1904_prefix_asan.log
```

The A8 sweep is deterministic (fixed-seed xorshift), so `corrupted=64
worst-inversions=3874` is exactly reproducible.

---

## 6. Root causes

### 6.1 Shared root cause

Every one of the 66 sites ports a .NET expression whose operand type carries a
**type-specific comparison contract** (`IComparable<T>.CompareTo` /
`IEquatable<T>.Equals`, reached through `Comparer<T>.Default` /
`EqualityComparer<T>.Default`) into a C++ expression that uses the **built-in
operator** on the same operand. For every type in the port except the two IEEE
binary floating types the two agree exactly, which is why every non-floating
test passes and why the defect was invisible for the whole life of these files.
For `float` and `double` they differ in exactly two places — NaN's order, and
NaN's equality with itself — and those two differences are the whole family.

### 6.2 The distinct second cause, present only in the sort surfaces

`Array::Sort`, `MemoryExtensions::Sort` and `Linq::OrderBy`/`OrderByDescending`
do not merely *report* a wrong comparison; they **hand the wrong comparison to
`std::sort` / `std::stable_sort` as a predicate**, and `<` over a NaN-bearing
range is not a strict weak ordering: NaN is incomparable with (and therefore
"equivalent to") every finite value, while finite values order among
themselves, so equivalence is not transitive. `[alg.sort]` makes that a
precondition violation — undefined behaviour, not a wrong answer — and §3.1
measures the observable consequence: **the finite elements come out
unsorted**, in 64 of 196 shapes.

This is a genuinely different defect from the first with a genuinely different
repair, and a plan that treated the family as one substitution would fix the
reported symptom and leave the UB. A single NaN in 4,096 elements (case A7)
does **not** corrupt, which is precisely why a hand-picked three-element
example cannot find this.

### 6.3 The third cause: one surface where the raw operator is *correct*

C#'s **lifted** `==` on `T?` is not `EqualityComparer<T>.Default`; it is the
underlying type's `operator ==`, so `(double?)double.NaN == (double?)double.NaN`
is `false` in C#. The port's `Nullable<T>::operator==` already reproduces this
(case N5), and "fix all the raw `==`s" would break it. `Nullable<T>::Equals`
and `NullableHelper::Equals` are the reflexive ones. The same split exists
between `MemoryExtensions::SequenceEqual` (already correct, §9.1) and
`SequenceCompareTo` (wrong).

---

## 7. Selected repair

### 7.1 The policy — one new private header

`modules/core/include/System/detail/ComparisonPolicy.hpp`, namespace
`System::detail`, alongside the existing `IntegerNumberStylesParser.hpp` and
`SpanLength.hpp` precedents. Header-only, no `.cpp`, no new component edge,
no public type added to any published namespace.

```cpp
// Counterpart of Comparer<T>.Default.Compare. For IEEE binary floating types
// this is Single/Double.CompareTo: NaN orders before every value including
// negative infinity, and two NaNs compare equal. For every other T it is the
// built-in relational operators, unchanged.
template<typename T> [[nodiscard]] constexpr int compareValues(const T& a, const T& b);

// Counterpart of EqualityComparer<T>.Default.Equals. For IEEE binary floating
// types this is Single/Double.Equals: `a == b || (isnan(a) && isnan(b))`.
template<typename T> [[nodiscard]] constexpr bool equalValues(const T& a, const T& b);

// Counterpart of Single/Double.GetHashCode's NaN and signed-zero
// normalisation, so a NaN-reflexive Equals keeps the equal-implies-equal-hash
// invariant. For every other T it is std::hash<T>, unchanged.
template<typename T> [[nodiscard]] std::size_t hashValue(const T& v);

// A strict weak ordering built from compareValues, safe to hand to std::sort
// even when the range contains NaN. For non-floating T it is `a < b`.
template<typename T> struct DefaultLess { bool operator()(const T&, const T&) const; };

// Counterpart of SortUtils.MoveNansToFront. Moves every NaN in [first, last)
// to the front, preserving nothing else about their order, and returns how
// many were moved. A no-op returning 0 for non-floating value types.
template<typename It> [[nodiscard]] std::size_t moveNaNsToFront(It first, It last);
```

Each is `if constexpr (std::is_floating_point_v<T>)` at the top, so for every
non-floating instantiation the generated code is **the code that is there
today**, byte for byte (§16 pins this).

### 7.2 Per-surface application

| Surface | Repair |
|---|---|
| `Array::Sort` (2), `MemoryExtensions::Sort` (1) | `moveNaNsToFront` over the range, then `std::sort` over the remainder — .NET's own algorithm (§4.2). NaN never reaches the comparator, so the SWO precondition holds structurally rather than by inspection. |
| `Array::BinarySearch` (2), `MemoryExtensions::BinarySearch` (2) | one `compareValues(array[mid], value)` replacing the `==`/`<` pair |
| `Array::IndexOf` (3), `LastIndexOf` (3) | `equalValues` |
| `MemoryExtensions::SequenceCompareTo` (2) | `compareValues` per element |
| `NullableHelper::Compare` | `compareValues` |
| `NullableHelper::Equals`, `Nullable<T>::Equals` | `equalValues` on the contained value, `has_value` handled first |
| `Nullable<T>::GetHashCode` | `hashValue` |
| `Nullable<T>::operator==`/`!=` | **unchanged** (§6.3) |
| `Tuple::detail::tupleCompare` | body becomes `return compareValues(a, b);` — one edit, eight `CompareTo` bodies |
| `Tuple1..8::operator==`, `ValueTuple1..8::Equals`/`operator==` | `equalValues` per component |
| `ValueTuple1..8::CompareTo` | `compareValues` per component |
| `Tuple1..8::GetHashCode`, `ValueTuple1..8::GetHashCode` | `hashValue` per component |
| `Linq::Contains`, `Distinct` | `equalValues` |
| `Linq::Min`, `Max` | the reference's own NaN rules (§4.3), **not symmetric** |
| `Linq::OrderBy`, `OrderByDescending` | `DefaultLess<Key>` / its reverse, applied to the selected keys |

### 7.3 Rejected alternatives

| Alternative | Why rejected |
|---|---|
| Specialise `operator<`/`operator==` for `float`/`double` | impossible — they are built-in operators for built-in types |
| A `SharpFloat` wrapper type used inside the containers | changes every public signature; a layout and source break for a NaN-only defect |
| Only fix ordering, leave equality | leaves `Contains`/`Distinct`/`IndexOf`/`Equals` wrong and splits the policy in two, which is exactly what the finding warns against |
| Only fix the six surfaces' comparators, no NaN pre-pass in `Sort` | a correct total-order comparator *would* satisfy SWO, but the repair's correctness would then rest on the hand-written predicate rather than on NaN being absent; .NET chose the pre-pass and the pre-pass also keeps the inner comparison a raw `<` |
| Do the pre-pass everywhere instead of a comparator | impossible for `OrderBy`, whose NaNs are in the *keys*, not the elements; and for `BinarySearch`, which does not own its range |
| Throw on NaN input | not the reference's behaviour anywhere |

---

## 8. Dependency graph

```
#1904 (this plan, design-only)
   |
   +-> #1905  detail/ComparisonPolicy.hpp  +  Array.hpp        (blocks all below)
         |
         +-> #1906  MemoryExtensions.hpp
         +-> #1907  Nullable.hpp
         +-> #1908  Tuple.hpp + ValueTuple.hpp
         +-> #1909  Linq.hpp
               |
               +-> #1910  CCF-010 reconciliation and closure
```

#1906–#1909 are independent of each other and may land in any order once
#1905 has introduced the policy header. #1910 requires all four.

---

## 9. Premises of the record that are stale, understated or wrong

Each was measured before this plan was written, and the original audit text is
retained unedited per the repository's convention.

### 9.1 CCF-010's claim about `MemoryExtensions` equality is **stale**

The cross-cutting record says "`MemoryExtensions` and `Array` both choose raw
`<` and `==` for their default sort/search paths". For `MemoryExtensions` the
`==` half has **already been repaired**, before the audit and independently of
it: `MemoryExtensions.hpp:18-32` defines
`Detail::MemoryExtensionsElementEquals`, whose `if constexpr` floating branch is
`a == b || (std::isnan(a) && std::isnan(b))` — `float.Equals` exactly — and
`Contains`, `ContainsAny`, `IndexOf`, `LastIndexOf`, `Count`, `Replace`,
`SequenceEqual`, `StartsWith` and `EndsWith` all route through it. Cases M5 and
M4 confirm two of those are already correct. **The port already contains a
correct, tested statement of half this family's policy in one file; CCF-010's
real subject is that it was never shared.** That reframing is why §7.1 puts the
policy in `System/detail/` rather than growing the `MemoryExtensions`-local
helper.

### 9.2 The severity `medium` **understates a reachable undefined-behaviour and
silent-corruption path**

Neither the finding nor the cross-cutting record claims data corruption of
values that are *not* NaN. §3.1 measures it: 64 of 196 sorted shapes come out
with their finite elements unordered, worst case 3,874 inversions in 65,536
elements, from `System::Array::Sort(std::vector<float>&)` — a public entry with
no unusual argument. The audit's own three-element example
(`{3,NaN,1}` → `1,3,NaN`) cannot exhibit it, and neither can a single NaN in
4,096 elements (case A7). The `MemoryExtensions` report *does* name the
strict-weak-ordering problem in one sentence and the `Array` report notes
"`std::sort` is not justified"; neither quantifies it and neither treats it as
a defect class distinct from the NaN ordering. This plan does (§6.2). **No new
`SR-AUD-*` identifier is issued** — this was found during remediation of an
existing finding, in the files that finding owns — and numbering stays frozen
at **364**.

### 9.3 `Linq::Max` is a defect the record does not mention, in the **opposite
direction** from `Min`

The `Linq.hpp` report says "`Min` uses `std::min_element`; a later NaN leaves a
prior finite value … `Max` and OrderBy also rely on raw `<`/`>`". That reads as
one symmetric problem. Measured, they are opposite: `Min({1,2,NaN})` **loses**
the NaN and returns 1 where .NET returns NaN (case L3), while
`Max({NaN,1,2})` **keeps** the NaN and returns NaN where .NET returns 2 (case
L4). A repair that made them symmetric would fix one and break the other.
The reference is deliberately asymmetric (§4.3).

### 9.4 `Nullable`'s report is **wrong about `operator==`** in one respect

The `Nullable.hpp` report says "`Nullable<T>::Equals` / `NullableHelper::Equals`
use raw `==` through `std::optional`" and treats that as the defect. It is the
defect for those two, but the report's framing invites fixing
`Nullable<T>::operator==` at the same time, and that would be a **divergence**:
C#'s lifted `==` is the underlying `operator ==`, so `(double?)NaN == (double?)NaN`
is `false` in .NET too (case N5). The port is already right there and must stay
right. Recorded and pinned by a permanent test rather than left to a future
reader's judgement.

### 9.5 The `ValueTuple` report's hash/equality remark is **load-bearing, not a
nice-to-have**

It notes "No test verifies a hash/equality invariant for any tuple containing
NaN". Once `Equals` becomes NaN-reflexive, `GetHashCode` **must** normalise
NaN or the repair itself introduces an equal-objects-unequal-hashes defect that
does not exist today. libstdc++'s `std::hash<double>` already normalises ±0.0
to `0` but hashes NaN's payload bits, so two NaNs with different payloads hash
differently. .NET normalises both cases (`Single.GetHashCode` folds NaN and
zero to `bits & 0x7F800000`). `hashValue` is therefore part of the repair, not
an extra (§7.1).

### 9.6 One finding claim is **already satisfied** and must not be "fixed" again

`SequenceCompareTo({NaN},{NaN})` returns `0`, which is what .NET returns (case
M4). It is correct by accident — both `<` tests fail — but it is correct, and
the repair must keep it so.

---

## 10. Compatible implementation tickets

| Ticket | Scope | Size | Depends on |
|---|---|---|---|
| **#1905** | `System/detail/ComparisonPolicy.hpp` + all 10 `Array.hpp` default entries | M | #1904 |
| **#1906** | 5 `MemoryExtensions.hpp` entries | S | #1905 |
| **#1907** | 4 `Nullable.hpp` entries | S | #1905 |
| **#1908** | `Tuple.hpp` (17 bodies) + `ValueTuple.hpp` (24 bodies) | M | #1905 |
| **#1909** | 6 `Linq.hpp` entries | S | #1905 |
| **#1910** | SR-AUD-046 → `remediated`, CCF-010 closure note, audit + `NEXT.md` + `plan.md` reconciliation | S | #1905–#1909 |

Every one is compatible. **None requires user approval** — see §13.

---

## 11. Design-first or approval-gated tickets

**None.** This family has no approval-gated remainder. That is unusual for a
post-audit family here and is stated deliberately so a future reader does not
look for one: nothing in §7 changes a signature, a `noexcept` specification, an
object layout, a vtable, a calling convention or an iterator representation,
and nothing requires a downstream source migration.

The one *observable* change class — a call that today returns a wrong or
undefined result for NaN input now returns .NET's result — is the same class
the user has already accepted three times in this remediation programme
(CCF-013's in-place Base64 output for 28 of 50 lengths, CCF-016's eleven
HResult constants, CCF-011's previously-returning calls that now throw), and it
is required by `CLAUDE.md`'s porting checklist item 5, "logic parity with the
.NET reference". If a future reviewer disagrees, the smallest severable piece
is `Linq::Min`/`Max` (§9.3), which is the only place a *finite* return value
changes; everything else changes only NaN's placement or a NaN comparison.

---

## 12. Ticket execution order

1. **#1905** — the policy header and `Array`. First because it introduces the
   shared mechanism and because `Array::Sort` is where the UB lives.
2. **#1906** — `MemoryExtensions`. Second because it is the file that already
   holds the correct equality rule (§9.1), so it is where the "one policy, not
   two" claim is proved by deleting the local duplicate.
3. **#1908** — `Tuple` + `ValueTuple`. Largest mechanical surface; one helper
   (`tupleCompare`) covers eight of the seventeen `Tuple` bodies.
4. **#1907** — `Nullable`. Smallest, and carries §9.4's must-not-change row.
5. **#1909** — `Linq`. Last of the implementations because `Min`/`Max` are the
   only asymmetric rows and benefit from the policy being settled.
6. **#1910** — reconciliation and closure.

---

## 13. Source, ABI, vtable, layout and iterator consequences

### 13.1 Signatures — none change

Every changed body is a member or static of a class template or a free function
template. No parameter type, return type, default argument, template parameter
list or `noexcept` specification changes anywhere in the family.

### 13.2 Mangled names — none change

`Array`, `MemoryExtensions`, `Linq` are non-template `struct`s of function
templates; `Nullable`, `Tuple*`, `ValueTuple*` are class templates. Every
affected function is `inline` or a template specialisation with COMDAT linkage.
The set of external symbols in `libsharp_runtime_core.a` is expected to be
identical before and after; **#1905 will measure it with `nm --extern-only` and
record the diff** rather than assert it.

### 13.3 Layout and vtable — none change

No data member is added, removed, reordered or retyped in any of the six files;
none of the affected types has a virtual function. `sizeof`/`alignof` of
`Nullable<double>`, `Tuple1<float>`, `ValueTuple8<…>` and the rest will be
asserted unchanged at compile time and at run time.

### 13.4 Iterator representation — none change

No iterator type in the family is touched. `Array::Sort`'s NaN pre-pass runs
over `std::vector<T>::iterator`, which it does not publish.

### 13.5 Component graph — unchanged at 41 modules / 91 edges

`System/detail/ComparisonPolicy.hpp` lives in `Core.Base`'s existing include
root and is included only by files already in `Core.Base`. No
`PUBLIC_DEPENDENCIES`, `PRIVATE_DEPENDENCIES` or `TEST_DEPENDENCIES` entry
changes, and the generated catalogue does not move. **It is nevertheless a new
public header, so `scripts/check_selective_components.sh` runs in this batch**
(the batch policy's first trigger).

### 13.6 Recompilation

Every changed body is `inline` or a template. A consumer **must be fully
rebuilt**, and the linker cannot enforce it — the same standing hazard recorded
for #1791, #1867, #1868, #1870, #1802 and CCF-012. Stated at each changed
declaration rather than assumed.

### 13.7 Performance

`compareValues`/`equalValues`/`hashValue`/`DefaultLess` are `if constexpr`-gated
and generate *the current instructions* for every non-floating `T`. For
floating `T`: `equalValues` costs one extra predicted-not-taken branch on the
unequal path; `compareValues` costs at most two extra comparisons; `Sort` costs
one extra linear pass over the range (`moveNaNsToFront`), which is `O(n)`
against `std::sort`'s `O(n log n)`. #1905 will measure `Array::Sort` on
1,000,000 `float`s and 1,000,000 `int`s before and after and record the paired
medians rather than assert "negligible".

---

## 14. Test matrix

Permanent, add-only. No existing test may be weakened, deleted, skipped or
recategorised.

Per surface, for `float` **and** `double`:

1. NaN vs finite, both argument orders;
2. NaN vs NaN;
3. NaN vs `+Infinity` and vs `-Infinity` — NaN must order below `-Infinity`;
4. signed zero (`+0.0` vs `-0.0`): equal, compare 0, and — for `hashValue` —
   the same hash;
5. the empty range and the one-element range (which `std::sort` never compares,
   the shape that hid four CCF-011 defects);
6. all-NaN ranges, which `.NET`'s pre-pass returns early from;
7. a range of `> 16` elements, so introsort's insertion-sort phase runs, and one
   of `> 4096`, so its partition phase does;
8. the non-floating controls (`int`, `std::string`, a user type with only
   `operator<`/`operator==`) asserted **byte-identical** to today's behaviour;
9. a type that is neither floating nor arithmetic must still compile and behave
   as it does today — a compile-time control on `DefaultLess<std::string>`;
10. for `Tuple`/`ValueTuple`: a NaN in the first, a middle and the last
    component, and inside `Rest` for arity 8;
11. for `Nullable`: `null` vs `NaN`, `NaN` vs `null`, `null` vs `null`, and
    `operator==` pinned as **`false`** for NaN vs NaN (§9.4);
12. equal-implies-equal-hash asserted for every type whose `Equals` changed.

The 28 defect rows of §3.1 each become at least one permanent assertion, and
the 8 already-correct rows each become a **pinning** assertion so they cannot
regress in the other direction.

---

## 15. Sanitizer matrix

| Sanitizer | Applicability | Why |
|---|---|---|
| **ASan** | run, expected clean before and after | the family reorders elements inside caller-owned storage; the `moveNaNsToFront` pass is new index arithmetic over a range and must be proved in-bounds, including empty, one-element and all-NaN ranges |
| **UBSan** | run, expected clean before and after | new integer index arithmetic; also pins that no `isnan`/comparison path introduces a float-cast or shift defect |
| **LSan** | run with ASan | `Distinct`, `OrderBy` and `Sort` allocate; `Tuple`/`ValueTuple` hold owning components in the tests |
| **TSan** | **not applicable, recorded not skipped** | no shared mutable state, no atomic, no lock, no cache, no singleton and no thread is introduced or touched anywhere in the family; every function is pure over its arguments |

`build-probe/1904_run.sh` already builds ASan and UBSan flavours; §3.2 is the
"before" reading and each implementation ticket re-runs it.

**Sanitizer activation must be proved, because §3.2 shows every sanitizer is
silent on the defect itself.** Each ticket records a deliberate
out-of-bounds/UB self-test in the same binary so a clean log cannot mean "the
sanitizer was not enabled".

---

## 16. Mutation tests

§3.2 is the reason this section exists: **no sanitizer, and not libstdc++ debug
mode, can see any defect in this family**, so the only gate is the test suite,
and the suite must be shown to fail when the repair is wrong. Every
implementation ticket runs its mutations by editing the shipped header,
rebuilding, re-running, and restoring.

| # | Mutation | Must fail |
|---|---|---|
| M-1 | `compareValues` floating branch returns `a < b ? -1 : (b < a ? 1 : 0)` (drops the NaN tail) | the NaN-ordering tests |
| M-2 | `equalValues` floating branch returns `a == b` (drops NaN reflexivity) | `IndexOf`/`Contains`/`Distinct`/`Equals` NaN tests |
| M-3 | `moveNaNsToFront` returns 0 without moving, and `Sort` still uses `std::sort` over the whole range | the sorted-order tests **and** the ≥4096-element corruption test |
| M-4 | `Sort` uses `DefaultLess` instead of the pre-pass | must **pass** — this is the equivalent-correct alternative, and a mutation that must not fail is what distinguishes "the tests pin the order" from "the tests pin the implementation" |
| M-5 | `Nullable<T>::operator==` "fixed" to be NaN-reflexive | the §9.4 pinning test |
| M-6 | `hashValue` floating branch returns `std::hash<T>{}(v)` | the equal-implies-equal-hash test for a NaN component |
| M-7 | `Linq::Min` and `Max` made symmetric (both short-circuit on NaN) | the `Max({NaN,1,2}) == 2` test |
| M-8 | `compareValues` NaN tail inverted (NaN orders *after* everything) | the NaN-vs-`-Infinity` test |

M-4 is deliberately a *negative* mutation: a family whose suite fails on a
different-but-equally-correct implementation is over-fitted, and this plan says
so in advance.

---

## 17. Completion criteria

CCF-010 closes, and **SR-AUD-046 becomes `remediated`**, when all of:

1. #1905–#1910 are `done`;
2. all 28 defect rows of §3.1 return .NET's value, and all 8 already-correct
   rows are unchanged, re-measured with the **unmodified** probe;
3. the A8 sweep reads `corrupted=0`;
4. permanent tests exist for every §14 row and the whole repository gate shows
   no test-count regression against the 14,745 floor;
5. every §16 mutation produces its stated outcome;
6. ASan + UBSan + LSan are clean with activation proved;
7. `nm --extern-only` is identical before and after on
   `libsharp_runtime_core.a`, and `sizeof`/`alignof` are asserted unchanged;
8. `scripts/local_ci_check.sh build` passes, the module graph is still 41/91,
   the Doxygen count is at or below the 1,942 ceiling, and
   `scripts/check_selective_components.sh` passes;
9. `audit/AUDIT_FINDINGS_INDEX.md`, the six per-file reports,
   `audit/AUDIT_CROSS_CUTTING_FINDINGS.md`, `NEXT.md`, `plan.md` and
   `plan.sqlite3` all agree.

Audit numbering stays frozen at **364**. No new `SR-AUD-*` identifier is issued
by this family.

---

## 18. Explicit exclusions

1. **Caller-supplied comparers.** `Array::Sort(…, comparison)`,
   `Array::BinarySearch(…, comparison)` and
   `MemoryExtensions::Sort(span, comparer)` pass the caller's predicate
   straight through, exactly as .NET's `IntrospectiveSort` does with a
   non-default `IComparer<T>`. A caller who supplies an inconsistent comparer
   gets undefined behaviour in both runtimes; that is the caller's contract and
   is not repaired here. It **is** documented at each declaration.
2. **`SequenceCompareTo`'s length tail magnitude.** The port returns
   `a.Length - b.Length`; .NET's generic overload returns
   `span.Length.CompareTo(other.Length)`, i.e. −1/0/1. Same sign, different
   magnitude. Not a CCF-010 defect (no comparison contract involved), no
   `SR-AUD-*` identifier, recorded here so a future reader does not mistake it
   for one.
3. **`Linq::Distinct`'s O(n²) loop** and **`Linq::OrderBy` calling its key
   selector from inside the comparator** — both named in the `Linq.hpp` report,
   both real, both about cost rather than about the comparison contract.
   Untouched.
4. **`Half`.** .NET's pre-pass covers `Half` too; the port has no `Half` type.
5. **`Decimal`, `Int128`, `UInt128`, `DateTime`, `TimeSpan`** and every other
   type with a hand-written `CompareTo`. None of them has a NaN state, so none
   can exhibit this defect. This is an argument from the absence of a NaN
   value, not from a sweep, and is stated that way deliberately.
6. **`std::optional`'s ordering operators** on `Nullable<T>` — the port exposes
   no `operator<` on `Nullable<T>`, and .NET has no lifted `<` either that
   would differ from `Compare`.
7. **SR-AUD-044, SR-AUD-051, SR-AUD-053, SR-AUD-062, SR-AUD-063,
   SR-AUD-134/135** — other findings in the same reports, unrelated causes,
   untouched and not closed by this family.
8. **CCF-015** (`SR-AUD-048`, UTF-8 whitespace) — same file
   (`MemoryExtensions.hpp`), entirely different cause. Deliberately not
   absorbed; §0 records why.

9. **The `Collections` module.** A larger population with the identical cause
   exists outside SR-AUD-046's six files and is deliberately **not** absorbed —
   see §18a.

---

## 18a. A larger population, measured and deliberately not absorbed

Found while writing this plan, by sweeping `modules/collections/include` for
the same two shapes. This is a **newly discovered population found during
remediation**, not a CCF-010 finding and not an SR-AUD finding: **no
`SR-AUD-*` identifier is issued** (numbering stays frozen at 364), and it is
recorded as ticket **#1912** with the inventory below.

### 18a.1 Default-ordering sites — 5, four of them the same undefined behaviour

| Site | Entry |
|---|---|
| `Generic/List.hpp:420` | `List<T>::Sort()` |
| `Generic/List.hpp:561` | `List<T>::BinarySearch(item)` (`std::lower_bound`) |
| `Immutable/ImmutableList.hpp:606` | `ImmutableList<T>::Sort()` |
| `Immutable/ImmutableList.hpp:623` | `ImmutableList<T>::Sort(index, count)` |
| `Immutable/ImmutableArray.hpp:283` | `ImmutableArray<T>::Sort()` |

The four `std::sort` sites carry **exactly** §6.2's precondition violation:
`List<float>` holding a NaN is an entirely ordinary thing for the ported game
code this library exists for, and `List<T>::Sort()` is a more likely call than
`Array::Sort`. `ArrayList::Sort` (`ArrayList.hpp:552`, `:568`) and the
`Sort(comparison)` overloads take a caller-supplied comparer and are excluded
on §18.1's rule.

### 18a.2 Ordered associative containers — a third, distinct shape

`SortedSet<T>` is `std::set<T>`, and `SortedDictionary<K,V>` and
`SortedList<K,V>` are `std::map<K,V>`; all three use `std::less` on the element
or key. A `float`/`double` element or key that is NaN therefore violates
`[associative.reqmts]`'s ordering requirement for the *lifetime of the
container*, not only during one algorithm call. This shape has no counterpart
in the six CCF-010 files and needs its own analysis; it is **not** simply "the
same fix again".

### 18a.3 Default-equality sites — 56 across 20 headers

`std::find`, `== item`, `== value`, `.second == value` and `->item == value` in
`List`, `Collection`, `ReadOnlyCollection`, `KeyedCollection`, `ImmutableArray`,
`Generic::Queue`, `Generic::Stack`, `LinkedList`, `Dictionary::ContainsValue`,
`SortedDictionary`, `SortedList`, `OrderedDictionary`, `ImmutableDictionary`,
`ImmutableSortedDictionary`, `StringDictionary`, `StringCollection` and the
legacy non-generic `ArrayList`/`Queue`/`Stack`.

### 18a.4 Why it is not absorbed

Three reasons, in order of weight. It is **outside the finding's file list**,
and this programme's convention (CCF-016 → #1875, CCF-014's 240 other `Try*`
methods) is that a population discovered during remediation is recorded and
ticketed, not silently folded in. It contains a **third shape** (§18a.2) whose
repair is not CCF-010's repair. And absorbing it would make a family that is
currently entirely compatible depend on decisions about `SortedSet<T>`'s
`State`, which several earlier tickets (#1782, #1783, #1784, #1786) have
already shown to be delicate.

Nothing in §18a is known to be *wrong* beyond the four `std::sort` sites, which
are measured-by-analogy rather than by their own probe; #1912 must reproduce
each site itself before claiming anything.

---

## 19. Nomenclature note

This document uses "the finding" for SR-AUD-046 and "the record" for
`audit/AUDIT_CROSS_CUTTING_FINDINGS.md`'s CCF-010 section. Where §9 corrects
either, the original text stays as written and the correction is additive, per
the convention established by SR-AUD-081, SR-AUD-100 and SR-AUD-362.

---

## 18b. §18a corrected by measurement (ticket #1913, 2026-07-31)

§18a above stays **exactly as written**. It said so itself: its counts were
"measured-by-analogy rather than by their own probe", and it instructed #1912 to
"reproduce each site itself before claiming anything". #1912 did, through design
ticket **#1913**. This section records where the measurement disagreed. The full
record is `docs/CollectionsComparisonContractPlan.md`; only the corrections to
*this* document's claims are listed here.

| §18a claim | Measured |
|---|---|
| §18a.1: 5 default-ordering sites | **6.** `ImmutableList<T>::BinarySearch(item)` (`ImmutableList.hpp:1009`) is a sixth; §18a.3 counted it as an equality site because its hit test was `==`, but it mixed that with a raw `<` step. |
| §18a.1: "the four `std::sort` sites carry exactly §6.2's precondition violation" | **Correct, and understated.** `List<double>{3,NaN,1,2}.Sort()` gave `[1,3,NaN,2]` — the finite elements unsorted in a *four*-element list — and a 196-shape sweep left 164 shapes with a non-zero inversion count among the finite elements, worst case 216,078,912. CCF-010's own `Array::Sort` figure was 64/196 and 3,874. |
| §18a.2: "3 ordered associative containers" | **6.** `ImmutableSortedSet`, `ImmutableSortedDictionary` and `PriorityQueue<TElement,TPriority>` were not named. |
| §18a.2: a lifetime-of-the-container ordering violation | **True, and the observable consequence is silent data loss.** `SortedSet<double>` with `Add(NaN); Add(1); Add(2)` ends at `Count == 1` holding `[NaN]`: the two later elements were accepted by the call and discarded. `SortedDictionary`/`SortedList` with keys 1 and 2 *reject* `Add(NaN, …)` as a duplicate while `ContainsKey(NaN)` answers true. |
| §18a.3: "56 sites across 20 headers", listing `StringDictionary`, `StringCollection` and the legacy `ArrayList`/`Queue`/`Stack` | **55 defect-capable sites across 16 headers.** Eleven of the sites the named header list implies cannot exhibit the defect at all — their operand is `std::string` or a `void*` recovered from `std::any`. Four more take a caller-supplied comparer and are excluded on §18.1's own rule. |
| — (not in §18a) | **The six named default comparers.** `Generic::Comparer<T>::Default`, `EqualityComparer<T>::Default`, `ObjectComparer`, `ObjectEqualityComparer`, `NullableComparer`, `NullableEqualityComparer` — the port's *own* `Comparer<T>.Default` and `EqualityComparer<T>.Default` — were all the raw operator. `Comparer<double>::Default().Compare(NaN, x)` returned `0` for every `x`, making the object an invalid comparator in its own right. |
| — (not in §18a) | **Eleven hash-based containers**, whose failure is worse than the ordered ones and in the opposite direction: `Dictionary<double,V>` accepted the *same* NaN key without limit while `ContainsKey` and `TryGetValue` answered false forever. |

§18a.4's reasoning — that the population is recorded and ticketed rather than
folded into CCF-010 — is unaffected and was the right call: the family needed
seven implementation tickets, and one of them (**#1919**, the seven containers
whose backing `std::` container is part of their public surface) is
approval-blocked. Absorbing any of it into CCF-010 would have made a fully
compatible family depend on an approval.

Audit numbering is unchanged at **364**; no `SR-AUD-*` identifier was issued for
any of the above.

---

## 18c. §18b's own blocked half, delivered (ticket #1924, 2026-07-31)

§18b above stays **exactly as written**. Its closing paragraph named **#1919** —
the containers whose backing `std::` container is part of their public surface —
as approval-blocked. That approval was given on 2026-07-31 in the exact words of
`docs/CollectionsComparisonContractPlan.md` §10, and #1919 was delivered as
tickets **#1921** (SortedSet), **#1922** (Dictionary, HashSet), **#1923**
(FrozenSet, FrozenDictionary, ReadOnlySet, ReadOnlyDictionary) and **#1924**
(evidence and closure).

Two of §18b's own premises were wrong, and the corrections are additive here as
well:

| Claim | Measured |
|---|---|
| "**seven** containers whose backing `std::` container is part of their public surface" | **Six.** `SortedSet<T>`'s `SetIterator` typedef and `comparer()` are **private** — both are declared above the class's `public:` label — and the only public declaration mentioning the backing type, `Iterator`'s constructor, also takes the private nested `State` and so cannot be called. The compatible / public-representation split is **11 / 6**, not 10 / 7. The original table recorded, for each header, the declaration that mentioned the backing type, without checking which access-specifier region it fell in. |
| §10's approval of "the public `iterator`/`const_iterator` typedefs of `Dictionary<double,V>`, `HashSet<double>`, `FrozenSet<double>` and `FrozenDictionary<double,V>`" | **Those do not change.** libstdc++'s `_Node_iterator<Value, ConstantIterators, CacheHashCode>` does not mention the hasher, and both bools are unchanged for `float` and `double`. Only `long double` moves, because `__is_fast_hash<std::hash<long double>>` is specialised to `false` while the primary template says `true` for the policy hasher, so the node stops caching its hash code. |

What was measured rather than asserted: **0 of 57** `sizeof`/`alignof` readings
changed, for either instantiation family; of 1,758 external symbols, 106 were
removed and 105 added and **not one belongs to a non-floating instantiation**
(the single unpaired removal is a weak COMDAT `find` GCC now inlines away).
19 mutations ran — 9 killed, 6 **rejected at compile time** by the suite's
cross-container assertions, 2 declared controls and 2 declared *equivalents*
survived. ASan, LSan and UBSan produced **zero** diagnostics over all 46 probe
cases, restating §15's fact for these containers.

Two new defects were found while implementing it, both **outside** this family's
population and both ticketed rather than absorbed: **#1925** — the policy
selects on `std::is_floating_point_v`, so a *nullable or composite* floating key
(`Dictionary<std::optional<double>,V>`) keeps raw IEEE equality and cannot find
a NaN key that .NET finds; and **#1926** — `long double` hashed insertion is
**1.300×** slower for the cache-hash-code reason above.

Audit numbering is unchanged at **364**; no `SR-AUD-*` identifier was issued.
**SR-AUD-046 / CCF-010 and the whole #1912 continuation are now closed.**
