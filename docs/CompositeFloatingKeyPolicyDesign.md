# Nullable and composite floating comparison policy (#1925, #1934)

**Status:** design complete; #1925 classified `needs_user`; no implementation  
**New related ticket:** #1934, default comparer selection for nullable floating
values  
**Audit numbering:** unchanged; neither ticket has an SR-AUD identifier  
**Measured implementation:** batch base `0e1b47d6`

## 1. Classification and recommendation

#1925 is **not** compatible implementation-ready. Its authoritative record
already says that changing the default backing predicates moves public template
types and needs separate approval. Measurement also disproves part of the
record's proposed scope:

- `std::optional<double>` is a usable hashed key today and has the measured
  unfindable-NaN defect. It is the direct C++ representation used here for
  .NET `Nullable<double>`.
- libstdc++ 14 provides no invocable `std::hash<std::pair<int,double>>` or
  `std::hash<std::tuple<double>>`. `Dictionary` with either key does not merely
  use the wrong equality; its default constructor is deleted and the
  instantiation cannot be used. Adding a project hash would be new supported
  surface, not the same repair as optional.
- the ordered defect is not "milder." For optional, pair, tuple, array,
  variant, and vector examples, inserting `{NaN, 1, 2}` under the same leading
  composite shape leaves one node. `find` then reports that same node for all
  three keys. This is the same non-strict-weak-ordering data-loss class #1919
  repaired one level up.
- current .NET has type-specific recursion for `Nullable<T>` and
  `ValueTuple<...>`. It does **not** recursively inspect every arbitrary user
  type. `EqualityComparer<T>.Default` calls that type's `IEquatable<T>.Equals`
  or `Equals`; `Comparer<T>.Default` calls its comparison contract. Arrays use
  their own reference equality by default. A rule that rewrites every C++ type
  "holding a floating member" would not model .NET and is not implementable in
  C++23 without an explicit customization contract.

The recommended bounded action is therefore:

1. approve one coordinated **direct nullable-floating** group for
   `std::optional<float>`, `std::optional<double>`, and
   `std::optional<long double>`;
2. in #1925, select null-aware, underlying-default hash/equality/order predicates
   for those key types across every current default-key consumer;
3. in #1934, make `Comparer<std::optional<F>>::Default()` and
   `EqualityComparer<std::optional<F>>::Default()` use the same nullable
   contract for those direct floating `F` types;
4. preserve `std::optional`'s raw lifted-style `operator==`, matching C#'s
   nullable `==` distinction;
5. leave pair, tuple, array, variant, vector, nested optional, and user-defined
   types unchanged until each mapping and hash-support decision has a precise
   contract.

#1925 and #1934 may be approved together because they are two entry points to
the exact same `Nullable<F>` default comparison contract. They must not be
grouped with network exception propagation, date/time grammar, or #1926.

## 2. Authoritative-record corrections

Original ticket text and acceptance criteria remain preserved in
`plan.sqlite3`. These are explicit corrections.

### 2.1 Correction to #1925: pair/tuple hash behavior

The record says pair and tuple "keep raw IEEE equality" in the same way as
optional. That is true of their operators and of ordered containers. It is not
the current hashed-container behavior on this toolchain: the required standard
hash is unavailable, so `Dictionary<std::pair<int,double>,V>` and
`Dictionary<std::tuple<double>,V>` fail to compile. A future hash specialization
would make previously ill-formed instantiations usable and must specify hash
combination, exception, portability, and public-type consequences.

### 2.2 Correction to #1925: ordered severity

The record calls the ordered case "milder." The retained probe demonstrates
silent key collapse. With NaN first, three distinct keys produce one map/set
node, and every lookup aliases that node. The strict-weak-ordering violation is
not a cosmetic order difference.

### 2.3 Correction to #1925: arbitrary user types

.NET does not field-walk arbitrary `T`. Its generic comparer calls `T`'s own
equality or comparison implementation. Sharp-runtime must likewise respect a
user-defined C++ type's `operator==`, ordering, and hash unless the project
introduces an explicit opt-in customization point. Automatic member recursion
is neither available in C++23 nor semantically faithful.

### 2.4 Correction to #1914: the generic nullable default was not covered

#1914's completion note says all six comparer surfaces route through the
policy. Its C9/C10 nullable cases exercise the dedicated
`NullableComparer<double>` and `NullableEqualityComparer<double>`, which are
correct. They do not exercise
`Comparer<std::optional<double>>::Default()` or
`EqualityComparer<std::optional<double>>::Default()`. Those generic defaults
still call the non-recursive helpers and remain wrong. This separately
observable surface is recorded as #1934 rather than silently widening #1925's
key-only implementation.

### 2.5 Correction to #1922: D10 was not a passing acceptance row

#1922's acceptance text says D1–D10 return the .NET answer, but its retained
postfix log has:

```text
D10 result=sameObject=false otherPayload=false count=1
```

The closure design explicitly reclassified D10 as an out-of-scope sentinel and
created #1925. #1922's original acceptance text is preserved, but it must not be
cited as evidence that nullable keys were fixed.

## 3. Current sharp-runtime measurements

### 3.1 Retained and permanent evidence

The existing permanent test
`CollectionsComparisonContract.NullableFloatingKeysKeepRawIeeeEqualityForNow`
passes and pins:

```cpp
Dictionary<std::optional<double>, int> dictionary;
dictionary.Add(std::optional<double>(NaN), 1);
dictionary.TryGetValue(std::optional<double>(NaN), value); // false
```

The focused run executed three tests: the two dedicated Nullable comparers
passed their .NET-compatible expectations, while the nullable dictionary test
passed its deliberate divergence expectation.

### 3.2 New matrix

`build-probe/1925_composite_key_matrix.cpp`, GCC 14.2, C++23, `-O2`, one
compiler job, measured:

| Type | Standard hash invocable | Hashed duplicate NaN count | Find inserted/copied NaN | Ordered `{NaN,1,2}` count |
|---|---:|---:|---:|---:|
| `optional<double>` | yes | 2 | false / false | 1 |
| `optional<optional<double>>` | yes | 2 | false / false | 1 |
| `pair<int,double>` | no | not usable | not usable | 1 |
| `tuple<double>` | no | not usable | not usable | 1 |
| `array<double,1>` | no | not usable | not usable | 1 |
| `variant<double,int>` | yes | 2 | false / false | 1 |
| `vector<double>` | no | not usable | not usable | 1 |

For all hashed rows that compile, the final count after adding two copied NaNs
and one ordinary value is 3; the ordinary value remains findable. For every
ordered row, `find(NaN)`, `find(1)`, and `find(2)` all return the one surviving
node. `std::variant<double,int>` is an additional example proving that the
original optional/pair/tuple list was not a closed structural category.

### 3.3 Default-comparer split (#1934)

Two NaNs with different payloads produced:

| Surface | Equality | Hashes equal | Compare NaN vs 1 |
|---|---:|---:|---:|
| `Comparer/EqualityComparer<optional<double>>::Default` | false | false | 0 |
| dedicated `NullableComparer/NullableEqualityComparer<double>` | true | true | -1 |
| expected .NET nullable default | true | true | -1 |

The underlying `equalValues(optional)` and `compareValues(optional)` helpers
also return false and 0 respectively. This is the root of both the generic
default-comparer defect and the key-policy gap.

### 3.4 Bounded optional candidate characterization

A probe-only empty hash/equality/order candidate for direct
`optional<double>` produces:

```text
map_count=1 duplicate_inserted=false find_nan=true
set_count=3 find_nan=true find_one=true find_two=true
```

Representation measurements on x86-64/libstdc++ 14:

| Object | Current | Candidate |
|---|---:|---:|
| unordered map type identity | standard predicates | different type |
| `sizeof` / `alignof` map | 56 / 8 | 56 / 8 |
| `sizeof` / `alignof` map const iterator | 8 / 8 | 8 / 8 |
| ordered set type identity | standard predicate | different type |
| `sizeof` / `alignof` set | 48 / 8 | 48 / 8 |
| `sizeof` / `alignof` set const iterator | 8 / 8 | 8 / 8 |
| current `Dictionary<optional<double>,int>` | 64 / 8 | candidate outer type not patched |

Equal sizes do not make the change ABI-neutral: comparator template arguments
make the standard containers and their symbols different types. An actual
implementation must repeat the complete #1919 representation and symbol
fixture across all affected outer collections.

### 3.5 Expected compile failures

The retained `1925_hash_unavailable_pair.log` and
`1925_hash_unavailable_tuple.log` instantiate the actual `Dictionary` and pin
the failure: its default constructor is deleted because the corresponding
`std::unordered_map` constructor is ill-formed with the non-invocable standard
hash. These are evidence, not negative consumer fixtures and not a request to
make those instantiations compile.

## 4. Current .NET behavior

Current dotnet/runtime source establishes distinct type-specific rules:

### 4.1 Floating primitive

`Double.Equals(double)` returns true for `==` or when both operands are NaN.
`Double.GetHashCode()` normalizes all NaNs and both zero signs before hashing.
`Double.CompareTo` places NaN below every non-NaN and makes all NaNs equivalent.

### 4.2 Nullable

`NullableEqualityComparer<T>.Equals` handles presence and delegates two present
values to `EqualityComparer<T>.Default`. Its hash calls the nullable value's
hash, which delegates to the contained value or zero. `NullableComparer<T>`
orders null before present and delegates two present values to
`Comparer<T>.Default`. Therefore a `double?` NaN key is reflexive, payload-
independent for hash/equality, and safely ordered.

This is intentionally different from C#'s lifted nullable `==`, which delegates
the primitive operator and says two nullable NaNs are unequal. The port's
`System::Nullable<T>::operator==` and `std::optional` operator must remain raw.

### 4.3 ValueTuple

Each `ValueTuple<T1,...>.Equals` field is compared with
`EqualityComparer<Ti>.Default`; `CompareTo` uses `Comparer<Ti>.Default` in
lexicographic order; hashing combines each field's `GetHashCode`. This makes a
floating tuple field NaN-reflexive and safely ordered. The port explicitly
documents `std::tuple` as its ValueTuple equivalent, so this is a real future
mapping issue—but not the same compatibility action as optional because the
current C++ hashed instantiation is unavailable.

### 4.4 Arbitrary types and arrays

`GenericEqualityComparer<T>` calls `T.Equals(T)`; the object fallback calls
`x.Equals(y)`. `GenericComparer<T>` calls `x.CompareTo(y)`. .NET does not
inspect fields and substitute floating comparers. An array's default equality
is reference identity, not an element-by-element floating policy. Therefore
"recurse into every composite" is not a .NET rule.

Official source references:

- dotnet/runtime `EqualityComparer.cs` (`NullableEqualityComparer`, generic and
  object comparers);
- `Comparer.cs` (`NullableComparer`, generic comparer);
- `Double.cs` (`Equals`, `GetHashCode`, `CompareTo`);
- `ValueTuple.cs` (per-field default equality/comparison and hash combination).

## 5. Affected sharp-runtime surface

The three `DefaultKey*` aliases are defined in public core header
`System/detail/ComparisonPolicy.hpp`. Sixteen collection headers consume at
least one alias.

### 5.1 Direct hashed consumers

| Public collection | Predicate-bearing storage | Public type consequence |
|---|---|---|
| `Generic::Dictionary<K,V>` | direct `unordered_map` | public `MapType`, `ToMap`, iterators |
| `Generic::HashSet<T>` | direct `unordered_set` | public `SetType`, iterators |
| `Frozen::FrozenDictionary<K,V>` | direct `unordered_map` | public `MapType`, factories/iterators |
| `Frozen::FrozenSet<T>` | direct `unordered_set` | public `SetType`, factories/iterators |
| `ObjectModel::ReadOnlyDictionary<K,V>` | direct `unordered_map` | public `MapType`, constructor, iterators |
| `ObjectModel::ReadOnlySet<T>` | direct `unordered_set` | public `SetType`, constructor, iterators |
| `Immutable::ImmutableDictionary<K,V>` | private alias, public `auto` iteration | deduced iterator return type changes |
| `Immutable::ImmutableHashSet<T>` | `std::function` predicate storage | behavior changes; backing standard type remains function-keyed |
| `Concurrent::ConcurrentDictionary<K,V>` | private direct `unordered_map` | object member type and inline symbols |
| `Generic::OrderedDictionary<K,V>` | private key-index `unordered_map` | object member type and inline symbols |
| `ObjectModel::KeyedCollection<K,I>` | private key-index `unordered_map` | object member type and inline symbols |

### 5.2 Ordered consumers

| Public collection | Predicate-bearing storage | Public type consequence |
|---|---|---|
| `Generic::SortedSet<T>` | private `std::set` alias | nested iterator/state and inline symbols |
| `Generic::SortedDictionary<K,V>` | private `std::map` | public nested iterator contains comparator-specific iterator |
| `Generic::SortedList<K,V>` | private `std::map` | public `auto begin/end` iterator type changes |
| `Immutable::ImmutableSortedDictionary<K,V>` | `std::function` comparator | behavior changes; backing type remains function-keyed |
| `Immutable::ImmutableSortedSet<T>` | `std::function` comparator | behavior changes; backing type remains function-keyed |

All default insertion, duplicate detection, lookup, removal, projection,
factory, growth/rebuild, index, range/view, and enumeration paths inherit their
container predicate. Explicit caller comparers and raw standard containers are
outside the change.

### 5.3 Non-key default comparer surface (#1934)

- `Comparer<std::optional<F>>::Default().Compare`;
- `EqualityComparer<std::optional<F>>::Default().Equals`;
- `EqualityComparer<std::optional<F>>::Default().GetHashCode`;
- any existing algorithm that explicitly invokes those default objects.

The dedicated `NullableComparer<F>` and `NullableEqualityComparer<F>` are
already correct and remain negative controls.

## 6. Design options

### Option A — preserve all current composite behavior

**Rule:** leave #1925 and #1934 unresolved or mark them deferred.

- Source/ABI: no change.
- Behavior: optional and variant NaN hash keys remain unfindable; ordered
  composites can silently collapse unrelated keys; generic optional default
  comparers remain inconsistent with the dedicated Nullable comparers.
- Migration/performance: none.
- Test: retain the divergence test and matrix.
- Rollback: none.

This was the old packet's recommendation. The newly measured ordered data loss
and #1934 gap make indefinite deferral less attractive, though it remains the
only no-change option.

### Option B — direct nullable-floating contract only (recommended)

**Rule:** for exactly `std::optional<F>` where `F` is `float`, `double`, or
`long double`:

- null equals null and hashes to zero;
- null orders before every present value;
- two present values delegate to the existing default floating equality, hash,
  and ordering;
- the three `DefaultKey*` aliases select empty policy predicate types;
- generic `Comparer`/`EqualityComparer` defaults use the same helper behavior;
- raw `std::optional`/`System::Nullable` operators remain unchanged.

**Exact precedence:** presence is decided first. Only two present operands reach
the floating policy. All NaN payloads compare equal and hash equally; NaN orders
before negative infinity; `+0.0` and `-0.0` remain equal and hash equally.

- Source compatibility: public backing aliases and deduced iterator types move
  for direct nullable-floating collection instantiations. Code that names raw
  standard container types where the collection now returns its policy type
  must migrate to `MapType`/`SetType`/`auto`, as under #1919.
- ABI: comparator-bearing standard-container types and many inline template
  symbols move. Equal measured sizes do not remove the type/symbol break. A
  coordinated rebuild is required.
- Layout/vtable: empty predicates are expected to preserve size/alignment, but
  every affected outer type and iterator must be measured. No field or virtual
  declaration needs to change. Existing comparer vtable slots keep their
  signatures and positions; only inline/default bodies change.
- `noexcept`/`constexpr`: can be preserved for direct floating optionals.
- Performance: one presence branch and, only for present NaN-sensitive
  comparisons, existing floating tests. Must be benchmarked against ordinary,
  null, finite, NaN, insert, and lookup workloads before closure.
- Migration: same kind as #1919, over a narrower additional instantiation
  family.
- Rollback: revert the optional helper/selector commit and invert the permanent
  behavior tests back to the pinned divergence.

### Option C — recurse into optional, pair, and tuple

This is the original acceptance proposal. It needs several decisions the
record did not specify:

- whether recursion includes nested shapes;
- whether `pair` maps to ValueTuple, KeyValuePair, or ordinary C++ pair
  semantics;
- the hash-combination algorithm and exception specification;
- whether making pair/tuple dictionaries compile is intentional additive API;
- arity limits and empty tuple behavior;
- whether `std::tuple` and the already-correct `System::TupleN` share a hash
  customization.

Ordered pair/tuple repair is justified by strict-weak-ordering safety, and
`std::tuple` has a documented ValueTuple mapping. Hashed support is a new
capability. **Do not approve as one vague recursion switch.** Split it after
Option B if desired.

### Option D — recurse through a broader standard-composite list

Adding array, variant, vector, or other standard types would close measured C++
operator defects but does not have one .NET semantic mapping. Some standard
types have hashes, others do not; sequences that map to .NET reference types
should not gain structural default equality. **Rejected as a universal
policy.** Individual mapped types need individual designs.

### Option E — automatically recurse into arbitrary user-defined fields

C++23 has no standard field reflection, and .NET respects user-provided
equality/comparison instead of rewriting fields. This option is both
unimplementable in the stated form and semantically wrong. **Rejected.**

### Option F — additive opt-in customization trait

Introduce a documented trait through which a C++ type opts into project default
hash/equality/order. This avoids guessing about arbitrary types, but it creates
new public customization API, ordering/hash invariants, ODR concerns, and
predicate-type transitions per specialization. It is a viable future design,
not approved by this prompt and unnecessary for direct optional.

### Option G — use wrapper predicate types for every key type

Always selecting `DefaultHash<T>`/`DefaultEqualTo<T>`/`DefaultLess<T>` would
make future overloads easier, but it changes public standard-container types for
**every** existing collection instantiation even when behavior is identical.
This is far broader than the defect. **Rejected.**

### Option H — additive collection factory or comparer constructor

A caller-selected nullable comparer could avoid changing defaults, but most of
the affected containers do not expose a uniform comparer constructor, and .NET
default behavior would remain wrong. Adding such APIs is a separate public
surface decision. **Not a repair for #1925/#1934.**

## 7. Recommended implementation split after approval

### #1934-A — direct optional-floating helper semantics

1. add a narrowly gated direct-optional-floating trait/helper in
   `ComparisonPolicy.hpp`;
2. make `compareValues`, `equalValues`, and `hashValue` implement presence-first
   nullable semantics only for that shape;
3. pin generic default `Comparer`/`EqualityComparer`, dedicated Nullable
   comparers, raw optional equality, payload NaNs, signed zero, infinities,
   null, finite values, and explicit caller comparers;
4. prove public declarations, virtual slots, type sizes, `noexcept`, and
   `constexpr` state unchanged.

### #1925-A — direct optional-floating key selection

1. select policy predicates in all three `DefaultKey*` aliases for the same
   direct optional-floating trait;
2. invert the existing divergence test and add complete hashed/ordered coverage
   across the sixteen consumers;
3. preserve compile-failure controls for pair/tuple hashed keys and unchanged
   behavior controls for out-of-scope composites;
4. repeat the #1919 layout, iterator, mangled/defined/undefined symbol, and
   negative-consumer evidence for both affected and unaffected families;
5. benchmark ordinary/null/NaN insert and lookup and document migration;
6. run focused ASan/UBSan. Sanitizers cannot detect comparison semantics; they
   cover only memory/undefined-behavior regressions introduced by the change.

The commits should remain logically separate for review but land in one
approved batch in the order #1934-A then #1925-A. The final state must not leave
`EqualityComparer<optional<F>>.Default` disagreeing with collection defaults.

### Future structural tickets, only if requested

1. ordered `std::tuple`/`std::pair` mapping and strict-weak-ordering safety;
2. hashed tuple/pair capability and hash combination;
3. explicit custom-type opt-in trait;
4. any standard composite with a separately justified .NET mapping.

No new SR-AUD identifiers are needed; audit numbering stays frozen.

## 8. Permanent test and evidence requirements

For the recommended direct nullable group:

- empty and `nullopt` containers;
- present finite minimum/maximum, `+0.0`, `-0.0`, both infinities;
- multiple NaN payloads and copies of the inserted key;
- duplicate Add/TryAdd, indexer overwrite, Contains/Find/TryGet, Remove, growth,
  trim/rebuild, frozen/read-only projection, immutable update, ordered range and
  enumeration;
- ordered `{NaN,1,2}` count and exact order `NaN,1,2`;
- hash/equality invariant and null hash;
- generic and dedicated nullable comparer consistency;
- raw optional and `System::Nullable` `operator==` remain NaN-nonreflexive;
- explicit caller comparator remains authoritative;
- `optional<int>`, `int`, `string`, direct float, pair, tuple, variant, array,
  vector, user-defined comparable/hashable type as negative/control families;
- pair/tuple Dictionary compile failures retained unless separately approved;
- before/after source declarations, symbols, undefined symbols, type names,
  sizes, alignments, field offsets where observable, iterator/proxy types,
  vtables, `noexcept`, and `constexpr` checks;
- complete component and repository gates.

No test may be weakened or deleted. The present divergence test is inverted
only after the corresponding approved implementation lands.

## 9. Exact copyable approval wording

Recommended coordinated approval:

> Approve the coordinated direct nullable-floating default-comparison change
> for tickets #1925 and #1934 only. For `std::optional<float>`,
> `std::optional<double>`, and `std::optional<long double>`, make
> `Comparer<T>.Default`, `EqualityComparer<T>.Default`, and
> `DefaultKeyLess`/`DefaultKeyHash`/`DefaultKeyEqual` use null-first ordering and
> the existing underlying floating .NET-compatible comparison, equality, and
> canonical NaN/signed-zero hash rules. I understand that this changes the
> comparator-bearing backing standard-container type, public
> `MapType`/`SetType` aliases, and affected iterator/deduced return types for
> that additional key family, requiring coordinated consumer rebuilds, even
> where measured size/alignment remains unchanged. Preserve raw nullable
> `operator==`, all public function declarations, vtable slots, object layout,
> `noexcept`, and `constexpr` state. Do not extend this approval to nested
> optional, pair, tuple, array, variant, vector, arbitrary user-defined types,
> new hash support, or ticket #1926.

Copyable no-change choice:

> Do not implement tickets #1925 or #1934. Retain and document the current
> nullable/composite default-comparison divergences and compile limitations.

Tuple-like expansion must be a separate explicit decision, for example:

> Design, but do not yet implement, a separately bounded mapping for
> `std::tuple`/`std::pair` floating members, including whether hashed
> Dictionary/HashSet instantiations that are currently ill-formed should become
> supported and the exact hash-combination/public-type contract. This wording
> does not approve that implementation.

## 10. Retained evidence

```text
build-probe/1919_probe.cpp                    (retained row D10)
build-probe/1919_postfix_plain.log            (D10 remains false/false)
build-probe/1925_composite_key_matrix.cpp
build-probe/1925_composite_key_matrix.bin
build-probe/1925_composite_key_matrix.log
build-probe/1925_composite_key_symbols.log
build-probe/1925_hash_unavailable.cpp
build-probe/1925_hash_unavailable_pair.log
build-probe/1925_hash_unavailable_tuple.log
build-probe/1925_focused_existing_tests.log
```

All new probes are repository-local and ignored. No production source, public
header, permanent test, or negative fixture was changed.

## 11. #1934 implementation record (2026-08-01)

The approved first work unit is complete. `ComparisonPolicy.hpp` now recognizes
only direct `std::optional<float>`, `std::optional<double>`, and
`std::optional<long double>` and applies presence-first comparison, null hash
zero, and the existing CCF-010 floating comparison/equality/hash policy to the
present value. Generic `Comparer<T>`/`EqualityComparer<T>`, their interface
dispatch, `ObjectComparer<T>`/`ObjectEqualityComparer<T>`, and the dedicated
Nullable comparers therefore agree. Raw `std::optional` and `System::Nullable`
operators were not changed and remain NaN-nonreflexive.

Correction: the implementation inventory in sections 2 and 4 did not name the
existing `ObjectComparer<T>` and `ObjectEqualityComparer<T>` wrappers. They
delegate to the same helpers, so their direct instantiations for exactly the
three approved optional floating types necessarily change too. This is not a
new generic capability: non-floating optionals, nested optionals, and other
composites still take their previous code paths.

Retained before/after behavior evidence is in
`build-probe/1934_prefix_nullable_comparer.log` and
`build-probe/1934_postfix_nullable_comparer.log`. The permanent five-test matrix
is `NullableFloatingComparerContractTests.cpp`; with the three existing
nullable contract tests, the focused result is 8/8, and the Collections.Core
executable is 2,747/2,747. Representative `optional<int>`, `optional<unsigned>`,
`optional<string>`, `optional<enum>`, and optional user-type output is identical
before and after.

The comparison/equality object and interface sizes and alignments remain 8/8
bytes for every approved type; declarations, virtual slot sets, and the tested
`noexcept`/`constexpr` properties are unchanged. The forced-instantiation
fixture has identical sharp-runtime symbol sets (438 defined symbols and 54
vtables before and after) and identical undefined sets (26). Its complete
defined-symbol set changes from 760 to 747 solely because the corrected inline
paths no longer instantiate 13 libstdc++ optional comparison/hash helpers; the
generated text bytes consequently differ. This is the approved template/inline
symbol consequence, not a layout change.

All six #1934 mutations were accounted for: nullable ordering, equality, hash,
signed-zero hash, and NaN canonicalization were killed by permanent tests; the
invalid null-presence mutation was rejected by a compile-time assertion; there
were no unexpected survivors. Focused ASan/UBSan completed without diagnostics.
LSan discovery was unavailable because the execution environment runs under
ptrace; that limitation is retained rather than reported as a clean LSan run.
Collection aliases remain unchanged at this checkpoint; #1925 is still the
dependent second work unit.
