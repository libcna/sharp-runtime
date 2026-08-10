# Audit: `modules/collections/include/System/Collections/Hashtable.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Collections.Core`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_Collections_Core && build/SharpRuntimeTests_Collections_Core --gtest_color=no` passed 1,422/1,422 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-363 — medium — Hashtable accepts null keys and returns null instead of the public Keys/Values collections

The raw-key path stringifies a null pointer as `"0"`, so `Add(nullptr, …)` succeeds rather than reporting the required null-key argument failure.  The same type advertises `IDictionary.Keys` and `Values` but returns `nullptr` for both.  The direct probe prints `null-key=accepted count=1` and `keys-null=1 values-null=1`; callers cannot safely consume the promised views.

## Missing assertions and diagnostics

- Hashtable tests omit null-key rejection and never dereference/use Keys or Values through the IDictionary contract.
- Add boundary diagnostics for null keys and a lifetime-safe view implementation or an explicit unavailable-feature result.

## Post-audit remediation (ticket #1775, 2026-07-27): REMEDIATED

The audit evidence above is retained unchanged. Ticket #1775
(`REMED-COLL-HASHTABLE-VIEWS`, P1, size M) closed both halves of the finding on
branch `feature/remediation-coll-hashtable-views`.

Two facts beyond the original probe were established before the repair:

- The null view is not merely an absent feature. A consumer that follows the
  `IDictionary` documentation and uses the promised view without a null check
  is an ASan-confirmed SEGV plus a UBSan `member access within null pointer of
  type 'struct ICollection'`, while the sibling `ListDictionaryInternal`
  answers the *identical* caller code correctly. That makes this an interface
  defect with divergent implementations, not a Hashtable-local omission.
- The stringified null key `"0"` aliases the ordinary string key `"0"` accepted
  by the `Add(const std::string&, const std::any&)` overload: after
  `Add(nullptr, v)`, `ContainsKey("0")` is true and `Add("0", …)` is rejected as
  a duplicate of an entry the caller never added.
- A third null-key entry point was found: `Remove(const char*)` forwarded a
  null argument into `std::string`'s null construction and terminated with a
  `std::logic_error` that code catching `System::Exception&` cannot see.

Repair: `getKeysProperty()`/`getValuesProperty()` return a live, caller-owned
`MemberCollection` whose `Count`, `SyncRoot`, `IsSynchronized`,
`GetEnumerator`, and `copyToCore` delegate to the owning table, following the
`ListDictionaryInternal::MemberCollection` precedent already in this component
and matching .NET's `KeyCollection`/`ValueCollection`. The views reuse the
ticket #1771/#1774 copy boundary unchanged. `toKey()` became the single
validating conversion site through which every raw-key path passes, so
`getItem`, `setItem`, `Contains`, `Add`, and `Remove` reject a null key with
`System::ArgumentNullException("key")`, as .NET's `Insert`/`ContainsKey`/
`Remove`/indexer do; `Remove(const char*)` gets the same check. No non-null
address stringifies to `"0"`, so the key-space alias is structurally
unreachable. No public signature changed and no virtual member was added or
removed, so this is neither a source nor an ABI break.

Closure evidence: 70 permanent regressions in
`DictionaryKeyAndViewContractTests.cpp`, whose view cases are parameterised
over *both* non-generic `IDictionary` implementations; the same 70 under
ASan + UBSan + LeakSanitizer with no diagnostic and no leak; a 33-assertion
replacement probe (`build-probe-hashtable/probe2_fixed_boundary.cpp`,
`failures=0`) covering the previously fatal scenarios plus liveness,
non-trivial values, a 20,000-entry table, and destruction order;
`SharpRuntimeTests_Collections_Core` 1,732/1,732; a `-Werror` standalone
`Collections.Core` consumer fixture
(`test/consumer/collections_dictionary_views.cpp`) that compiles and runs; and
a network-permitted `scripts/local_ci_check.sh build` of 12,991/12,991 tests
across 37 executables with zero warnings/errors.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.

## Post-remediation follow-up: ticket #1787 (2026-07-28)

Ticket #1787 (`REMED-COLL-VERSION-COUNTER-OVERFLOW-SWEEP`, P3, size M) repaired
the mutation counter this file's fail-fast enumerator snapshots. It carries **no
`SR-AUD-*` identifier** — the numbering is frozen at 364 and the pattern was
found during remediation, by ticket #1786's own inventory — and it reopens no
finding here. The original evidence above is retained unchanged.

The counter is now `System::Collections::detail::MutationCounter` (64-bit unsigned) and
the enumerator snapshots `detail::MutationVersion`. `sizeof`, `alignof`, and the
counter's own byte offset are all unchanged — measured, not assumed — because the
widening consumed padding the type already had at that position.

Repository-wide, three defect classes were reproduced against the committed
pre-fix headers before anything changed
(gitignored `build-probe-collversion/probe2_defects.cpp`): fourteen UBSan
signed-integer-overflow reports at `++version_`, fifteen 2^32
snapshot-reuse (ABA) reproductions, and — recorded in neither #1786's nor
#1787's description — an assignment defect that transplanted the *source's*
counter into the destination and needed no overflow at all, with six
AddressSanitizer `heap-use-after-free`/`heap-buffer-overflow` reproductions. The
full record, including the .NET comparison and the per-type layout measurements,
is `docs/CollectionVersionCounterSweep.md`.

## Post-audit note — design ticket #1795 (2026-07-28)

**No `SR-AUD-*` identifier**: the numbering is frozen at 364 and this was found
during remediation. Design ticket #1795 measured `Hashtable::Enumerator`'s two
`const void*` accessors and found a **write path into live dictionary storage**
that ticket #1794's own description asserted did not exist:

- `getValueProperty()` returns `&it_->second`, a pointer to the live
  `std::unordered_map`'s `mapped_type`, which is a **non-`const` `std::any`**.
  `const_cast` + assignment through it is not undefined behaviour — it is
  well-formed, defined C++ that rewrites the stored value, leaves the mutation
  counter unmoved, and is invisible to a second, independent enumerator. The
  rewritten value is then returned by `Hashtable::at()`.
- `getKeyProperty()` returns `&it_->first`, a `const std::string` inside the map.
  The write there *is* undefined behaviour, and at 64 entries it produces an
  entry that `getCountProperty()` still counts and that **no lookup can return
  by either its old or its new key**.
- Neither accessor performs the fail-fast version check `MoveNext()` and
  `Reset()` perform, so calling either again after a `Remove` or `Clear`
  dereferences an invalidated iterator — AddressSanitizer `heap-use-after-free`.
  `getEntryProperty()` and `getCurrentProperty()` are unaffected because they
  read the enumerator's own `current_` cache; that asymmetry is itself the
  design's evidence for making the cache mandatory.

Two **pre-existing, separate** write escapes on this class are recorded but were
**not** in scope: `operator[](const std::string&)` returns a non-`const`
`std::any&` and `getItem()` returns `const_cast<std::any*>(&it->second)`; both
bypass the mutation counter and both are already documented in the header as
narrow gaps. Implementation is ticket #1794, `blocked`. Full record:
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md`.


### Remediated by ticket #1794 (2026-07-28)

`Enumerator::getKeyProperty()` and `getValueProperty()` now answer from the
existing `current_` snapshot and return an owning `std::any` by value; the two
`&it_->first`/`&it_->second` dereferences were the last container reads inside an
accessor on this class and are gone. `MemberEnumerator::getCurrentProperty()`
loses its `static_cast` pair and forwards the boxed accessors directly; the key
view still boxes `std::string` and the value view still boxes the stored value's
own payload, never a nested `std::any`. `sizeof(Hashtable)` and
`sizeof(Hashtable::Enumerator)` are unchanged at 72.

The write paths recorded above were reconfirmed against the pre-fix headers
before anything changed (`defects=20`, including the 64-entry key corruption) and
are now inexpressible: `const_cast` cannot convert a `std::any` to a pointer.

**The two pre-existing write escapes on this class remain open and were
deliberately out of scope**, but no longer live only in a design risk register:
`operator[](const std::string&)` and `getItem()`'s
`const_cast<std::any*>(&it->second)` are now carried by inactive ticket **#1796**
(`REMED-COLL-HASHTABLE-WRITE-ESCAPES`, P3, `blocked`), which needs its own design
and its own approval before it may begin. Full record:
`docs/IDictionaryEnumeratorKeyValueSafetyDesign.md` §37.

## Post-audit note — design ticket #1797 (2026-07-28)

**No `SR-AUD-*` identifier**: the numbering is frozen at 364 and this was found
during remediation. Design ticket **#1797**
(`REMED-COLL-HASHTABLE-VALUE-ACCESS-DESIGN`, P3, size M, `design`, **done**)
completed the design for ticket #1796. **#1796 stays `blocked`.** No production
or test source changed. Durable record:
`docs/HashtableValueAccessSafetyDesign.md`.

**The two escapes #1796 names are real, and they are not the whole inventory.**
The design found **four** mutable or aliasing routes on this file:

| Route | Escapes | Bumps? | Dangles? | Named by #1796? |
|---|---|---|---|---|
| `void* getItem(const void*) const` (:119) | value storage, writable, type-erased | never | yes | yes |
| `std::any& operator[](const std::string&)` (:285) | value storage, writable, typed | never, **and inserts on a bare read** | yes | yes |
| `const std::any& at(const std::string&) const` (:291) | value storage, `const` alias | never | yes | **no** |
| `setItem`/`Add` raw-key `void*` **value** (:135, :220) | reads caller storage through a non-`const` `void*` | always | n/a | **no** |

Rows 6–12 of the design's §4 inventory — `getKeys`, `getValues`, `CopyTo`,
`copyToCore`, both member views, and every `Enumerator`/`MemberEnumerator`
accessor — are **already safe**, the last of those having been made so by
tickets #1793 and #1794.

Sixteen defects were reproduced against the **committed** headers before any
change (`build-probe/1797_probe1_escapes.log`), and **all sixteen are silent
under UndefinedBehaviorSanitizer alone**. Three findings matter beyond the
ticket's own description:

- **`at()` is a third write escape.** The referent is a non-`const` `std::any`
  inside a non-`const` `Hashtable`, so `const_cast<std::any&>(h.at("k")) = v` is
  **not** undefined behaviour — it is well-formed, fully defined C++ that rewrote
  live dictionary storage with `version_` unmoved. It is the same mechanism
  design #1795 found on the pre-#1794 enumerator accessor, on a member nobody had
  looked at. A `const Hashtable&` can therefore rewrite every value in the table
  through two `const` members.
- **Rehash does not dangle; erasure and assignment do.** `std::unordered_map` is
  node-based, and the address of a stored value was **unchanged across 8,000
  insertions**. The measured hazard is `Remove`, `Clear`, copy assignment, move
  assignment and destruction — **nine AddressSanitizer `heap-use-after-free`
  reports across fourteen scenarios**, 0 LSan leaks with detection proved active
  by a 317-byte self-test. Copy and move assignment are the non-obvious two:
  #1787 correctly advances the *destination's* counter, but a retained
  `std::any&` is not an enumerator and nothing checks it.
- **The worst case produces no diagnostic at all.** `operator[]` on an absent key
  inserts structurally without bumping, so an outstanding enumerator's version
  check passes and it walks a rehashed bucket array. At 4,008 entries it visited
  **2,045 distinct keys**, reached **6 of its 8** pre-mutation seed keys, threw
  nothing, and ASan, UBSan and LSan all reported nothing. Memory-safe and wrong.

**Correctly-behaving controls, recorded so the taxonomy is not overstated:**
`setItem` bumps on insert, replace **and equal replace**, which is exactly what
.NET's `Hashtable.Insert` does; both raw-key paths reject a null key; mutating
the object a stored pointer refers to correctly does *not* bump; and every value
path stores exactly what it is given, never flattening a nested `std::any`.

Selected: `getItem` → `std::any` by value; `operator[]` → a **non-copyable**
`ValueReference` proxy whose read conversion returns `std::any` **by value**,
plus a new `const` by-value overload; `at()` → by value throwing
`KeyNotFoundException`. `setItem`/`Add`'s raw-key `void*` value parameters stay
**deliberately**: migrating them to `const std::any&` makes `Add("literal", v)`
store the entry under the stringified *address* of the literal, with no
diagnostic under `-Werror`.

`sizeof(Hashtable)` is **unchanged at 72** under every candidate — this is not an
object-layout break. The virtual `getItem` change is a **silent ABI break**:
byte-identical mangled name, vtable slot unchanged at `0x38`, `this` moving
`%rdi → %rsi` behind a hidden `sret`, reproduced as a stale caller that links
with zero diagnostics and then SEGVs.

Implementation is **#1796**, `blocked` on the four-item approval in design §32.
`ListDictionaryInternal`'s own two defects, found while establishing whether the
two `IDictionary` implementations agree, are **new inactive ticket #1798**.

---

## Remediated by ticket #1796 (2026-07-28)

**All four escape routes are closed.** `REMED-COLL-HASHTABLE-WRITE-ESCAPES`
implemented design ticket #1797 exactly, on the user's explicit four-item
approval (source break, silent semantic change, silent ABI break, changed
exception type). Durable record: `docs/HashtableValueAccessSafetyDesign.md`
§34–§37.

| Route | Was | Now |
|---|---|---|
| `getItem(const void*) const` | `void*` into live storage | **`std::any` by value** |
| `operator[](const std::string&)` | `std::any&`, inserted on a bare read | **`ValueReference` proxy** — tracked write, owning read, no insert on read |
| `operator[](const std::string&) const` | did not exist | **`std::any` by value** |
| `at(const std::string&) const` | `const std::any&`, `std::out_of_range` | **`std::any` by value, `KeyNotFoundException`** |
| `setItem(const std::string&, const std::any&)` | did not exist | **new typed tracked setter** |
| `setItem`/`Add` raw-key `void*` *value* parameter | — | **unchanged, deliberately** (design §13.4) |

Measured before and after on the committed headers, one session apart:

| Measurement | Pre-fix | Post-fix |
|---|---|---|
| Defects reproduced by `1797_probe1_escapes` | **16** | route removed — the probe no longer compiles |
| UBSan runtime errors over those 16 | **0** (all silent) | n/a |
| ASan `heap-use-after-free` / 14 lifetime scenarios | **9** | **0** |
| LSan leaks (detection proved active) | 0, self-test 317 B / 2 allocs | **0**, self-test 318 B / 2 allocs |
| Enumeration integrity — 8 seeds, one outstanding enumerator, then **4,000 missing-key reads through `operator[]`** (the identical experiment, rerun) | Count **8 → 4,008**, walked **2,045** distinct, **6/8** seeds, threw 0, no sanitizer report | Count **8 → 8**, walked **8** distinct (0 duplicates), **8/8** seeds, threw 0 |
| The same, at 4,008 pre-seeded entries and 64 missing-key reads | — | Count **4,008 → 4,008**, walked **4,008** distinct, **8/8** seeds |
| `sizeof(Hashtable)` / `alignof` | 72 / 8 | **72 / 8 — unchanged** |
| `sizeof(ListDictionaryInternal)` / `alignof` | 40 / 8 | **40 / 8 — unchanged** |
| `Hashtable::ValueReference` | did not exist | **40 / 8**, never stored by the collection |

**The ABI break is real and was reproduced end to end against the real
production declarations, not a shim.** The caller symbol
`_Z11callGetItemRN6System11Collections11IDictionaryEPKv` is **byte-identical**
before and after; the vtable slot is **unchanged at `*0x38(%rax)`**; no symbol is
added or removed. The calling convention is not the same — pre-fix `this` in
`%rdi` with the result in `%rax` and a tail-call `jmp`; post-fix `%rdi` is the
hidden `sret` pointer, `this` moves to `%rsi`, and a real `call` is emitted. A
caller object compiled against the old header **links against the new
implementation with zero diagnostics (`exit=0`) and then segfaults
(`exit=139`)**, with 14 UBSan diagnostics first, beginning `member access within
misaligned address … for type 'const struct Hashtable'`. `callSetItem`, the
unchanged control, is byte-identical machine code at slot `*0x40(%rax)`.
**Every consumer must be fully rebuilt**, and the linker cannot enforce it.

Permanent coverage:
`modules/collections/tests/System/Collections/HashtableValueAccessSafetyTests.cpp`,
**55 tests**, parameterised over both `IDictionary` implementations wherever the
assertion is about the interface; the whole file passes under ASan + UBSan + LSan
with zero findings. Consumer fixtures
`test/consumer/collections_hashtable_value_access.cpp` (compiled and **run**
against `Collections.Core` alone under `-Wall -Wextra -Wpedantic -Werror`) and
`..._negative.cpp` (**11 of 11** marked alias spellings rejected, verified by
`build-probe/1796_check_negative.py` rather than by the file merely failing to
compile).

**Still open, and not claimed closed by this ticket:**

- `setItem`/`Add`'s raw-key `void*` *value* parameter remains type-erased
  (design §13.4, with the `Add("literal", v)` address-key corruption that is the
  reason it was not "tidied up").
- Using any accessor after the *collection* itself is destroyed remains
  undefined; that is the port-wide borrow convention, unchanged.
- `ListDictionaryInternal`'s own two defects — `setItem` skipping the version
  bump on the replace branch, and both accessors accepting a null key — are
  **untouched** and remain ticket **#1798**. Its `getItem` was migrated
  mechanically (the same caller pointer, now boxed) and nothing else about it
  changed. `HashtableValueAccessSafetyTests.cpp` pins the null-key divergence as
  a deliberate recorded state so #1798 has a test to flip.

---

## Follow-up remediation by ticket #1802 (2026-07-29) — `Remove`'s absent-key over-bump

**No `SR-AUD-*` identifier**: the audit numbering is frozen at 364 and this was
found during remediation, by design ticket #1799's probe rather than by the
audit. **SR-AUD-363 is not reopened** and the original evidence above is retained
unchanged. Durable record: `docs/HashtableValueAccessSafetyDesign.md` §35.

All three `Remove` overloads were `_map.erase(key); ++version_;`, so the fail-fast
mutation counter advanced **whether or not the key was present**. Reproduced
against the committed headers before anything was edited
(`build-probe/1802_probe1_remove.cpp`, log `build-probe/1802_prefix.log`): **24
defects over 43 checks**. Removing an absent key moved the counter `3 → 4`, and
the `InvalidOperationException` that followed came out of **every** outstanding
enumerator kind — the `IDictionaryEnumerator`, the key view, the value view, and
the same reached through an `IDictionary&`. A full walk after one absent `Remove`
yielded **0 of 3** entries and threw; `Reset()` threw too. At 20,000 entries the
counter moved and the enumerator died after a `Remove` that removed nothing.

**The consequence is a false positive, and it is the opposite of #1798's defect.**
`Count` and contents were correct on every measured row; nothing was corrupted
and no memory was misused. The counter simply claimed a mutation that never
happened. #1798's `ListDictionaryInternal` defect was the memory-unsafe
direction — a real mutation the counter missed.

.NET `Hashtable.Remove` calls `UpdateVersion()` at `Hashtable.cs:999`, **inside**
the branch that matched a bucket; the absent case falls out of the collision walk
having touched neither `_count` nor `_version`. .NET `ListDictionaryInternal`
does `version++` first and unconditionally, so .NET's own two implementations
disagree here — which is why `docs/ListDictionaryInternalSetterDesign.md` §9.3
had to *choose* a rule (the `Hashtable` one, "advance on effective mutation")
rather than "match .NET". This ticket is what makes the port's `Hashtable`
actually follow the rule the port had already written down in
`detail/MutationCounter.hpp`.

**Repair:** all three overloads route through one new private helper,
`removeKey(const std::string&)`, which is `if (_map.erase(key) != 0) ++version_;`.
`std::unordered_map::erase(const key_type&)` already returns the number of
elements removed, so the effective/no-op distinction costs **no second lookup, no
`Contains` pre-check, no second key conversion, no allocation and no lock** — the
deciding value was already being computed and discarded. The bump is *after* the
erase, so a throwing key conversion leaves both contents and counter untouched.
`toKey()` is unchanged and remains the single validating conversion site; the
null-key contract established by #1775 and re-asserted by #1796 is untouched and
is re-pinned, message text included.

**`Clear()` is a decided, documented deviation and was deliberately NOT changed:**
it still bumps unconditionally, including on an already-empty table, where .NET
`Hashtable.Clear` early-returns at `Hashtable.cs:426`. `_occupancy` has no
`std::unordered_map` analogue, so `if (_map.empty()) return;` would *not*
reproduce .NET's rule; the unconditional bump errs in the safe direction; and it
matches .NET `ListDictionaryInternal.Clear` and the port's own sibling. It is now
asserted in both the permanent suite and the consumer fixture rather than left as
a comment.

**Closure evidence:** the reproduction probe re-run against the repaired headers,
**0 defects over the same 43 checks**; a new permanent suite
`HashtableRemoveVersioningTests.cpp` (**+67 tests**), whose enumerator matrix is
parameterised over four enumerator families and whose interface cases are
parameterised over **both** non-generic `IDictionary` implementations;
`CollectionVersionCounterTests.cpp`'s `HashtableAdapter` gains
`kHasNoOpMutation = true` with an absent-key `Remove` as its no-op mutation,
matching `ListDictionaryAdapter`; the whole `Collections.Core` suite green under
ASan + UBSan + LeakSanitizer with **0** findings and LSan **proved active** by a
350-byte self-test reported as 383 bytes in 2 allocations; a `-Werror`
`Collections.Core` consumer fixture
(`test/consumer/collections_hashtable_remove.cpp`) that compiles **and runs**,
also clean under the sanitizers; and measured ABI evidence —
`sizeof(Hashtable)` **unchanged at 72**, the **19-entry vtable byte-identical**
with `Remove` still at slot `0x70`, `this` still in `%rdi` with no `sret`, the
undefined-symbol list identical, and `callClear`/`callAdd`/`callSetItem`
byte-identical machine code. Allocation counts are identical on every `Remove`
path.

**A full consumer rebuild is mandatory and silent if skipped** — every affected
body is `inline` in a header, so a stale object links with zero diagnostics and
keeps the old false positive, link-order dependently at `-O0` and per-TU at
`-O2`, with `-flto -Wodr` diagnosing nothing. It is **not** an ABI break, unlike
#1794's and #1796's; the failure mode is silence, never a crash.

**Not claimed closed:** everything in §34.8 of the design record, including the
raw-key `void*` *value* parameter, accessor use after the collection is
destroyed, ticket #1800's pre-existing seam divergence (this ticket adds a fourth
specialisation spelled **token-for-token** as the existing `SR1794_SEAM_BODY`
ones, so it introduces no new divergent body), and ticket #1801's untracked
negative fixtures. CNA and mobile-eggbert were not inspected.
