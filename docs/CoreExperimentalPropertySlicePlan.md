<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `SharpRuntime::Experimental::Property` slice — plan

Ticket #2243. Two frozen audit findings in
`modules/core/include/SharpRuntime/Experimental/Property.hpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-179 | medium | Property assignment writes through the setter but returns a disconnected stale cache value |
| SR-AUD-181 | low | Advertised `DEF_PROP_AUTO` and custom property macros name a nonexistent `Property` type |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **slice of one header**, not a
`SharpRuntime::Experimental` review and not a `modules/core` review.

---

## 1. Exact scope, and what is deliberately left out

In scope: the `operator=` of `SharpRuntime::Experimental::Property<T>`, the four
`DEF_PROP_*`/`IMPL_PROP_*` macro pairs the same header defines, and the
`ExperimentalPropertyTests` block in `tests/integration/Task39RemainingTests.cpp`.

`modules/core/include/SharpRuntime/Experimental/ReadonlyProperty.hpp` is read and
**pinned**, not modified. Its own audit report carries no finding and records that
`ReadOnlyProperty<T>` "deliberately inherits the base's callback and
stale-assignment implementation", so SR-AUD-179 "remains relevant if callers cast
to the base". That base route is measured here (§3 case 6, §8) and is a test
target of this slice; the derived header's own text needs no change.

Out of scope, by decision rather than omission:

- **`SharpRuntime/Prop.hpp`**, the macro family this header's own note says is
  preferred in production. It carries no finding and is a different design.
- **The two remaining "other missing assertions" bullets** of the audit report
  that are neither of the findings: copy/move of a wrapper whose callbacks
  capture an owner, and generic non-default-constructible `T`. The second is
  answered rather than tested — see §5, it is a consequence of the member this
  slice deliberately retains.
- **`modules/core`'s other 50 open findings.** Nothing in this slice touches them.

## 2. Are these one family? — a slice, not a cause family

**They are not a cause family.** They share the file, the header's
"experimental, deliberately unused" status, and the compatibility class. They do
not share a root cause, and neither repair is a step towards the other:

- **SR-AUD-179 is a value-semantics defect in the class.** A private data member
  that models nothing is handed to the caller as the result of a public
  expression. No macro is involved; the defect reproduces through plain
  `Property<T>` with no macro in the translation unit.
- **SR-AUD-181 is a name defect in the preprocessor.** Four macros spell a
  namespace that does not exist. No object, no value and no runtime behaviour is
  involved; the failure is a translation failure. It reproduces with the class
  entirely correct.

The inherited ranking that selected this pair described it as "two ordinary
implementation bugs, one file, no visible approval boundary". The first half is
confirmed. The second half is **corrected**: one approval boundary is present and
was not visible from the finding text — removing the defective member changes
`sizeof(Property<T>)` (§5). It is separated out rather than crossed.

So: one bounded review (#2243), two independent implementation tickets (#2244,
#2245), one approval request (#2246) and one post-audit adjacency (#2247).

## 3. Before evidence, measured 2026-08-10

`build-probe/2243_probe1_before.cpp`, compiled against the shipped
`build/libsharp_runtime_core.a` (`build-probe/2243_probe1_before.log`):

```
case1 stored=new assignment_result= getter_after=new
case2 stored=42 assignment_result=-455742320
case3 stored=first getter=first ref=written-through-the-returned-reference
case4 stored=0 assignment_result=-1786670400 getter_after=0
case5 sizeof(Property<int>)=72 sizeof(Property<std::string>)=96 sizeof(ReadOnlyProperty<int>)=72 two_functions=64
case6 threw: Setter not implemented.
case7 a=-455742320 b=9
```

Case 1 reproduces the finding's own printed evidence, `stored=new
assignment_result=`, verbatim. The controls hold: the setter did run
(`stored=new`) and the getter does see it (`getter_after=new`), so only the
expression's own result is wrong.

**Three premise sharpenings, each measured rather than inferred:**

1. **For a scalar `T` the returned value is indeterminate, not merely stale.**
   The finding says the member "is merely default-initialized". `T cachedValue;`
   is *default*-initialised, which for `int` performs no initialisation at all, so
   `int x = (p = 42);` reads an indeterminate value. Two runs of the same binary
   printed `535613888` and `-455742320` for case 2. Valgrind memcheck reported
   nothing, which is expected — the storage is an automatic object inside a
   caller-provided frame — so the run-to-run variation is the evidence, not the
   sanitizer.
2. **The returned `T&` also silently swallows writes.** Case 3 assigns through
   the returned reference and the property is unchanged: `getter=first`. The
   finding describes a bad *read*; the lvalue is bad in both directions.
3. **A synchronised cache would not have been a repair.** Case 4 uses a setter
   that clamps `-5` to `0`. A cache holding the *assigned* value would report
   `-5` for a property that holds `0`. This is what selects "read back through the
   getter" over "keep the cache and write to it" in §4.1.

Case 7 adds a consequence the finding does not reach: chained assignment
`pa = pb = 9` propagated the indeterminate cache into `pa`, so `a` was garbage
while `b` was 9.

Case 5 is the layout measurement §5 turns on. Case 6 is the pinned control:
assignment through a `Property<T>&` bound to a `ReadOnlyProperty<T>` throws
`System::NotSupportedException`, and must still throw afterwards.

For SR-AUD-181, `build-probe/2243_probe2_macros.cpp` is a minimal class using all
four documented pairs. As shipped it does not compile
(`build-probe/2243_probe2_macros.log`):

```
Property.hpp:19:27: error: 'Property' in namespace 'SharpRuntime' does not name a template type
   19 |     public: SharpRuntime::Property<type> name;
```

Recompiling the same source with **only** the two `DEF_PROP_*` macros re-spelled
(`-DPROBE_FIXED_NAMESPACE`) compiles and runs correctly, which establishes that
the namespace is the *whole* defect — no second problem hides behind the first.
The finding's "followed by a missing constructor field error" is a *consequence*
of the type name failing, not an independent second cause.

## 4. The two members, individually

### 4.1 SR-AUD-179 — the assignment expression reports what the property holds (#2244)

`T& operator=(const T&)` becomes `T operator=(const T&)`, returning `getter()`
after the setter has run. Three properties of that choice, in order of how much
they were argued:

- **By value, not by reference.** The wrapper owns no storage that a reference
  could refer to; the value lives in whatever the supplied callables close over.
  Any reference this function returns is either a reference to a member that
  models nothing (the defect) or a dangling reference to a temporary. `T` by
  value is the same answer `std::atomic<T>::operator=` gives, and for the same
  reason.
- **`getter()`, not `value`.** Case 4 above. A setter may transform, clamp or
  ignore its argument, and the expression must report the property, not the
  request.
- **Not `Property&`.** The other spelling the finding sanctions ("should return
  the property or a value that reflects the operation") is idiomatic C++
  assignment, but it breaks chained assignment for this abstraction: `pa = pb = 9`
  would resolve the outer assignment to the implicitly declared *copy* assignment
  and give `pa` a copy of `pb`'s callbacks, silently aliasing two properties onto
  one backing store. Returning `T` keeps case 7 meaning what a caller expects.

Ordering is pinned by test: the setter runs first, the read-back happens only on
success, and a read-only property throws before the getter is reached.

### 4.2 SR-AUD-181 — the macros name the type that exists (#2245)

`DEF_PROP_AUTO` and `DEF_PROP_CUSTOM` expand to
`SharpRuntime::Experimental::Property<type>`. The two `IMPL_PROP_*_READONLY`
macros and `IMPL_PROP_CUSTOM` never name the type and are unchanged.

The durable half of this repair is not the edit, it is that the macros are now
**compiled by the test build**: `MacroWidget` in
`tests/integration/Task39RemainingTests.cpp` instantiates all four pairs. A macro
nothing compiles is a macro that silently rots, which is exactly how this finding
came to exist.

## 5. The approval boundary this slice found and did not cross (#2246)

`cachedValue` is, after #2244, read by nothing and written by nothing. Deleting it
is the obvious completion — and it changes the object:
`sizeof(Property<int>)` would fall 72 → 64 and `sizeof(Property<std::string>)`
96 → 64 on the measured toolchain (case 5). This repository treats an object-layout
change as requiring explicit per-action user approval — tickets #1788 and #1789
each obtained it for a single `sizeof` change in a header-only collection, and
several currently blocked tickets (#1889, #2199) are blocked on exactly this.
`Property.hpp` is on the public include surface; `docs/ComponentCatalog.md` names
it as `Core.Base`'s representative public header.

So the member is **retained, documented and pinned**:

- its declaration carries `[[maybe_unused]]` and a comment saying it is vestigial,
  why it is still there, and which ticket carries the request;
- the class doc-comment states the consequence a user can actually hit — `T` must
  be default-constructible, for no reason other than this member. That is the
  audit report's own alternative for this case: "Document or remove the
  default-constructible `cachedValue` requirement **if the cache is retained**";
- `ExperimentalPropertyTests.Layout_CachedValueIsStillPartOfTheObject` pins the
  invariant (the object is strictly larger than its two callables) rather than a
  toolchain-specific number, so the test that must change when #2246 is approved
  is identified in advance.

`[[maybe_unused]]` is not decoration. Clang enables `-Wunused-private-field` under
`-Wall`, and the Emscripten toolchain this repository also builds is Clang-based;
an undecorated dead member would be a warning there under rule 1's zero-warning
requirement, on a platform this session cannot run.

## 6. CCF relationships — none minted, none extended

- **CCF-011** (empty `std::function` values cross public boundaries without an
  explicit policy) is **CLOSED**, with six named members, none in this header.
  `Property<T>`'s constructor does accept an empty getter and defers to
  `std::bad_function_call` at the first read, which is the same *shape*. It is
  recorded as an **adjacency and a post-audit defect (#2247)**, not as a member and
  not as an occurrence: no frozen finding names it, CCF-011 is closed, and reopening
  a closed family for a type it never listed would misreport what that family
  measured. This follows the standard the immediately preceding batches set —
  SR-AUD-064 was adjacency-only to CCF-011 despite sharing a constructor body, and
  SR-AUD-105's undefined-`SpecialFolder` fall-through was recorded as an adjacency
  rather than absorbed.
- **CCF-019** (borrowed views with no liveness bound) is unresolved and untouched.
  The escaping `T&` of §3 case 3 is *not* a CCF-019 instance: it did not point into
  a container the caller could outlive, it pointed at a member of the property
  itself, whose lifetime is the property's. The defect was that it modelled
  nothing, not that it dangled.
- **CCF-021 / #2131** and **CCF-022 / #2109** remain unminted and are untouched.

No family is minted, extended, closed or renumbered by this slice.

## 7. Compatibility, ABI, layout and `noexcept`

| Property | Before | After |
|---|---|---|
| `sizeof(Property<int>)` | 72 | 72 |
| `sizeof(Property<std::string>)` | 96 | 96 |
| `sizeof(ReadOnlyProperty<int>)` | 72 | 72 |
| Data members, order, types | 2 callables + `T cachedValue` | unchanged |
| Virtual functions, vtable | none | none |
| `noexcept` on any member | none | none |
| Exported symbols | none — a class template, wholly header-inline | unchanged |

**One source-compatible change and one source-incompatible change**, both
deliberate and both stated rather than discovered later:

- `operator=`'s return type changes from `T&` to `T`. Every use that consumes the
  result as a value — `T x = (p = v);`, `std::cout << (p = v)`, `(p = v)` as a
  statement, `p1 = p2 = v` — compiles unchanged and is now *correct* where it was
  previously wrong. **`T& r = (p = v);` no longer compiles**, which is the defect
  itself: that was the spelling that handed out the disconnected lvalue. The
  rejection is verified (`build-probe/2243_probe3_after.log`,
  `-DPROBE_REFERENCE_BINDING`). There is no first-party use of `operator=` on this
  type anywhere in the repository, so no first-party call site changed meaning.
- The macros go from *not compiling at all* to compiling. Nothing that previously
  built can break.

`ReadOnlyProperty<T>` is unaffected: its own `T& operator=(const T&) = delete;`
hides the base's `operator=` **by name**, not by signature, so `ro = v` is still
rejected at compile time and the base-reference route still throws.

## 8. Test matrix

Twelve permanent add-only regressions in
`tests/integration/Task39RemainingTests.cpp`, all in the existing
`SharpRuntimeIntegrationTests` executable.

| # | Test | Pins |
|---|---|---|
| 1 | `Assignment_ExpressionYieldsTheValueTheGetterReadsBack` | §3 case 1, the finding's own reproduction |
| 2 | `Assignment_ExpressionYieldsTheValueForAScalarType` | §3 case 2, the indeterminate-read route |
| 3 | `Assignment_TransformingSetter_ExpressionReportsTheStoredValue` | §3 case 4, why `getter()` and not `value` |
| 4 | `Assignment_Chained_SetsBothProperties` | §3 case 7, and why not `Property&` |
| 5 | `Assignment_InvokesTheSetterThenTheGetterExactlyOnce` | call count and order |
| 6 | `Assignment_ReadOnly_ThrowsBeforeInvokingTheGetter` | the read-back is not reached on failure |
| 7 | `Assignment_ReadOnlyPropertyThroughBaseReference_Throws` | §3 case 6, the `ReadonlyProperty` report's base route |
| 8 | `Layout_CachedValueIsStillPartOfTheObject` | §5, the pin #2246 must update |
| 9–12 | `ExperimentalPropertyMacroTests.*` (auto, auto-readonly, custom, custom-readonly) | all four macro pairs, compiled |

The four pre-existing `ExperimentalPropertyTests` are unchanged and still pass;
nothing was retired.

## 9. Sanitizer matrix

**Not run, deliberately.** Neither finding is a memory-safety or
undefined-behaviour class a sanitizer discriminates. SR-AUD-181 is a translation
failure. SR-AUD-179's scalar route *is* an indeterminate read, but it lives in a
caller-provided automatic object, which is precisely the case ASan does not model
and MSan cannot see without instrumenting libstdc++'s `std::function`; the
before-probe under valgrind memcheck was silent while the defect was plainly
present, and the run-to-run value variation is the stronger evidence. Running a
sanitizer here would produce a clean report that says nothing about either
finding.

## 10. Slice completion criteria

1. Both findings `remediated` in `audit/AUDIT_FINDINGS_INDEX.md`, with the premise
   sharpenings recorded in the row.
2. Zero errors and zero warnings from `cmake --build build --parallel 2`.
3. No test-count regression; the twelve new tests accounted for exactly.
4. `sizeof` unchanged for all three instantiations.
5. Every deliberately-not-done item carries a ticket: #2246 (approval), #2247
   (post-audit adjacency).
## 11. Outcome, measured 2026-08-10

Both findings remediated. `build-probe/2243_probe3_after.log`, same cases:

```
case1 stored=new assignment_result=new getter_after=new
case2 stored=42 assignment_result=42
case3 (T& bound to the assignment expression) no longer compiles
case4 stored=0 assignment_result=0 getter_after=0
case5 sizeof(Property<int>)=72 sizeof(Property<std::string>)=96 sizeof(ReadOnlyProperty<int>)=72 two_functions=64
case6 threw: Setter not implemented.
case7 a=9 b=9
case8 result=5 getterCalls=1 setterCalls=1
case9 getterCalls=0
```

Case 5 is identical before and after — the layout did not move. Case 6 is
identical — the read-only contract did not move. The macro probe compiles and
runs against the repaired header with `-Wall -Wextra -Wpedantic` and no
`-D` override.

### 11.1 Mutation checks

Both mutations were applied to the production header, rebuilt with
`cmake --build build --parallel 2`, and re-run (`build-probe/2243_mutations.log`).

- **M1** — `operator=` returns `cachedValue` again (return type left as `T`, so
  only the returned *object* is mutated). **6 tests fail**: matrix rows 1, 2, 3,
  4, 5 and the auto-macro row, which exercises the assignment expression through
  the macro-generated property. Rows 6–8 correctly survive: they pin behaviour M1
  does not touch.
- **M2** — the macros name `SharpRuntime::Property` again. **The build fails with
  24 errors.** This is the durable half of #2245: the fixture is compiled, so the
  defect cannot silently return.

No mutation was skipped as unsafe, and none is equivalent.

### 11.2 What did not change

`ReadonlyProperty.hpp`, `Prop.hpp`, every other `modules/core` header, the
component graph, the component catalogue, the module boundaries, and the four
pre-existing `ExperimentalPropertyTests`.

---

## 9. #2247 — the empty getter, closed (2026-08-10)

Opened by this slice's §6 as an **adjacency** to CCF-011 and implemented as a bounded
compatible ticket in the following batch. `Property<T>`'s constructor now rejects an
empty getter with `System::ArgumentNullException("customGetter")`, **before** anything
is done with either callable. CCF-011's established policy — decide emptiness at the
public boundary, and report an argument to an ordinary method as
`ArgumentNullException` — applied unchanged, so **no family was reopened, extended,
renumbered or minted**: CCF-011 stays closed with its six named members, none of them
this header, and no `SR-AUD-*` identifier was created.

**What was defective**: an empty `std::function` reaching a call throws
`std::bad_function_call`, a *native* exception that code catching `System::Exception&`
does not see, raised at the first read — arbitrarily far from the construction that
caused it.

**Premise correction to #2247's own acceptance criterion.** It says "ReadOnlyProperty
is unaffected". `ReadOnlyProperty` **does** exist —
`modules/core/include/SharpRuntime/Experimental/ReadonlyProperty.hpp`, whose filename
spells the second word with a lowercase `o`, which is why a naive search for the class
name misses the file — and it is **not** unaffected: it forwards its getter straight to
this constructor, so it inherits the rejection. That is the correct outcome and is
pinned by two tests. What is genuinely unaffected is the read-only **spelling**, an
empty *setter*, which still constructs and still throws
`System::NotSupportedException` on a write.

**Scope kept.** The empty setter is deliberately not rejected; #2246's layout question
is not absorbed; the vestigial `cachedValue` and `sizeof(Property<int>) == 72` are
untouched and their pin still passes.

**+9 tests** in `tests/integration/Task39RemainingTests.cpp`, beside the twelve this
slice added. No signature, layout, vtable, `noexcept` or symbol change — only a throw
added on a path that previously produced an uncatchable failure later.
