<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# ImmutableSortedSet comparator-equivalence design (#1936)

## 1. Status and non-approval boundary

This document scopes ticket #1936. It does not approve or implement the
recommended change. The current production header is unchanged. The retained
characterization probe records current behavior; it is not a passing
postcondition test for an unapproved result.

The original ticket described a direct-floating NaN defect. That reproduction
is accurate but the proposed type boundary is not: the root cause is the
generic `ImmutableSortedSet<T>::SetEquals` fallback. Any `T` whose ordering
comparer calls two raw-unequal values equivalent can reproduce it. Direct
`float`, `double`, and `long double` are the default-comparer instances of the
problem. A case-insensitive `ImmutableSortedSet<string>` is the retained
non-floating instance.

The completed #1934/#1925 direct nullable-floating contract is not reopened.
Its existing specialized membership branch already avoids the fallback for
exactly `optional<float>`, `optional<double>`, and `optional<long double>`.

## 2. Evidence and reproduction method

The durable source and output are:

- `build-probe/1936_immutable_sorted_set_matrix.cpp`;
- `build-probe/1936_immutable_sorted_set_matrix.csv` (105 lines: header, 96
  floating/nullable scenario rows, six surface rows, one non-floating defect
  row, and its surface row);
- `build-probe/1936_immutable_sorted_set_abi.cpp`, `.log`, and `.nm`.

The probe was compiled serially with GCC 14.2.0, C++23, `-O2 -DNDEBUG`, and
warnings-as-errors. It uses two quiet NaN payloads for every direct and
nullable floating type. Long-double payloads use `nanl("1")` and `nanl("2")`;
the contract tested is NaN class equivalence, not a claim that every platform
preserves those payload strings distinctly.

`ImmutableSortedSet<T>` is a public concrete class but does not derive from
this repository's `IImmutableSet<T>`, `ISet<T>`, or `IReadOnlySet<T>`.
Consequently there is no public/interface dispatch path for this class in the
current port. The repository interface also accepts `vector<T>`, whereas the
concrete method accepts another `ImmutableSortedSet<T>`. The probe records
`inherits_IImmutableSet=false` for all six types. This is a correction to the
initial requested premise that both concrete and interface paths existed.

The class has no `Equals` member or sequence-equality member. The evidence
therefore distinguishes its set predicates from a probe-local raw
position-by-position sequence comparison and from comparator-equivalent
sequence comparison.

## 3. Complete current-behavior matrix

The following table condenses the identical result across each three-type
group. `SE`, `Sub`, `PSub`, `Sup`, `PSup`, and `Ov` mean `SetEquals`, subset,
proper subset, superset, proper superset, and overlap. Result counts are
`Intersect/Union/Except/SymmetricExcept`. The full per-type rows and both
sequence comparisons remain in the CSV.

| Scenario | Type scope | SE | Sub/PSub | Sup/PSup | Ov | Result counts | Classification |
|---|---|---:|---:|---:|---:|---:|---|
| empty vs empty | all six | true | true/false | true/false | false | 0/0/0/0 | correct |
| equal one finite | all six | true | true/false | true/false | true | 1/1/0/0 | correct |
| NaN self | direct F | **false** | true/**true** | true/**true** | true | 1/1/0/0 | defective |
| NaN copy | direct F | **false** | true/**true** | true/**true** | true | 1/1/0/0 | defective |
| independent equal NaN, same payload | direct F | **false** | true/**true** | true/**true** | true | 1/1/0/0 | defective |
| independent equal NaN, distinct payload | direct F | **false** | true/**true** | true/**true** | true | 1/1/0/0 | defective |
| equal mixed NaN/finite, different insertion/payload | direct F | **false** | true/**true** | true/**true** | true | 3/3/0/0 | defective |
| the preceding five NaN-equal rows | optional F | true | true/false | true/false | true | matching | corrected by #1925 |
| +0 vs -0 | all six | true | true/false | true/false | true | 1/1/0/0 | correct |
| equal finite values and infinities, different insertion | all six | true | true/false | true/false | true | 3/3/0/0 | correct |
| `{NaN,1}` proper subset of `{NaN,1,2}` | all six | false | true/true | false/false | true | 2/3/0/1 | correct |
| reverse proper-superset case | all six | false | false/false | true/true | true | 2/3/1/1 | correct |
| duplicate NaNs and finite inputs collapse to equal set | direct F | **false** | true/**true** | true/**true** | true | 2/2/0/0 | defective equality/proper predicates |
| preceding duplicate case | optional F | true | true/false | true/false | true | 2/2/0/0 | correct |
| same equivalence, default vs reverse comparer, NaN set | direct F | **false** | true/**true** | true/**true** | true | 3/3/0/0 | defective equality/proper predicates |
| preceding comparer case | optional F | true | true/false | true/false | true | 3/3/0/0 | correct |
| this comparer collapses the other's `{1,2}` | all six | true | true/false | true/false | true | 1/1/0/0 | correct; this comparer governs |
| only other comparer collapsed `{1,2}` | all six | false | false/false | true/true | true | 1/2/1/1 | correct; stored other has one value |
| null self | optional F | true | true/false | true/false | true | 1/1/0/0 | correct |
| `{null}` proper subset of `{null,NaN}` | optional F | false | true/true | false/false | true | 1/2/0/1 | correct |
| case-insensitive `{"A"}` vs `{"a"}` | string/custom comparer | **false** | true/**true** | true/**true** | true | 1/1/0/0 | generic defect |

`F` in the table means each of `float`, `double`, and `long double`. For every
defective equal-NaN row, raw sequence equality is false and comparator
sequence equivalence is true. Signed zero and ordinary finite/infinity raw
sequence equality are true. Different ascending/descending enumeration order
is not sequence equality, even when set equality after rehashing is true.

All operations except `SetEquals` and its two “proper” dependents return the
mathematically expected sets in the matrix. This includes `Intersect`,
`Union`, `Except`, and `SymmetricExcept`. `IsSubsetOf`, `IsSupersetOf`, and
`Overlaps` are correct. Equal defective sets are simultaneously reported as a
proper subset and a proper superset because both methods are implemented as a
non-proper predicate followed by `!SetEquals`.

## 4. Current .NET behavior and contract distinctions

The comparison is source-derived from current `dotnet/runtime` `main` as
observed on 2026-08-01 because this repository environment has no `dotnet` or
Mono runtime. The authoritative sources are:

- [ImmutableSortedSet<T> current source](https://github.com/dotnet/runtime/blob/main/src/libraries/System.Collections.Immutable/src/System/Collections/Immutable/ImmutableSortedSet_1.cs);
- [Double current source](https://github.com/dotnet/runtime/blob/main/src/libraries/System.Private.CoreLib/src/System/Double.cs);
- [ImmutableSortedSet<T>.SetEquals API](https://learn.microsoft.com/en-us/dotnet/api/system.collections.immutable.immutablesortedset-1.setequals?view=net-10.0).

The source makes the result mechanically determined:

1. `SetEquals` returns true immediately for `ReferenceEquals(this, other)`.
2. For an immutable or mutable sorted set with an equal comparer object, it
   requires equal counts and performs a linear ordered scan using
   `source.KeyComparer.Compare(item, current) == 0`.
3. Otherwise it creates a `SortedSet<T>(other, this.KeyComparer)`, checks the
   post-collapse count, and performs the same comparer-based linear scan.
4. `Double.CompareTo` returns zero for two NaNs and for signed zero. It orders
   NaN below non-NaN values. `Double.Equals` makes NaN reflexive, while raw C#
   `double.operator==` remains NaN-nonreflexive.

Therefore current .NET answers as follows:

| Case | .NET result |
|---|---|
| NaN-bearing set `SetEquals(self)` | true by reference fast path |
| copied/reference-alias set | true |
| independently constructed equal NaN sets, same or distinct payload | true by comparer scan |
| mixed NaN/finite equal sets and different insertion order | true |
| +0 set vs -0 set | true; each comparer-equivalent set retains one zero |
| same comparer object | compatible linear comparer scan |
| distinct comparer objects with the same equivalence relation | rebuild under this comparer, then true |
| different equivalence relations | result is intentionally governed by this set's comparer and can be directional |
| equal NaN sets proper subset/superset | both false |
| subset/superset/overlap and four immutable set-producing operations | governed by this set's comparer; NaN is findable and equivalent |

These are distinct notions:

- element raw equality: `double.NaN == double.NaN` is false;
- element default equality: `double.NaN.Equals(double.NaN)` is true;
- ordering equivalence: `Comparer<double>.Default.Compare(NaN, NaN) == 0`;
- set mathematical equality: membership equivalence under the set's ordering
  comparer, independent of insertion and enumeration identity;
- sequence equality: positional and order-sensitive. LINQ `SequenceEqual`
  uses `EqualityComparer<T>.Default`; it is NaN-reflexive for `double`, but two
  equal sets enumerated in opposite comparer order are not equal sequences;
- object identity: .NET `ImmutableSortedSet<T>` does not override
  `Object.Equals`; reference equality remains distinct from `SetEquals`.

## 5. Root cause and complete affected surface

The production method first rebuilds `other` under `data_->key_comp()` and
correctly checks the post-collapse count. It then has two paths:

- direct nullable floating types use ordered membership lookup and are correct;
- every other `T` calls `*data_ == *otherRehashed`.

`std::set::operator==` compares elements position by position with raw element
`operator==`. It does not use the tree comparator. Rehashing puts both ranges
in the same comparator order but cannot make raw equality implement comparator
equivalence. Direct floating NaN and case-insensitive string values expose the
same generic mismatch.

This is not caused by `std::equal` in sharp-runtime source (although the
standard container equality has equivalent positional semantics), ignored
comparer precedence, a wrong rehash fast path, immutable-wrapper delegation,
or an unpropagated direct-floating alias. It is a general set-algorithm defect
after otherwise-correct rehashing.

Production callers of the defective result are exactly:

- the public concrete `SetEquals` call itself;
- `IsProperSubsetOf`, through `IsSubsetOf(other) && !SetEquals(other)`;
- `IsProperSupersetOf`, through `IsSupersetOf(other) && !SetEquals(other)`.

No other production helper calls it. `Contains`, non-proper relations,
`Overlaps`, `Intersect`, `Union`, `Except`, and `SymmetricExcept` have separate
comparator-based implementations. `SortedSet<T>::SetEquals` already performs
a linear two-direction comparator-equivalence scan and is not defective.
`ImmutableHashSet<T>::SetEquals` uses membership under this set's hasher and
equality predicate and is not this algorithm.

The semantic affected scope of a generic correction is all
`ImmutableSortedSet<T>` instantiations for which raw `==` and comparator
equivalence disagree. Default direct floating types are the ticket's bounded
observable examples. Custom comparers can expose it for otherwise ordinary
types. Instantiations whose two relations agree keep the same answers.

## 6. Design options

### Option 1 — use this set's comparator equivalence (recommended)

After the existing rehash and count check, scan both ordered ranges together.
For each pair `a,b`, require `!less(a,b) && !less(b,a)`. Optionally return true
first when both immutable wrappers share the same `data_` pointer. This is the
algorithm already used by `SortedSet<T>::SetEquals` and mirrors .NET's
compatible-comparer helper.

- Before/after: only comparator-equivalent/raw-unequal equal sets change false
  to true; their two proper predicates change true to false. All matrix
  results otherwise remain unchanged.
- Type scope: one generic method body; behavior moves only where relations
  differ. No floating or nullable specialization is needed.
- Source compatibility: declarations and call sites unchanged.
- Template/inline ABI: the inline template body changes. Existing mangled
  `SetEquals<T>` names should remain, but emitted code and the set of called
  comparator/template helpers can change for every instantiated `T`.
- Aliases/iterators/layout: no alias, iterator spelling, field, size,
  alignment, or vtable change. Current probe measurements are 16/8 for every
  tested set and 8/8 for each deduced tree const iterator.
- Symbols: no new public declaration or expected public mangled-name change;
  instantiated body bytes and local/weak helper inventory can move and must be
  measured.
- Complexity: existing rehash is O(m log m); the post-check remains O(n), with
  up to two comparer calls instead of one raw equality call. A shared-root
  check makes self/copy O(1).
- Performance: rehash normally dominates independent comparisons. Expensive
  custom comparers can make the O(n) tail slower than raw equality; permanent
  benchmarks must cover self/copy, equal independent, unequal early/late, and
  comparer mismatch.
- Permanent tests/mutations: see section 7.
- Rollback: restore the previous method body and tests' current-behavior
  expectation; no data migration or representation rollback is needed.

### Option 2 — use sharp-runtime's selected equality policy

Replace raw equality with `System::detail::equalValues` or
`DefaultKeyEqual<T>`. This fixes default floating NaN and agrees with #1934 for
the bounded nullable types.

It is rejected because a sorted set's uniqueness and membership are defined
by its ordering comparer, not `EqualityComparer<T>.Default`. It fails the
case-insensitive string example and can disagree with any runtime custom
ordering. Source/layout effects are small and complexity is O(n), but the
result would not match current .NET or the rest of this class.

### Option 3 — delegate to the corrected `SortedSet` implementation

Conceptually this reuses the correct linear comparator scan. In this port,
however, `SortedSet<T>` fixes its comparator at compile time while
`ImmutableSortedSet<T>` stores a runtime `std::function`. Delegation would
need a new comparer-aware adapter/API or another reconstruction, introduce
avoidable allocation and template surface, and risk alias/iterator/symbol
movement. It is rejected in favor of copying the small established algorithm.

### Option 4 — mutual ordered lookup

After rehashing, verify that every value in each set is findable in the other.
This is semantically correct under the common comparator and resembles the
current nullable branch.

It is O(n log n) after an O(m log m) rebuild, rather than the available O(n)
ordered scan. One-direction lookup plus equal post-collapse counts is already
sufficient, so mutual lookup adds no correctness. It preserves declarations,
aliases, iterators, and layout but emits more lookup machinery. Rejected for
unnecessary cost.

### Option 5 — retain and document the divergence

This preserves every source/template artifact and today's speed, but violates
set reflexivity, makes equal sets simultaneously proper subsets and
supersets, differs from .NET, and contradicts the class's own non-proper
predicates. Rejected.

### Option 6 — specialize direct floating instantiations only

A branch gated by `std::is_floating_point_v<T>` would fix exactly `float`,
`double`, and `long double` while retaining #1925's separate nullable branch.
It is behaviorally narrower and has no declaration/layout consequence.

It is rejected as the preferred design because the retained string probe
proves the same helper is generically wrong, it duplicates algorithm paths,
and it leaves runtime custom-comparer parity knowingly broken. It remains a
valid fallback only if the user explicitly approves a direct-floating-only
contract and accepts the documented generic divergence.

## 7. Exact implementation, test, mutation, and rollback scope

If Option 1 is approved, the exact production scope is one method body in
`modules/collections/include/System/Collections/Immutable/ImmutableSortedSet.hpp`:

1. optionally return true if `data_ == other.data_`;
2. retain rehashing of `other` under this set's comparator;
3. retain the post-collapse size check;
4. replace both the nullable special case and raw set equality fallback with
   one ordered, two-direction comparator-equivalence scan for every `T`;
5. change no declaration, alias, field, iterator, constructor, or other set
   operation.

Permanent acceptance tests should cover all three direct floating types, all
three nullable regression controls, same/distinct NaN payloads, signed zero,
finite/infinity, empty/single/mixed/duplicate sets, self/copy/independent and
insertion-order cases, proper relations, every related operation in section 3,
same-equivalence different comparer objects/order, different equivalence
relations with this-comparer precedence, and the non-floating
case-insensitive string case. Existing raw optional and raw sequence behavior
must remain unchanged. The port's absent interface path should be pinned by a
compile-time surface assertion only if repository test style supports it.

Mutation obligations are:

- restore raw `std::set::operator==` (killed by every direct NaN and custom
  string equality test);
- compare in only one direction (killed by asymmetric/custom comparer cases);
- use selected equality instead of comparator equivalence (killed by string);
- skip rehashing or compare ranges in their original order (killed by reverse
  comparer and collapsing-equivalence cases);
- compare size before rather than after rehash (killed by the collapsing case);
- incorrectly broaden or remove nullable policy selection (killed by existing
  #1925 matrix and negative boundary).

Rollback is a single-method revert plus removal/reversion of the newly
approved positive postcondition tests. No serialized state, layout, object
representation, or migration action is involved.

## 8. Recommendation and exact approval wording

Recommend Option 1 as a separate #1936 implementation batch. It should not be
folded into or described as reopening #1934/#1925. The default direct-floating
symptom belongs to already-supported CCF-010 ordering; the repair itself is a
generic `ImmutableSortedSet` set-mathematics correction.

The correction is source compatible and representation neutral, but it is an
observable semantic and inline-template-body change. It therefore cannot be
implemented under the current no-new-implementation approval. It needs
explicit semantic/template approval even though it does not need a new public
declaration, alias, iterator, layout, or vtable approval.

Copyable approval wording:

> Approve ticket #1936 Option 1 exactly: change only
> `ImmutableSortedSet<T>::SetEquals` so that, after rebuilding `other` under
> this set's existing ordering comparer and checking the post-collapse count,
> it compares the two ordered ranges by the comparator-equivalence relation
> `!less(a,b) && !less(b,a)` for every `T`; a shared-backing-data true fast path
> is also approved. This intentionally fixes direct `float`, `double`, and
> `long double` NaN reflexivity and the same generic custom-comparer defect,
> and consequently makes equal sets not proper subsets or supersets. Preserve
> this-comparer precedence, all other set-operation results, raw floating and
> optional operators, public declarations and aliases, iterator types, object
> layout, and vtables. Add the complete direct/nullable/custom-comparer matrix,
> mutation proof, performance comparison, and source/symbol/layout evidence.
> Do not change #1934/#1925 policy selection or any other collection.

Rejected alternatives are selected equality, `SortedSet` delegation,
mutual lookup, documented divergence, and a direct-floating-only
specialization. No recommendation in this document is user-approved.

## 9. Option 1 implementation evidence (2026-08-01)

The user subsequently approved Option 1 exactly as quoted above. Commit
`0ce730a6` changes only the generic `ImmutableSortedSet<T>::SetEquals` body.
It first returns true for shared backing data, rebuilds `other` under this
set's current comparator, retains the post-collapse count check, and then
scans both ordered ranges. Each pair must satisfy both `!less(a, b)` and
`!less(b, a)`. The method returns true only when both ranges end together.
There is no raw element equality, raw `std::set` equality, floating-point
specialization, nullable specialization, or change to another collection.

The retained 105-row pre-fix probe was rebuilt before the production edit and
was byte-for-byte identical to the design artifact (CSV SHA-256
`673c77899dcf8cd7d7de9edc7e1d9fbdd135033b1d5cfe6fa848e6c5be877d96`).
The post-fix probe changed only the approved rows:

- direct `float`, `double`, and `long double` NaN-bearing self, shared-copy,
  independent, insertion-order, same/distinct-payload, mixed, duplicate, and
  comparer-identical equality cases changed from false to true;
- their equal-set proper-subset and proper-superset results changed from true
  to false;
- case-insensitive string equality for `{"A"}` and `{"a"}` changed from false
  to true;
- nullable floating controls and every retained `IsSubsetOf`, `IsSupersetOf`,
  `Overlaps`, `Intersect`, `Union`, `Except`, and `SymmetricExcept` result were
  unchanged. Empty, signed-zero, finite, infinity, genuinely unequal, and
  genuinely proper controls also retained their expected results.

Commit `0d7d1cc6` adds 11 permanent contract tests. They cover the complete
direct `float`, `double`, and `long double` matrix; nullable `float`, `double`,
and `long double` controls including null, NaN, signed zero, finite values,
and infinities; case-insensitive strings; raw-unequal values collapsed by a
custom comparator; this-comparer precedence; comparer-mismatched inputs;
post-collapse cardinality; ordinary raw-equal controls; both proper
relations; and all seven unchanged related operations. The focused selection
passed 16/16 tests and the full Collections.Core executable passed 2,763/2,763
tests after adding the 11 tests.

The final eight-mutation campaign had no unexpected survivor. Restoring raw
`std::set` equality, using raw element equality, testing only
`!less(a, b)`, skipping the rebuild, using the other set's comparator, and
bypassing the generic body for direct floating types were killed by the
permanent tests. Removing the post-collapse count check was behaviorally
equivalent because the final two-range completion check still rejects unequal
cardinality; the required count check remains as an early post-collapse
invariant. Removing the shared-backing fast path was also behaviorally
equivalent and affects performance only. Neither equivalent mutation is
reported as killed.

The before/after source and object probes used representative `float`,
`double`, `long double`, `optional<double>`, `string`, and `int`
instantiations; the string instantiation was also exercised with a runtime
case-insensitive comparator. Public declarations and mangled `SetEquals`
names, private type aliases, deduced public iterator types, `noexcept`, and
`constexpr` status are unchanged. Every representative set remains 16 bytes
with 8-byte alignment, has one `data_` field at offset zero, is standard
layout and non-polymorphic, and has no vtable or virtual slots. Every const
iterator remains an 8-byte, 8-byte-aligned `std::_Rb_tree_const_iterator` of
the corresponding element type. `SetEquals` remains non-`noexcept` and
non-`constexpr`.

The public mangled-name set and all 35 undefined external names were
identical. At `-O0 -fno-inline`, defined external helper names changed from
1,441 to 1,397: 50 weak raw `std::set`/`std::_Rb_tree`/`std::equal`/element
equality and old optional-lookup helpers disappeared, and six weak
`std::shared_ptr::operator==` instantiations appeared for the true fast path.
The weak `SetEquals` body grew from 0x14a bytes for five representatives and
0x1ed for `optional<double>` to 0x380 for every representative. These are the
approved inline-template/helper effects, not public declaration or layout
changes. Unchanged layout does not imply an unchanged template body, so every
affected target was rebuilt.

The performance probe used five alternating warm-up pairs and eleven measured
alternating pairs. Median pre/post time and paired post/pre median ratio were:

| case | pre median (ns) | post median (ns) | paired ratio |
|---|---:|---:|---:|
| self, 5,000 calls | 39,128,619 | 9,067 | 0.000233 |
| shared copy, 5,000 calls | 39,181,910 | 8,807 | 0.000226 |
| equal independent | 1,180,074 | 1,288,127 | 1.087219 |
| equal, different insertion order | 1,190,554 | 1,275,954 | 1.083954 |
| comparer-mismatched equivalent | 2,233,449 | 2,276,880 | 1.037902 |
| unequal, early | 975,298 | 1,002,559 | 0.996970 |
| unequal, late | 1,221,431 | 1,313,074 | 1.111172 |
| NaN-bearing independent | 514,820 | 661,156 | 1.255290 |
| custom-comparer strings | 442,964 | 657,540 | 1.510281 |
| large integer control | 3,587,010 | 2,793,064 | 0.989305 |

The self and shared-copy results demonstrate the intended O(1) fast path.
Independent comparison retains O(m log m + n) rebuild-and-scan complexity.
The NaN and custom-string before measurements stopped at the old incorrect
answer and therefore do not represent identical semantic work. The integer
control was noisy (paired ratio range 0.446--2.073), and the remaining
independent-set changes did not establish a separate stable regression. No
new inactive ticket was created.

The main and sanitizer builds recompiled every known affected immutable-set
consumer plus the new test translation unit after the header timestamp, then
relinked Collections.Core. Focused ASan+UBSan execution passed all 16 selected
tests. A separate 2,000-iteration ASan+UBSan probe exercised construction,
rebuilding, comparison, shared copy, move, iteration, destruction, direct and
nullable NaNs, and a stateful custom comparator capture without a diagnostic.
The LSan-enabled discovery attempt ran all 11 new tests successfully but then
failed at LeakSanitizer's ptrace step, so no clean LSan result is claimed.
Sanitizers cannot prove comparator equivalence, reflexivity, or mathematical
set semantics; permanent tests and mutation evidence remain the correctness
gate.

Two design premises were refined without changing the approved contract. The
post-collapse count check is independently mutation-equivalent to the final
two-ended scan but is retained exactly as approved, and default/custom string
comparators share one `ImmutableSortedSet<string>` template instantiation
rather than producing separate public template symbols. No separate defect
was found, no SR-AUD identifier was issued, and frozen audit numbering remains
unchanged.
