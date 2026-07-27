<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# ReadOnlyDictionary::Empty() mutable-singleton contract

*Design record for ticket #1779 (`REMED-COLL-READONLYDICT-EMPTY-DESIGN`), audit
finding SR-AUD-359. Recorded 2026-07-27 before any production change. No
production or test source changed under this ticket.*

## 1. Executive decision

`System::Collections::ObjectModel::ReadOnlyDictionary<K,V>::Empty()` currently
returns a non-`const` reference to a process-wide function-local `static`
singleton. Because the class relies on its compiler-generated copy assignment
operator, ordinary assignment through that reference —
`ReadOnlyDictionary<K,V>::Empty() = someOtherInstance;` — silently rebinds the
singleton's private backing map for the remainder of the process, corrupting
every past and future caller of `Empty()` for that `<K,V>` instantiation.

**Decision:** change `Empty()`'s return type from
`static ReadOnlyDictionary<K, V>&` to `static const ReadOnlyDictionary<K, V>&`.
This is the selected architecture (§9); it is a one-line signature change that
closes the finding completely, is directly analogous to .NET's own
get-only `Empty` property (which has no setter and therefore cannot be
reassigned at the language level — see §4), and preserves every other
observable behavior, including the existing singleton-identity regression
test. **This is a public API signature change** (the qualifier of a returned
reference), so per this repository's established approval boundary (the same
one applied to ticket #1770/#1771's `ICollection::CopyTo` removal) it requires
explicit user approval before implementation. No production change is made
under this design-only ticket; implementation is proposed as separate,
inactive ticket **#1780**, marked `blocked` on that approval (§20).

## 2. Finding and reproduction

Audit evidence
(`audit/modules/collections/include/System/Collections/ObjectModel/ReadOnlyDictionary.hpp.audit.md`,
SR-AUD-359, medium): "`Empty()` returns a non-const reference to a process-static
wrapper, so default copy assignment can rebind its private shared backing map.
The direct probe prints `empty-before=0`, assigns a one-entry read-only wrapper
into `Empty()`, then prints `empty-after-assignment=1`."

This design re-reproduced the finding independently before any design decision,
per the run's reproduction requirement, in the repository-local, gitignored
`build-probe-readonlydict/` tree (matches the `build-probe-<name>/` convention
used by tickets #1770-#1771, #1775, and #1778; excluded by the repository's
`build*` `.gitignore` entry, so nothing under it is committed):

- `probe1_mutable_empty.cpp`, compiled `-std=c++23 -Wall -Wextra -Wpedantic
  -fsanitize=address,undefined -g` against `Core.Base`
  (`build/libsharp_runtime_core.a`) and run with `ASAN_OPTIONS=detect_leaks=0`:

  ```
  empty-before=0
  empty-after-assignment=1
  second-caller-observes=1
  same-instance=1
  ```

  This exactly matches the audit's own `empty-before=0` /
  `empty-after-assignment=1` symptom, and adds two facts beyond the original
  evidence: the contamination is visible to an *unrelated* second call site
  (`second-caller-observes=1`) that never touched the first call's local
  variable, and `&empty == &empty2` confirms it is the identical process-wide
  singleton object (`same-instance=1`), not a copy — i.e. this is a real,
  silent, global data-corruption hazard for every consumer of
  `ReadOnlyDictionary<K,V>::Empty()`, not a local mistake contained to one call
  site. No sanitizer diagnostic fires for this scenario (it is a logic defect,
  not a memory-safety one — the assignment is valid, well-defined C++; it is
  simply semantically wrong for a value advertised as the immutable empty
  singleton).
- `ReadOnlyDictionary_fixed.hpp` — an unmodified copy of the production header
  with only `Empty()`'s return type changed to
  `static const ReadOnlyDictionary<K, V>&`, used to validate the proposed fix
  without touching production source:
  - `probe2_fix_rejects_assignment.cpp` — attempts the same
    `empty = nonEmpty;` assignment against the fixed header. Compilation fails
    as intended:
    ```
    error: passing 'const System::Collections::ObjectModel::ReadOnlyDictionary<...>'
    as 'this' argument discards qualifiers [-fpermissive]
    ```
  - `probe3_fix_preserves_behavior.cpp`, compiled and run clean under
    ASan+UBSan (`all-assertions-passed=1`): confirms singleton identity
    (`&empty1 == &empty2`), emptiness, normal construction, `ContainsKey`,
    indexer access, and copy-construction of an independent local instance all
    remain exactly as they behave today.

## 3. Current sharp-runtime behavior

`modules/collections/include/System/Collections/ObjectModel/ReadOnlyDictionary.hpp`
(owned by the `Collections.Core` physical component):

```cpp
template<typename K, typename V>
class ReadOnlyDictionary {
    std::shared_ptr<std::unordered_map<K, V>> dict_;
public:
    explicit ReadOnlyDictionary(std::shared_ptr<std::unordered_map<K, V>> dictionary);
    static ReadOnlyDictionary<K, V>& Empty() {
        static ReadOnlyDictionary<K, V> empty(std::make_shared<std::unordered_map<K, V>>());
        return empty;
    }
    // getCountProperty, getIsEmptyProperty, ContainsKey, operator[], TryGetValue,
    // getKeysProperty, getValuesProperty, begin()/end() — all const, read-only.
protected:
    const std::unordered_map<K, V>& getDictionaryProperty() const;
};
```

Key facts:

- The class declares **no** public non-`const` member function. Every public
  accessor is `const`. The only way to mutate an existing instance's `dict_`
  member (rebind it to a different backing map) is the compiler-generated copy
  assignment operator (and move assignment operator), since neither is
  declared or deleted.
- `getDictionaryProperty()` is `protected` and returns `const&`, so no public
  caller — not even a subclass outside the type itself — can add, remove, or
  mutate entries in the wrapped `unordered_map` through this type. The wrapped
  map is genuinely read-only through every *public* accessor.
- The only observable mutation path is therefore whole-object assignment:
  `dict_` is reseated to point at an entirely different map. A copy of
  `Empty()` (e.g. `auto local = ReadOnlyDictionary<K,V>::Empty();`) cannot
  affect the singleton — assignment must go through the singleton reference
  itself, which is exactly what the non-`const` return type permits.
- `Empty()` is a function-local `static`, so it is lazily initialized on first
  use per `<K,V>` instantiation (C++11 thread-safe static-local-initialization
  guarantee) and lives for the remainder of the process. There is exactly one
  instance per distinct `<K,V>` pair for the life of the process — matching
  .NET's per-closed-generic-type static field lifetime.

## 4. Current .NET behavior

Read from
`/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/ObjectModel/ReadOnlyDictionary.cs`
on 2026-07-27, not from memory:

```csharp
public static ReadOnlyDictionary<TKey, TValue> Empty { get; } =
    new ReadOnlyDictionary<TKey, TValue>(new Dictionary<TKey, TValue>());
```

- `Empty` is a `{ get; }`-only auto-property backed by a `private readonly`
  compiler-generated field. **There is no setter.** `ReadOnlyDictionary<K,V>.Empty
  = x;` is a `CS0200` compile error in every C# consumer — the language itself
  makes the singleton unassignable, not a runtime check or a documented
  caution.
- Even if a caller obtained the reference and wanted to mutate it, .NET
  reference types have no C# operator that rebinds an *existing* object's
  fields through an object reference (`obj = other;` only rebinds the local
  variable, never the referent); the only way to mutate an object's own state
  is through its own instance methods, and `ReadOnlyDictionary<TKey,TValue>`
  exposes none that mutate `m_dictionary`. So .NET's `Empty` singleton is
  doubly protected: no settable property, and no instance mutator regardless.
- `Dictionary` (the protected escape hatch analogous to sharp-runtime's
  `getDictionaryProperty()`) returns the writable `IDictionary<TKey,TValue>`
  by reference for subclass use, mirroring sharp-runtime's protected accessor,
  but it is `protected`, not reachable by ordinary callers of `Empty`.

**Conclusion:** sharp-runtime's C++ port introduced this hazard by translating
a get-only auto-property into a mutable C++ reference-returning static method.
The fix in §9 (`const` reference) is not a design compromise — it is the
literal C++ expression of "no setter exists," reproducing .NET's guarantee
exactly rather than approximating it.

## 5. Complete affected-surface inventory

Searched the entire sharp-runtime repository (not just literal
`ReadOnlyDictionary` occurrences — also its sibling `ReadOnlySet`,
`ReadOnlyCollection`, and the `IReadOnlyDictionary<TKey,TValue>` interface, to
rule out an aliased or wrapper path to the same hazard):

- **Public declaration:** one class,
  `modules/collections/include/System/Collections/ObjectModel/ReadOnlyDictionary.hpp`
  (header-only template; component `Collections.Core`).
- **Private representation:** `std::shared_ptr<std::unordered_map<K, V>> dict_`
  — the only mutable state; no other member.
- **Constructors:** one public `explicit` constructor from
  `std::shared_ptr<std::unordered_map<K,V>>` (throws `ArgumentNullException`
  on null); implicit compiler-generated copy/move constructors (both benign —
  they create a distinct object, never touch the singleton); implicit
  compiler-generated copy/move **assignment** operators (the hazard).
- **Methods participating in the defect:** only `Empty()` itself. No other
  method returns a mutable reference to shared/static state; `getKeysProperty()`
  /`getValuesProperty()` return `std::vector<K>`/`std::vector<V>` **by value**
  (independent copies), and `operator[]`/`TryGetValue` return/copy `const V&`/
  `V&` **into caller-owned storage**, not references into the singleton.
- **Derived implementations:** none. `ReadOnlyDictionary<K,V>` has no virtual
  functions and is not subclassed anywhere in the repository (`grep -rn
  "public ReadOnlyDictionary" modules/ tests/` finds no result). Contrast with
  `System::Collections::Hashtable`/`ListDictionaryInternal` from ticket #1775,
  where the defect required parameterising over two live implementations of a
  shared interface — there is no interface here, so there is no analogous
  polymorphic-caller surface to inventory.
- **Interface paths:** `ReadOnlyDictionary<K,V>` does not implement
  `System::Collections::Generic::IReadOnlyDictionary<TKey,TValue>` or any other
  interface in this port (unlike .NET, where `ReadOnlyDictionary<TKey,TValue>`
  implements `IDictionary<TKey,TValue>`, `IReadOnlyDictionary<TKey,TValue>`,
  and the non-generic `IDictionary`). This port's type is a narrower, standalone
  wrapper. `IReadOnlyDictionary.hpp` and `IImmutableDictionary.hpp` are
  unrelated types with no shared code path to this defect.
- **Direct callers of `Empty()` in the repository:** exactly one, the test
  below. `grep -rn "ReadOnlyDictionary<.*>::Empty\(\)" modules/ tests/` and a
  broader `grep -rn "\.Empty()" modules/ tests/` (to catch any semantically
  equivalent alias or helper wrapping this call) both return only:
  - `modules/collections-object-model/tests/System/Collections/ObjectModelTests.cpp:304-305`
    (`Empty_IsEmptyAndCached`), which uses `auto&` (not an explicit non-`const`
    reference type), so it is source-compatible with the proposed fix — see
    §13.
- **Polymorphic callers:** none (no virtual dispatch is involved).
- **Tests:** `ObjectModelTests.cpp::ReadOnlyDictionaryTests` (10 cases,
  `modules/collections-object-model/tests/...`) and
  `ObjectModelBatch18Tests.cpp::ReadOnlyDictionaryBatch18Test` (8 cases) cover
  construction, `Count`, `IsEmpty`, `ContainsKey`, the indexer (found and
  missing), `TryGetValue`, `Keys`, `Values`, and the null-dictionary
  constructor guard. Exactly one case,
  `ReadOnlyDictionaryTests.Empty_IsEmptyAndCached`, touches `Empty()`, and it
  only asserts emptiness and singleton identity — it does **not** assert that
  `Empty()` resists assignment, which is precisely the missing assertion the
  audit's own per-file report calls out ("Tests do not verify `Empty` remains
  empty after copies, assignments, or access from another consumer.").
- **Examples / public-header consumer fixtures:** no dedicated
  `test/consumer/*.cpp` fixture exists yet for `Collections.ObjectModel`'s
  `ReadOnlyDictionary` specifically; §16 proposes one for the implementation
  ticket.
- **Documentation:** the class's own Doxygen comment on `Empty()` ("Gets a
  reference to a shared, permanently-empty instance") already asserts
  permanence but not immutability-via-the-reference; §10 corrects it as part
  of the proposed declaration.
- **Module dependencies:** `Collections.Core`'s only public dependency is
  `Core.Base` (for `SharpRuntimeHelper.hpp`, `ArgumentNullException.hpp`) plus
  the sibling `KeyNotFoundException.hpp` in the same component. The proposed
  fix touches only the return-type qualifier of one method; it adds no
  `#include`, no new dependency edge, and does not change the 41-module/
  90-edge boundary graph.
- **Audit evidence:** the per-file report cited in §2, and
  `audit/AUDIT_FINDINGS_INDEX.md` row for SR-AUD-359, both reviewed and left
  unmodified by this design ticket (their evidence is preserved verbatim; only
  the reconciliation commit's own new material is added, per §17).

## 6. Constraints

- Do not implement a public source-breaking change without explicit user
  approval (this run's boundary).
- Do not change `ReadOnlyDictionary`'s general assignability for
  non-singleton, caller-constructed instances beyond what is required to close
  the specific finding (avoid the scope creep the repository consistently
  avoids — see ticket #1775's "the two defects the finding records, and
  nothing else").
- Preserve the existing `Empty_IsEmptyAndCached` regression test's assertions
  (`getCountProperty() == 0` and `&empty1 == &empty2`) exactly; do not weaken a
  passing test to accommodate a redesign.
- No new dependency edge, no module-boundary change, no vtable/virtual-dispatch
  change (the class has none today and must not gain one solely for this fix).
- Match .NET's actual contract (§4), not a stricter or looser approximation of
  it, per this repository's parity philosophy.

## 7. Alternatives

Three materially different alternatives were evaluated (a fourth,
"do nothing, document only," was rejected outright without a full write-up: it
leaves a confirmed, reproducible global-state-corruption hazard live in a
public API and contradicts the audit's own remediation mandate, so it is not a
serious candidate).

### Alternative A — `const`-reference return type (SELECTED, §9)

`static const ReadOnlyDictionary<K, V>& Empty()`. Assignment through the
returned reference becomes a compile error (`discards qualifiers`); every
other operation — copy-construction from `Empty()`, reading `Count`/`Keys`/
`Values`/indexer, comparing `&Empty() == &Empty()` — is unaffected because
every one of those operations was already `const`-qualified or value-returning.

### Alternative B — return by value instead of by reference (REJECTED)

`static ReadOnlyDictionary<K, V> Empty()`, returning a fresh copy (cheap: one
`shared_ptr` copy-construction and a refcount increment) on every call instead
of a reference to a shared object. This also prevents the mutation hazard
(there is no shared object left to corrupt — each caller gets its own,
independent handle onto the same, still-immutable-through-every-public-path
empty map).

**Rejected** because it silently changes an established, tested, and
documented behavioral guarantee — reference/singleton identity — for no
additional safety benefit over Alternative A: `&Empty() == &Empty()` would no
longer generally hold (two temporaries have different addresses), which
directly contradicts the existing passing regression test
`ReadOnlyDictionaryTests.Empty_IsEmptyAndCached`'s `EXPECT_EQ(&empty1, &empty2)`
assertion and the class's own doc-comment ("a reference to a shared,
permanently-empty instance"). It would also diverge further from .NET, whose
`Empty` genuinely is one process-wide object referenced by every caller (a
get-only auto-property backed by one static field), not a value regenerated
per access. Weakening a passing test and an already-correct piece of the
existing contract to fix an unrelated defect is exactly the kind of
collateral change this repository's ticket-scoping convention avoids.

### Alternative C — delete `ReadOnlyDictionary`'s assignment operators entirely (REJECTED — broader than the finding, not a cosmetic variant of A)

In addition to (or instead of) Alternative A,
`ReadOnlyDictionary(const ReadOnlyDictionary&) = default;` stays, but
`ReadOnlyDictionary& operator=(const ReadOnlyDictionary&) = delete;` (and the
move-assignment counterpart) are added, so **no** instance of
`ReadOnlyDictionary<K,V>` anywhere — not just the `Empty()` singleton — can
ever be reseated after construction. This is the most literal match to .NET's
full immutability model: a .NET `ReadOnlyDictionary<TKey,TValue>` instance
genuinely cannot have its backing dictionary swapped out after construction,
by any caller, anywhere, not only the one returned by `Empty`.

**Rejected for this ticket's bounded scope** (not rejected as an idea in
principle — it is a legitimate future hardening, just not this finding's
fix): SR-AUD-359 is specifically about the `Empty()` singleton being
externally rebindable, not about whether an arbitrary caller-owned, non-shared
`ReadOnlyDictionary` local variable may be reassigned to wrap a different map.
Deleting assignment for *every* instance is strictly broader than what the
audit evidence demonstrates a defect in, and this repository's remediation
convention (ticket #1775 §"Bounded scope") is to fix exactly what the finding
records and nothing else, leaving genuinely separate hardening ideas for a
future, independently justified finding rather than folding them into this
one. It also has a real (if currently unobserved) compatibility cost:
`std::vector<ReadOnlyDictionary<K,V>>`-style storage, or any future internal
code that reassigns a local instance, would stop compiling; nothing in the
repository does this today (confirmed by the inventory in §5), but Alternative
A achieves the complete fix for the *actual* finding without foreclosing that
possibility for ordinary instances. If a future audit or consumer need
specifically demonstrates unwanted reassignment of a non-singleton instance,
that is new evidence for a new, separately scoped finding — not a reason to
broaden this one now.

## 8. Compatibility matrix

| Alternative | Memory safety | Ownership | Source compat. | ABI | Complexity | Test impact | Matches .NET |
|---|---|---|---|---|---|---|---|
| A — `const&` return (selected) | No change (already safe) | No change | Compatible for every in-repo caller (`auto&`); breaks only the exact hazardous explicit-non-`const`-reference pattern, which no caller uses | None (header-only template, no vtable, no exported symbol) | Trivial (one qualifier) | Existing test unaffected; one new assertion recommended (§14) | Exact — reproduces "no setter" |
| B — return by value | No change | New: no shared identity | Breaks `&Empty() == &Empty()` callers, including the existing regression test | None | Trivial | Existing regression test must be rewritten/weakened | Diverges — .NET's `Empty` is one shared instance |
| C — delete all assignment | No change | Strictly stronger (no instance ever reassignable) | Compatible today (no in-repo reassignment found) but forecloses a legitimate future use of ordinary (non-singleton) instances; broader surface for downstream (CNA/mobile-eggbert, not inspected per scope) to potentially hit | None | Small | Existing tests unaffected | Exact for singleton; over-matches for ordinary instances (.NET's restriction comes from "no setter on `Empty`," not from unassignability of every reference-typed value that happens to hold a `ReadOnlyDictionary`) |

Alternative A dominates: it is the only option that is simultaneously the
exact .NET-contract match, has zero new complexity, breaks no passing test,
and stays exactly within the finding's bounded scope.

## 9. Selected architecture

Change `Empty()`'s declared return type from `ReadOnlyDictionary<K, V>&` to
`const ReadOnlyDictionary<K, V>&`. No other member, representation, or
constructor changes. No virtual member is added, removed, or changed (the
class has none). No new base class, no new dependency edge.

## 10. Proposed declarations

```cpp
// modules/collections/include/System/Collections/ObjectModel/ReadOnlyDictionary.hpp

/**
 * @brief Gets an empty ReadOnlyDictionary instance.
 *
 * C++ counterpart of .NET ReadOnlyDictionary<TKey,TValue>.Empty, a get-only
 * property with no setter. Returning a const reference is the literal C++
 * expression of that contract: the process-wide singleton cannot be
 * reassigned through the value this method returns.
 * @return A const reference to a shared, permanently-empty instance.
 */
static const ReadOnlyDictionary<K, V>& Empty() {
    static ReadOnlyDictionary<K, V> empty(std::make_shared<std::unordered_map<K, V>>());
    return empty;
}
```

Everything else in the header (constructor, `getCountProperty`,
`getIsEmptyProperty`, `ContainsKey`, `operator[]`, `TryGetValue`,
`getKeysProperty`, `getValuesProperty`, `begin()`/`end()`,
`getDictionaryProperty()`) is unchanged — none of them are part of the
defect, and none needs to change to close it.

No change to the class's copy/move constructors or copy/move assignment
operators (Alternative C, §7, explicitly rejected for this ticket's scope):
ordinary caller-constructed instances remain freely copyable and assignable
exactly as today.

## 11. Ownership and lifetime model

Unchanged from current behavior, restated for completeness:

- `dict_` is a `std::shared_ptr<std::unordered_map<K,V>>`; the wrapper does
  not own the map exclusively — it shares ownership, matching .NET's
  wrap-not-copy semantics (mutations made directly to the wrapped map through
  a reference the *original* caller retained are still visible through this
  wrapper, since nothing here is a defensive copy).
- The `Empty()` singleton's `dict_` points at a map created once, at first
  use, that is never populated through any public path (no public mutator
  reaches it) and, after this fix, can never be swapped out from under the
  singleton either. Its lifetime is the remainder of the process, per
  standard C++11 function-local `static` semantics — unchanged by this
  design.
- Ordinary (non-`Empty()`) instances remain default-copyable and
  default-assignable value types over a shared backing map; this design does
  not touch that behavior (§7 Alternative C, rejected).

## 12. Validation and exception matrix

No new validated inputs and no new exception paths are introduced. The single
existing validation — the constructor's null-`dictionary` check — is
unchanged:

| Operation | Precondition | Result |
|---|---|---|
| `ReadOnlyDictionary(nullptr)` | `dictionary == nullptr` | throws `System::ArgumentNullException("dictionary")` (unchanged) |
| `Empty()` | none | returns the process-wide empty singleton, now by `const&` (changed: return type only) |
| `Empty() = other;` | — | **no longer compiles** (`error: passing 'const ReadOnlyDictionary<...>' as 'this' argument discards qualifiers`) — this is the intended remediation, not a new runtime exception; the hazard is removed at compile time, matching .NET's own compile-time `CS0200` rejection of `Empty = x` |
| `auto copy = Empty();` | — | unchanged: constructs an independent, separately owned instance; cannot affect the singleton before or after this fix |

## 13. Implementation phases

Proposed for implementation ticket #1780 (not performed under this design
ticket):

1. Change `Empty()`'s return type as in §10.
2. Update the Doxygen comment as in §10.
3. Add the new regression coverage in §14 to
   `ObjectModelTests.cpp::ReadOnlyDictionaryTests` (the file that already owns
   `Empty_IsEmptyAndCached`).
4. Rebuild and run the focused `Collections.ObjectModel`/`Collections.Core`
   test targets, then the full repository gate.
5. Add the standalone consumer fixture described in §16 if one does not
   already assert this behavior.
6. Update `NEXT.md`, `plan.md`, `plan.sqlite3`, and the SR-AUD-359 audit
   records to `remediated`, following the exact reconciliation pattern used by
   tickets #1774/#1775/#1778.

## 14. Permanent test plan

Add to `modules/collections-object-model/tests/System/Collections/ObjectModelTests.cpp`
(next to the existing `Empty_IsEmptyAndCached`), for the implementation
ticket:

- `Empty_ReturnTypeIsConstReference` (compile-time proof, not a runtime
  assertion): a `static_assert` on
  `std::is_same_v<decltype(ReadOnlyDictionary<std::string,int>::Empty()),
  const ReadOnlyDictionary<std::string,int>&>` so a future accidental
  reversion to a mutable reference is caught at compile time, not only by a
  runtime regression.
- `Empty_RemainsEmptyAfterConstructingUnrelatedInstances` — constructs several
  independent, non-empty `ReadOnlyDictionary` instances and copies of `Empty()`
  itself, then re-reads `Empty().getCountProperty() == 0`, directly covering
  the audit's own called-out gap ("Tests do not verify Empty remains empty
  after copies, assignments, or access from another consumer.") for every
  operation that *is* still possible after the fix.
- Retain `Empty_IsEmptyAndCached` verbatim (§6 constraint) — its
  `getCountProperty() == 0` and `&empty1 == &empty2` assertions continue to
  hold unmodified.
- A compile-fail assertion (`empty = someInstance;` must not compile) is
  **not** expressed as a GoogleTest case, consistent with how ticket #1770/
  #1771 handled the analogous "removed overload must not compile" evidence: it
  belongs in a repository-local probe log (§2), not in the permanent suite,
  since GoogleTest cannot assert a compile failure.

## 15. Sanitizer plan

This is a pure signature/qualifier change with no new allocation, pointer
arithmetic, concurrency, or lifetime transfer — the underlying `shared_ptr`
representation, its refcounting, and its destruction order are all unchanged.
Consistent with ticket #1776's precedent for an analogously narrow,
non-memory-shape change ("a pure message-composition fix... a dedicated
sanitizer campaign was not run beyond the existing focused-suite coverage"),
a dedicated ASan/UBSan/TSan campaign beyond the design-phase probes in §2 is
not planned for the implementation ticket. The design-phase probes already
ran clean under ASan+UBSan (§2); the implementation ticket's focused test run
should include `-fsanitize=address,undefined` in its CI-equivalent build
configuration as it already does for every `Collections.Core` test binary.

## 16. Consumer-fixture plan

No standalone `test/consumer/*.cpp` fixture exists yet for
`System::Collections::ObjectModel::ReadOnlyDictionary` specifically (§5). The
implementation ticket should add
`test/consumer/collections_object_model_readonlydictionary.cpp`, compiled
`-Wall -Wextra -Wpedantic -Werror` against only `Collections.Core` +
`Core.Base`, that constructs a `ReadOnlyDictionary`, reads `Empty()`, confirms
`static_assert`s the `const&` return type, and exercises the ordinary
read-only accessors — mirroring the shape of the existing
`test/consumer/collections_dictionary_views.cpp` fixture from ticket #1775.
This is new coverage, not a modification of an existing fixture, so it carries
no compatibility risk of its own.

## 17. Migration guidance

No consumer migration document (unlike
`docs/Migration-ICollectionCopyTo.md`) is needed if the implementation
proceeds, because:

- The only source-incompatible pattern is declaring an explicit non-`const`
  reference to hold `Empty()`'s result (e.g.
  `ReadOnlyDictionary<K,V>& x = ReadOnlyDictionary<K,V>::Empty();`) or
  assigning through it. Confirmed absent everywhere in this repository (§5).
- `auto&`, `const auto&`, and `const ReadOnlyDictionary<K,V>&` callers — the
  only patterns found — continue to compile unchanged; `auto&` deduces to
  `const ReadOnlyDictionary<K,V>&` automatically once the return type changes.
- CNA and mobile-eggbert are out of this run's scope (per the run's absolute
  repository-scope rule) and were not inspected; if either declares an
  explicit non-`const` reference to `Empty()`'s result or assigns through it,
  that specific call site would need the same one-line adjustment
  (`auto&`/`const auto&` instead of an explicit non-`const` type) whenever
  they next upgrade to a sharp-runtime revision containing this change — the
  same category of "rebuild and adjust the few call sites the compiler flags"
  guidance as ticket #1771's migration document, just far narrower in scope.

## 18. Risks

- **Residual risk if only Alternative A ships:** an ordinary,
  non-singleton `ReadOnlyDictionary` instance remains assignable (Alternative
  C, §7, deliberately deferred). This is not a regression — it is today's
  existing, unaudited behavior for non-singleton instances, and no evidence in
  this repository shows it causes a defect. It is flagged here for
  transparency, not hidden.
- **Unknown downstream impact:** CNA/mobile-eggbert were not inspected (out of
  scope). If either explicitly declares a non-`const` reference to `Empty()`,
  this change would newly fail to compile for them on upgrade — the same
  category of risk ticket #1773 already tracks for the unrelated `CopyTo`
  break, and would be folded into that same kind of downstream-sweep ticket
  rather than blocking this fix, exactly as #1771's migration guidance
  handles it.
- **Doxygen:** the implementation ticket must re-measure the warning count
  before and after its diff (§ Doxygen baseline discrepancy, tracked
  separately in this run — see the planning reconciliation commit) and must
  not increase it.

## 19. Rejected approaches

See §7 for the full evaluation of Alternatives B and C, both rejected with
evidence; the "do nothing" non-alternative is rejected in the §7 preamble.

## 20. Proposed implementation ticket

**#1780** (`REMED-COLL-READONLYDICT-EMPTY`, P2, size XS) — implement §9-§16 of
this design: change `Empty()`'s return type to
`const ReadOnlyDictionary<K, V>&`, update its doc-comment, add the three new
regression cases in §14, add the consumer fixture in §16, and run the full
validation gate. **Marked `blocked`**: this changes a public method's return
type, which this repository's established convention (applied identically to
ticket #1770/#1771's `ICollection::CopyTo` removal) treats as requiring
explicit user approval before implementation, even though this design finds no
in-repository source break and no ABI break (the class is a header-only
template with no vtable and no exported linkage symbol). The exact approval
needed: explicit confirmation that changing
`ReadOnlyDictionary<K,V>::Empty()`'s return type from
`ReadOnlyDictionary<K,V>&` to `const ReadOnlyDictionary<K,V>&` may proceed as a
public API signature change. Depends on nothing else; SR-AUD-359 remains
`confirmed` until #1780 lands.
