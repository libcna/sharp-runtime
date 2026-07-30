<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# The collection mutation-counter test seam: one definition, and how it is kept that way

*Ticket #1800 (`REMED-COLL-VERSION-SEAM-ODR`), P3, size S, category `defect`,
area `Collections`. Branch `feature/remediation-test-version-access-odr`,
2026-07-29.*

---

## 1. The defect and its ticket

`SharpRuntime::Testing::CollectionVersionAccess<TOwner>` is a **test-only access
seam**: `modules/collections/include/System/Collections/detail/MutationCounter.hpp`
declares it and never defines it, `detail::BasicMutationCounter` befriends every
specialisation of it, and each of fifteen collections befriends the one
specialisation that names it. That arrangement lets a regression position a
mutation counter at a 2^32 or 2^64 boundary without performing billions of real
mutations, and lets a test assert *"this operation moved the counter by exactly
one"* — neither of which is observable through the fail-fast enumerator contract
alone. Because production defines nothing, a consumer that names the seam gets
`error: incomplete type … used in nested name specifier`; that is proved, not
asserted, by `test/consumer/collections_mutation_version_negative.cpp`.

The defect was in how the *definition* was supplied. **Five** translation units
of the single `SharpRuntimeTests_Collections_Core` program each wrote their own
copy, in two families:

| Family | Introduced by | Members declared |
|---|---|---|
| `SR1787_SEAM_BODY` | #1787, in `CollectionVersionCounterTests.cpp` | `version()` **and** `positionVersion()` |
| `SR1794_SEAM_BODY` | #1794, copied by #1796, #1798 and #1802 | `version()` only |

Three specialisations therefore had **two token-different definitions in one
program**. Two definitions of the same class with different member sets violate
the one-definition rule, [basic.def.odr]/12, and the case is **ill-formed, no
diagnostic required**.

Ticket #1796 found it and deliberately recorded rather than fixed it; #1798 and
#1802 each added a further copy spelled token-for-token as the `SR1794` one, so
the count of *distinct* bodies stayed at two while the number of copies grew to
five. #1799 opened #1800 for it. Those disclosures stand and are not rewritten:
`audit/AUDIT_FINAL_REPORT.md` (#1796, #1798 and #1802 closure sections),
`docs/HashtableValueAccessSafetyDesign.md` §35.9, and
`docs/ListDictionaryInternalSetterDesign.md` §37.7 are the historical record of
a known defect being carried honestly rather than quietly.

**No `SR-AUD-*` identifier is created.** The audit numbering is frozen at 364 and
this was found during remediation.

---

## 2. Complete specialisation inventory, measured

The inventory was **not** taken by grepping. `build-probe/1800_inventory.py`
preprocesses each translation unit with the build's own flags — so every macro is
expanded before anything is compared — tokenises the result with a tokeniser that
splits `>>` so nested template argument lists close correctly, and hashes the
token sequence of every `struct CollectionVersionAccess<…> { … }` definition.

### 2.1 Pre-fix

Every one of the five files belongs to the **same** executable,
`SharpRuntimeTests_Collections_Core`: `cmake/SharpRuntimeComponents.cmake`
globs `modules/collections/tests/**/*.cpp` into one `add_executable`. There is
no cross-executable duplication to excuse anything.

| Translation unit | Executable | Specialisations defined | Body | Token hash (preprocessed) |
|---|---|---|---|---|
| `CollectionVersionCounterTests.cpp` | `Collections_Core` | 16 | `SR1787_SEAM_BODY` | `<Hashtable>` `3e9e8d8206d573a7`, `<ListDictionaryInternal>` `8226bb75521eed16`, `<BasicMutationCounter<V>>` `6cc4fe132e9ded24` |
| `DictionaryEnumeratorKeyValueSafetyTests.cpp` | `Collections_Core` | 3 | `SR1794_SEAM_BODY` | `621bd6c50a16d95e`, `a893903a131f4b62`, `25674d10849b8227` |
| `HashtableRemoveVersioningTests.cpp` | `Collections_Core` | 3 | `SR1794_SEAM_BODY` | same three as above |
| `HashtableValueAccessSafetyTests.cpp` | `Collections_Core` | 3 | `SR1794_SEAM_BODY` | same three as above |
| `ListDictionarySetterContractTests.cpp` | `Collections_Core` | 3 | `SR1794_SEAM_BODY` | same three as above |

Per specialisation argument list:

| Specialisation | Definitions | Distinct bodies | Verdict |
|---|---:|---:|---|
| `<System::Collections::Hashtable>` | 5 | **2** | **ODR violation** |
| `<System::Collections::ListDictionaryInternal>` | 5 | **2** | **ODR violation** |
| `<System::Collections::detail::BasicMutationCounter<V>>` | 5 | **2** | **ODR violation** |
| `<ArrayList>`, `<Queue>`, `<Stack>`, `<BitArray>` | 1 each | 1 | legal |
| `<Generic::List<T>>` and eight further generic partial specialisations | 1 each | 1 | legal |

**The third one was not in the ticket.** #1800's row names
`CollectionVersionAccess<Hashtable>` and `<ListDictionaryInternal>`; the
**partial** specialisation `CollectionVersionAccess<BasicMutationCounter<V>>` was
divergent too — `SR1787`'s carries `read` *and* `write`, `SR1794`'s carries
`read` alone — and it is the one whose divergence is *behaviourally* reachable,
because both collection-level bodies delegate to it. Section 4 shows the
consequence.

### 2.2 Linkage, as emitted

Every member is defined inside the class body, so it is implicitly `inline`
([dcl.inline]/4) and GCC emits it as a **weak COMDAT** symbol — `W` in `nm`, one
`.group` section per function:

```
0000000000000000 W _ZN12SharpRuntime7Testing23CollectionVersionAccessIN6System11Collections9HashtableEE7versionERKS4_
0000000000000000 W _ZN12SharpRuntime7Testing23CollectionVersionAccessIN6System11Collections9HashtableEE15positionVersionERS4_m
0000000000000000 W _ZN12SharpRuntime7Testing23CollectionVersionAccessIN6System11Collections6detail20BasicMutationCounterImEEE4readERKS6_
0000000000000000 W _ZN12SharpRuntime7Testing23CollectionVersionAccessIN6System11Collections6detail20BasicMutationCounterImEEE5writeERS6_m
```

Weak COMDAT is exactly why nothing complained: the linker's *job* is to keep one
copy of a weak symbol and discard the rest. It has no way to know the copies were
supposed to be the same and were not.

One case is legal and is called out so it is not "fixed" by mistake:
`CollectionVersionCounterTests.cpp` instantiates the seam over a
`SortedDictionary` keyed by a class local to a `TestBody()` inside an anonymous
namespace. Those two symbols are `t` — **internal linkage** — so they cannot
collide with anything and are outside the ODR question entirely.

### 2.3 Not every definition emits a symbol

`ListDictionarySetterContractTests.cpp` defines `<Hashtable>` and never calls it,
so no symbol is emitted for it at all. A symbol-table check alone would therefore
have found four copies, not five. **This is why the checker of section 8 reads
source text and not object files.**

---

## 3. The C++ analysis, stated exactly

1. **What kind of entity.** `template<> struct CollectionVersionAccess<Hashtable>
   { … };` is an *explicit (full) specialisation* — a class definition.
   `template<typename V> struct CollectionVersionAccess<BasicMutationCounter<V>>
   { … };` is a *partial specialisation* — a class template definition. Both are
   subject to [basic.def.odr].

2. **Why more than one definition is allowed at all.** [basic.def.odr]/12 permits
   a class to be defined in more than one translation unit **provided** each
   definition consists of the *same sequence of tokens* and each name in it means
   the same thing in each definition. Header inclusion is the intended mechanism.

3. **Why these definitions broke it.** The two bodies are not the same token
   sequence: one declares `positionVersion`, the other does not; one declares
   `write`, the other does not. Same name, same template arguments, different
   class. The token hashes in §2.1 are the measurement.

4. **Why it is IFNDR and not an error.** [basic.def.odr]/15 ends with "no
   diagnostic required" for these violations, because detecting them needs
   whole-program comparison of every class definition, which neither a
   translation-unit compiler nor a symbol-table linker performs. Section 4 shows
   three tools declining to say anything.

5. **Whether macros affect token identity.** No — the token sequence compared is
   the one *after* preprocessing. Two macros with different names and identical
   expansions would be identical; two macros with the same name and different
   expansions, which is what happened here in spirit, are not. The inventory
   therefore compares *preprocessed* tokens, and the permanent checker of §8
   independently forbids two macros for one seam inside one file.

6. **Linkage.** External for the class type, weak/COMDAT for each in-class member
   function. Not `inline` in the "one definition may be discarded silently"
   *variable* sense; the effect is the same.

7. **Would token-identical definitions be enough?** Yes — that is precisely what
   [basic.def.odr]/12 asks for, and it is what the repair delivers, because the
   tokens now come from one file. Ticket #1796 was right that spelling its copy
   identically made things no worse; it was also right that this is not a
   solution, because "identical by discipline" degrades on the next edit.

8. **Would an anonymous namespace fix it?** It would make it *legal*: each
   translation unit would have its own distinct type with internal linkage, no
   shared symbol, no ODR question. It would also break the design — the friend
   declarations in the production headers name
   `SharpRuntime::Testing::CollectionVersionAccess`, and a class in a per-TU
   anonymous namespace is a *different* class that those declarations do not
   befriend, so it could not reach `version_` at all. Rejected on those grounds,
   not on style.

9. **Must a specialisation be declared before first use?** Yes.
   [temp.expl.spec]/7: if a template is specialised after it has already been
   implicitly instantiated in that translation unit, the program is ill-formed,
   again with no diagnostic required. All five suites include the authoritative
   header before any use, which satisfies this by construction; scattering
   definitions through the file did not.

10. **Can one definition live in a shared test-support component?** Yes, and it
    now does — as a header, not a compiled component. Section 6 explains why a
    compiled component cannot carry it.

11. **Would a primary-template customization hook avoid specialisations?** It
    would replace them with something worse. The primary template must stay
    *undefined* so that a consumer cannot name it; a hook (a defaulted member,
    a trait) requires a definition, which is exactly the property the negative
    consumer fixture exists to protect. Rejected.

---

## 4. The pre-fix reproduction

Sources: `build-probe/1800_odr_a.cpp` (the `SR1787` body, copied verbatim),
`build-probe/1800_odr_b.cpp` (the `SR1794` body, copied verbatim **with one
marked edit**: its counter-level `read` returns `getValueProperty() + 1000`,
which is the kind of edit #1800's row predicts a future ticket will make),
`build-probe/1800_odr_main.cpp` (the driver). Real repository headers, real
`build/libsharp_runtime_core.a`. Logs: `1800_repro.log`, `1800_lto.log`,
`1800_san.log`, `1800_opt.log`.

```
compiler: c++ (Debian 14.2.0-19) 14.2.0
linker:   GNU ld (GNU Binutils for Debian) 2.44
compile:  c++ -std=c++23 -g -O{0,1,2} -Wall -Wextra -I modules/{collections,core,io,uri,text,buffers}/include -c
link:     c++ -g -O{0,1,2} -o <out> <objs…> build/libsharp_runtime_core.a
```

The driver performs seven real `Add`s, so the honest answer is **7**.

| Optimisation | Link order | TU A reads | TU B reads | Agree? | Linker said |
|---|---|---:|---:|---|---|
| `-O0` | A then B | **7** | **7** | yes | nothing, exit 0 |
| `-O0` | B then A | **1007** | **1007** | yes | nothing, exit 0 |
| `-O1` | A then B | 7 | 1007 | **no** | nothing, exit 0 |
| `-O1` | B then A | 7 | 1007 | **no** | nothing, exit 0 |
| `-O2` | either | 7 | 1007 | **no** | nothing, exit 0 |

Read the first two rows carefully. **Swapping two object files on the link line
changed the answer that translation unit A got, in a translation unit that had
spelled the correct body itself.** That is the ticket's "a test compiled against
one intended body can execute the other", demonstrated rather than argued: A
spelled `return c.getValueProperty();` and printed `1007`.

Rows three onwards are worse in a different way: at `-O1` and above each unit
inlines its own body and the two units **disagree inside one process**, whatever
the link order. The repository builds `Debug` (`-O0`), so it lives in the
link-order regime today; a Release configuration would move it into the
disagreement regime without a single source change.

`positionVersion` is in A's class definition and not in B's, and the driver calls
it. After `positionVersion(4242)` the `-O0` A-then-B build reads `4242` and the
B-then-A build reads `5242`: the *write* went through A's `write`, the *read*
came back through B's `read`. The two halves of one seam came from two different
classes.

### 4.1 What the tools said

| Check | Result |
|---|---|
| `ld`, ordinary link, both orders | **no diagnostic**, exit 0 |
| `-flto -Wodr -Wlto-type-mismatch`, both orders | **no diagnostic**, exit 0, link order still decided the answer |
| `-fsanitize=address` with `ASAN_OPTIONS=detect_odr_violation=2` | **no diagnostic** — ASan's ODR detector compares *global variables*, not class definitions |
| `-fsanitize=undefined` | **no diagnostic** — no operation in either body is undefined in isolation |

GCC's `-Wodr` compares type layout and data members across LTO partitions.
Neither divergent body has a data member, and member functions are not part of
the layout, so there is nothing for it to see. **`-Wodr` is not a substitute for
the check in §8, and this is the measurement that says so.**

---

## 5. Alternatives evaluated

| | ODR correct | Test precision kept | Production impact | ABI/layout | CMake work | Selective builds | Future-ticket safety |
|---|---|---|---|---|---|---|---|
| **A. One inline definition in a tracked test-support header** ✅ **selected** | yes | full | none | none | none | works everywhere | second body in the same TU is a compile error |
| B. Out-of-line definitions in a shared test-support source | yes | full | none | none | new target + link edges | fragile | no better than A |
| C. Friend accessor in the production header | yes | full | **new production API** | none measured, but the seam becomes reachable | none | works | breaks the negative consumer fixture |
| D. Per-test local accessors, no specialisation | yes | full | **15 collections × N suites of new friends** | none | none | works | worst maintenance |
| E. Behavioural assertions only, no counter access | yes | **lost** | none | none | none | works | n/a |
| F. One shared macro, each TU still writing its own list | **no** | full | none | none | none | works | the same defect, one indirection deeper |
| G. Keep the duplicates, rely on link order | **no** | full | none | none | none | — | rejected explicitly |

Why each non-selected option was rejected:

- **B — out-of-line in a shared source.** The seam's members must be usable from
  every including unit, so the *class* definition has to be in a header
  regardless; only the function bodies could move. And it cannot cover the nine
  **partial** specialisations at all: a partial specialisation's members are
  templates, and there is no argument set to define them out-of-line for. So B
  keeps the header, adds a CMake target and link edges to each of the sanitizer,
  selective and consumer configurations, and covers 6 of 15 cases. Strictly
  worse than A.
- **C — production friend accessor.** It would put a *definition* of the seam in
  a production header, which is exactly what
  `test/consumer/collections_mutation_version_negative.cpp` exists to forbid:
  the fixture must keep failing with `incomplete type`. It is also a production
  API change, which #1800's row rules out. Rejected on both counts. Note that
  this is *not* a case where a public break is being relabelled as test-only:
  the seam's *declaration* and *friendship* are already production and stay
  exactly as they are; only where the definition is written changed.
- **D — per-test local accessors.** Each suite would need its own name
  befriended by each collection it touches — fifteen production friend
  declarations per suite instead of one. More production surface, not less.
- **E — behavioural assertions only.** #1787's near-boundary cases cannot be
  reached by real mutations (2^32 and 2^64 of them), and #1796/#1798/#1802 all
  assert a delta of *exactly one*, which fail-fast cannot distinguish from a
  delta of two. This would delete tests, not relocate them.
- **F — a shared macro with per-TU specialisation lists.** The two bodies that
  diverged were *already* macros. Centralising the macro while leaving each unit
  to spell its own `template<> struct … { MACRO(X) }` leaves the same failure
  mode: a unit can pass a different macro, or define a body without one. §8's
  rule 4 forbids that shape explicitly.
- **G.** Rejected. §4 is the reason.

---

## 6. The selected architecture

**One header owns every definition; every suite includes it.**

`modules/collections/tests/support/CollectionVersionSeam.hpp` contains:

- the counter-level partial specialisation
  `CollectionVersionAccess<detail::BasicMutationCounter<V>>` with `read` **and**
  `write` — the only code anywhere that touches `BasicMutationCounter::value_`;
- one macro, `SHARP_RUNTIME_COLLECTION_VERSION_SEAM(...)`, expanding to
  `version()` **and** `positionVersion()`, `#undef`ed at the end of the header so
  it cannot leak into an including unit;
- all fifteen collection specialisations, each written through that macro.

The five suites lost their local blocks and gained one line:

```cpp
#include "../../support/CollectionVersionSeam.hpp"
```

### 6.1 Why a *relative* include and no CMake change

Because a relative include cannot be misconfigured. The preprocessor resolves it
against the including file's own directory, so the header is reachable, with the
same spelling and no target property, in:

- the full `build/` tree;
- the isolated `Collections.Core` selective build (§10);
- the `build-asan/` sanitizer tree;
- any future consumer or fixture configuration.

Adding `modules/<m>/tests` to every test target's include path would have worked
in `build/` and then needed re-checking in each of the others. All five users sit
in `modules/collections/tests/System/Collections/`, so all five spell the include
identically.

`scripts/validate_module_boundaries.py` scans `.hpp` files under `tests/` and
resolves only includes beginning `System/` or `SharpRuntime/`; the new header's
own includes all resolve to `Collections.Core`, which the module already depends
on, and `"../../support/…"` is not a project-prefixed include, so nothing in the
boundary model changes. The graph still reads **41 modules / 90 edges**.

### 6.2 Which body became canonical, and why

The **richer** one — `SR1787`'s, with `positionVersion` and `write`. The four
suites that never had `positionVersion` now can call it and do not. The
alternative, canonising the lean body, would have deleted #1787's entire
near-boundary matrix. The three surviving preprocessed token hashes
(`3e9e8d8206d573a7`, `8226bb75521eed16`, `6cc4fe132e9ded24`) are the `SR1787`
hashes from §2.1 unchanged, which is the measurement that **no test capability
was traded away**.

### 6.3 What the seam permits, and what it does not

| Capability | Available to repository tests | Available to a consumer |
|---|---|---|
| Read a collection's counter (`version`) | yes | **no** — incomplete type |
| Read a counter object (`read`) | yes | **no** |
| Position a counter (`positionVersion`, `write`) | yes | **no** |
| Reach any other private member | **no** — the seam names `version_` and nothing else | no |
| Alter a production invariant | only by positioning the counter, which is the point | no |

The seam remains *access only*. It adds no operation a collection could not
perform on itself, it does not expose storage, and it is unreachable outside the
test trees because production still defines nothing.

---

## 7. Production impact — none, measured

| Surface | Change |
|---|---|
| Public headers | none — not one file under any `modules/*/include` was touched |
| Signatures, vtables, object layout | none |
| Mangled symbols in any library | none |
| `System::Collections` behaviour | none |
| Consumer source compatibility | none |
| Consumer rebuild requirement | none |

`git diff --stat` for this ticket touches only `modules/collections/tests/`,
`scripts/`, `test/`, `docs/`, `audit/`, `CLAUDE.md`, `NEXT.md`, `plan.md`,
`README.md`-adjacent planning files and `plan.sqlite3`. The counter contract of
#1787, the safe value access of #1796, the `ListDictionaryInternal` semantics of
#1798, the effective-mutation `Remove` of #1802, the owning `IEnumerator` and
`IDictionaryEnumerator` contracts, the shared `MutationCounter`, and every
`SortedSet` remediation are untouched and their suites pass unmodified.

### 7.1 The one cost, reported

The authoritative header includes all fifteen collection headers, because a full
specialisation needs a complete type. Four suites that previously included two of
them now include all fifteen. Measured with `-fsyntax-only`, best of three
(`build-probe/1800_compiletime.log`):

| Translation unit | Before | After | Delta |
|---|---:|---:|---|
| `CollectionVersionCounterTests.cpp` | 2.02 s | 2.04 s | +0.02 s (+1.0 %) |
| `DictionaryEnumeratorKeyValueSafetyTests.cpp` | 1.23 s | 1.61 s | +0.38 s (+30.9 %) |
| `HashtableRemoveVersioningTests.cpp` | 1.25 s | 1.67 s | +0.42 s (+33.6 %) |
| `HashtableValueAccessSafetyTests.cpp` | 1.21 s | 1.60 s | +0.39 s (+32.2 %) |
| `ListDictionarySetterContractTests.cpp` | 1.27 s | 1.69 s | +0.42 s (+33.1 %) |

**+1.6 s of front-end time** across the four, against a 336 s clean-first rebuild
of the whole repository — about 0.5 %. Splitting the header into a generic and a
non-generic half would recover most of it and would still satisfy the checker,
because its rule is per-specialisation and the two halves would be disjoint. It
was **not** done: two headers means a maintainer has to decide which one to
include and which one a new collection belongs in, and that decision is exactly
the kind that produced two bodies in the first place. The 1.6 s is the price of
having one place to look.

---

## 8. The permanent regression check

`scripts/check_version_seam_odr.py`, with fixtures in
`test/check_version_seam_odr_test.py`. Deterministic, standard library only, no
configured build required, `--root` for testing against a fixture tree.

It first **discovers** the seams rather than hard-coding them: any class template
declared and not defined inside `namespace SharpRuntime::Testing` in a
`modules/*/include` header is a test-only access seam. Two are found today —
`CollectionVersionAccess` and `SortedSetVersionAccess` (#1786's, which
`SortedSetVersionOverflowTests.cpp` defines and which is already, and now
verifiably, single-sited). A seam added by a future ticket is covered without
editing the checker.

Then four rules:

1. **No seam specialisation may be defined in `modules/*/include` or
   `modules/*/src`.** A defined seam is a reachable seam; this is the machine
   form of what the negative consumer fixture proves by compilation.
2. **Each `(seam, template-argument list)` may be defined in exactly one file.**
   Other suites include that file. This is the structural rule: it does not
   detect divergence, it makes divergence unconstructible.
3. **All definitions of one pair must be token-identical.** With rule 2 in force
   this is unreachable; it is the rule that still holds if rule 2 is ever given
   an exception, and it states the actual ISO requirement.
4. **Within one file, seam bodies written as a macro invocation must all invoke
   the same macro.** This is the rule that catches the divergence coming back
   *inside* the authoritative header — the shape alternative F would have left
   open.

Rule 2 is deliberately **stricter than ISO C++**: two token-identical
definitions in two files of two different executables are legal. The repository
forbids it anyway, because "two files, currently identical" is one edit and one
file move away from being the defect this ticket closed, and because the
`CONFIGURE_DEPENDS` glob means a file's executable membership is decided by its
directory, not by a deliberate act.

### 8.1 Evidence that it works

Against the **committed pre-fix sources**, extracted from git into
`build-probe/1800_prefix_tree/` (`build-probe/1800_checker_proof.log` and the
transcript below):

```
FAIL: 6 test-seam ODR problem(s) found:
  - 'CollectionVersionAccess<System :: Collections :: Hashtable>' is defined in 5 files …
  - 'CollectionVersionAccess<System :: Collections :: Hashtable>' has 2 different token bodies …
  - 'CollectionVersionAccess<System :: Collections :: ListDictionaryInternal>' is defined in 5 files …
  - 'CollectionVersionAccess<System :: Collections :: ListDictionaryInternal>' has 2 different token bodies …
  - 'CollectionVersionAccess<System :: Collections :: detail :: BasicMutationCounter < V >>' is defined in 5 files …
  - 'CollectionVersionAccess<System :: Collections :: detail :: BasicMutationCounter < V >>' has 2 different token bodies …
exit: 1
```

All three divergences, including the one #1800's row did not name.

Against the **post-fix tree with a hypothetical future suite injected** into a
throwaway copy (`build-probe/1800_injected_tree/`, never committed):

```
FAIL: 2 test-seam ODR problem(s) found:
  - … is defined in 2 files (…/FutureTicketTests.cpp, …/support/CollectionVersionSeam.hpp) …
  - … has 2 different token bodies (195e4230cb65cfc8, 69c17c90702c9c28) …
exit: 1
```

Removing the injected file returns exit 0.

Against the real repository:

```
OK: test-only access seams have one definition each (2 seam(s), 17 specialisation definition(s))
```

### 8.2 The second, earlier line of defence

The checker is the *net*. The compiler is the *wall*. A suite that includes the
authoritative header and then writes its own body no longer links quietly — it
does not compile (`build-probe/1800_redefinition_probe.cpp`):

```
error: redefinition of 'struct SharpRuntime::Testing::CollectionVersionAccess<System::Collections::Hashtable>'
note: previous definition … support/CollectionVersionSeam.hpp:154
```

The checker exists for the one case the compiler cannot see: a suite that writes
its own body and does *not* include the header.

### 8.3 The checker's own tests

`test/check_version_seam_odr_test.py`, 12 cases, each building a miniature
repository on disk:

- one defining header included by two suites → accepted;
- a *use* and a `friend` declaration are not definitions → accepted;
- a commented-out second body → accepted;
- **two suites with different bodies → rejected** (the exact #1800 shape);
- two suites with *identical* bodies → rejected by rule 2, and explicitly **not**
  by rule 3;
- a definition in a production header → rejected;
- a definition in a production source → rejected;
- two seam macros in one file → rejected;
- a repository with no declared seam → reported (so the checker cannot pass
  vacuously if the declaration is renamed);
- `build*/` and `vendor/` are not scanned;
- `A<B<C>>` tokenises as seven tokens, not six;
- comments and string literals cannot perturb a comparison.

### 8.4 Where it runs

`scripts/local_ci_check.sh` — the repository gate and the `full` GitHub Actions
job — gained:

```bash
echo "==> Validating test-only access seams (ticket #1800)"
python3 scripts/check_version_seam_odr.py
python3 test/check_version_seam_odr_test.py
```

alongside the module-boundary validator, before anything is configured or built,
so a violation is reported in about a second rather than after a full build.

### 8.5 Relationship to ticket #1801

**#1801 is not touched and is not closed.** It asks for a tracked, CI-run
*per-site* checker for the six negative consumer fixtures in `test/consumer/`,
generalising `build-probe/1796_check_negative.py`. #1800's checker shares none of
that infrastructure: it reads source text, compiles nothing, and knows nothing
about `// must fail:` markers. `build-probe/1796_check_negative.py` remains
untracked and `test/consumer/*_negative.cpp` remain proved only by "the fixture
did not compile". #1801's scope is intact and it stays `blocked`.

---

## 9. Sanitizers

| Suite | Configuration | Result |
|---|---|---|
| `Collections.Core`, 2,504 tests | `build-asan`, `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`, `ASAN_OPTIONS=detect_leaks=1:detect_odr_violation=2` | **2,504 passed, zero ASan / UBSan / LSan diagnostics** |

No use-after-free (no accessor changed — the accessors are the same tokens, in a
different file), no signed-overflow report (the counter has been unsigned since
#1787 and none of that changed), no leak.

**TSan is not relevant to this ticket and was not run.** The change relocates
test-only class definitions; it introduces no thread, no shared mutable state and
no atomic. The only atomics in reach are `SortedSet`'s Count-cache tag and count,
which belong to a different seam (`SortedSetVersionAccess`) that this ticket did
not modify and which #1784 and #1786 already covered under TSan. Running it here
would re-measure their work, not this one's.

Section 4.1 is the standing reminder that **sanitizers are not an ODR check**:
with the real divergence present and `detect_odr_violation=2` set, ASan said
nothing at all.

---

## 10. Compatibility and configuration evidence

| Configuration | Command | Result |
|---|---|---|
| Full build, fresh configure + clean-first | `cmake --fresh -S . -B build` then `cmake --build build --clean-first --parallel 3` | 0 warnings, 0 errors, 336 s |
| Full test gate | `scripts/run_component_tests.sh build` | **13,790 tests across 37 executables** |
| `Collections.Core` alone | `./build/SharpRuntimeTests_Collections_Core` | **2,504 passed** |
| Isolated `Collections.Core` selective build | `scripts/check_selective_components.sh Collections.Core collections_mutation_version.cpp` | 2,504 passed, consumer fixture passed |
| Full selective matrix, 10 components | `scripts/check_selective_components.sh` | all passed, 3 forbidden fixtures rejected, 480 s |
| Sanitizers | `build-asan`, `Collections.Core` | 2,504 passed, no diagnostic |
| Module boundaries | `scripts/validate_module_boundaries.py` | **41 modules / 90 edges** |
| Validator fixtures | `test/validate_module_boundaries_test.py` | 7/7 |
| Component catalogue | `scripts/generate_component_catalog.py --check` | current |
| Database consistency | `scripts/db_consistency_check.py --db plan.sqlite3` | no problems |
| Seam ODR check | `scripts/check_version_seam_odr.py` | OK, 2 seams, 17 definitions |
| Seam checker fixtures | `test/check_version_seam_odr_test.py` | 12/12 |
| Doxygen | `scripts/check_doxygen_warnings.sh` | **1,940** of the 1,942 ceiling |
| Whitespace | `git diff --check` | clean |

### 10.1 Fresh-rebuild evidence

The rebuild was mandatory rather than tidy: stale object files can hold a
discarded seam COMDAT, so an incremental build could have "passed" on yesterday's
definitions.

```
fresh configure marker: 2026-07-29T07:44:41Z
objects older than the fresh configuration: 0
object files rebuilt:                       632
test executables older than the marker:     0
test executables present:                   37
```

All five seam translation units carry post-marker timestamps. In the relinked
executable each seam symbol appears exactly once, and — measured by extracting
`.text.<mangled>` from each object and hashing it — the COMDAT emitted by every
unit is now **byte-identical**:

| Symbol | `CollectionVersionCounterTests` | the four later suites |
|---|---|---|
| `…CollectionVersionAccess<Hashtable>::version` | `e0c26a9f7a7161ea…` | `e0c26a9f7a7161ea…` (one defines it without using it, so emits nothing) |
| `…CollectionVersionAccess<ListDictionaryInternal>::version` | `7943e8795cd6af8a…` | `7943e8795cd6af8a…` |
| `…CollectionVersionAccess<BasicMutationCounter<unsigned long>>::read` | `f368a8d5d7313dec…` | `f368a8d5d7313dec…` |

### 10.2 The post-fix link-order probe

`build-probe/1800_fixed_{a,b,main}.cpp` are `1800_odr_*.cpp` with the local
bodies replaced by an include of the authoritative header — the shape the five
real suites now have. Six builds (three optimisation levels × two link orders,
`build-probe/1800_postfix.log`):

```
-O0 a-then-b : A reads 7, B reads 7, A == B: yes
-O0 b-then-a : A reads 7, B reads 7, A == B: yes
-O1 a-then-b : A reads 7, B reads 7, A == B: yes
-O1 b-then-a : A reads 7, B reads 7, A == B: yes
-O2 a-then-b : A reads 7, B reads 7, A == B: yes
-O2 b-then-a : A reads 7, B reads 7, A == B: yes
```

The seven-`Add` answer, every time, from both units, at every optimisation level,
in both link orders. Compare §4.

---

## 11. Build-resource accounting

| Directory | Purpose | Max parallelism |
|---|---|---|
| `build/` | fresh configure, clean-first rebuild, full gate, selective source | **3** |
| `build-asan/` | ASan/UBSan/LSan `Collections.Core` | **3** |
| `build-probe/` | every probe of this ticket, `1800_` file prefix | 1 (one compiler process per probe) |
| `build-tmp/` | repository-local `TMPDIR` for every `mktemp` in `run_component_tests.sh`, `local_ci_check.sh`, `check_selective_components.sh`, `check_doxygen_warnings.sh` | — |

**No new build directory was created**; `CLAUDE.md` rule 10's name set is closed
and this ticket's work is separated by the `1800_` file prefix inside the shared
`build-probe/`. **No compilation exceeded three jobs.**
`scripts/check_selective_components.sh` needed the repository-local `TMPDIR`
(it calls `mktemp -d`); it already caps itself at `--parallel 3` internally.

---

## 12. Risks and residual limitations

1. **The checker parses, it does not compile.** It cannot see a divergence
   introduced by two different `#define`s of the *same* macro name in two headers
   that a seam file includes conditionally. Rule 2 makes that unreachable — one
   file defines the seam and it has one macro — but the limitation is real and is
   why rule 4 exists.
2. **Rule 2 is stricter than ISO C++.** A future ticket with a genuine need for a
   second defining file will have to change the checker deliberately. That is the
   intent.
3. **The `+1.6 s` of §7.1 will grow** if a sixteenth collection is added. If it
   ever matters, split the header along the generic/non-generic line; the checker
   already permits it.
4. **`SortedSetVersionAccess` is now covered but was never broken.** Its single
   definition site is pinned rather than repaired.
5. **Nothing here makes the repository ODR-clean in general.** This is one seam
   family. No broad ODR sweep was performed and none is claimed.
6. **The `-O1`/`-O2` disagreement of §4 was latent, not hypothetical.** The
   repository builds `Debug`; a future Release configuration would have surfaced
   it as a wrong answer rather than a link-order coin flip. That is now moot, but
   it is worth recording that "benign in practice" was true only of the
   configuration that happened to be in use.

---

## 13. Implementation status

**✅ DONE.** Files changed:

| File | Change |
|---|---|
| `modules/collections/tests/support/CollectionVersionSeam.hpp` | **new** — the one authoritative definition, 15 collections + the counter-level seam |
| `modules/collections/tests/System/Collections/CollectionVersionCounterTests.cpp` | `SR1787_SEAM_BODY` block removed, header included |
| `…/DictionaryEnumeratorKeyValueSafetyTests.cpp` | `SR1794_SEAM_BODY` block removed, header included |
| `…/HashtableRemoveVersioningTests.cpp` | `SR1794_SEAM_BODY` block removed, header included |
| `…/HashtableValueAccessSafetyTests.cpp` | `SR1794_SEAM_BODY` block removed, header included |
| `…/ListDictionarySetterContractTests.cpp` | `SR1794_SEAM_BODY` block removed, header included |
| `scripts/check_version_seam_odr.py` | **new** — the permanent checker |
| `test/check_version_seam_odr_test.py` | **new** — 12 fixtures for the checker |
| `scripts/local_ci_check.sh` | runs both, in the pre-build validation phase |
| `docs/CollectionVersionTestSeamDesign.md` | **new** — this record |
| `docs/CollectionVersionCounterSweep.md` | §13.2 reconciled: "one translation unit" became "one header" |
| `CLAUDE.md` | architecture invariant for test-only access seams |
| `NEXT.md`, `plan.md`, `plan.sqlite3`, `audit/*` | closure records |

No production source, signature, symbol or layout changed. No test was deleted
and no assertion weakened: `13,790` and `2,504` are the same figures the ticket
started from, and the canonical seam body is the richer of the two that existed.

### 13.1 Rollback

Revert the two commits. The five suites regain their local bodies and the
repository regains the IFNDR; nothing else moves, because nothing else was
touched.

---

## 14. Addendum — ticket #1803, the consumer-side half of §12 item 4

*Appended 2026-07-29 by ticket #1803 (`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`).
Everything above this line is #1800's own record and is preserved unedited.
#1800 stays `done`; it was not reopened, and neither
`scripts/check_version_seam_odr.py` nor `modules/collections/tests/support/CollectionVersionSeam.hpp`
was modified.*

§12 item 4 says `SortedSetVersionAccess` "is now covered but was never broken",
and §8's checker has counted its single definition site since #1800 closed. What
#1800 did **not** provide for it — and deliberately did not claim to — is the
consumer-side proof that `test/consumer/collections_mutation_version_negative.cpp`
gives `CollectionVersionAccess`. #1801 recorded that asymmetry
(`docs/NegativeConsumerFixtureValidation.md` §16.4 item 4) and opened #1803 for
it. #1803 added `test/consumer/collections_sorted_set_version_negative.cpp`,
**15 sites**, all rejected; the full record is
`docs/NegativeConsumerFixtureValidation.md` §18.

Two measurements from that ticket belong here, because they concern this
document's checker rather than its fixture.

**1. §8's discovery rule has a blind spot, and #1803's fixture is what covers
it.** The checker discovers a seam as "a class template declared and **not
defined** inside `namespace SharpRuntime::Testing` in a `modules/*/include`
header". Give the *primary template* a body in `SortedSet.hpp` and it stops
matching that description: rule 1 — "no seam specialisation may be defined in
`modules/*/include`" — never fires, because there is no longer a seam to apply it
to. Measured on identical mirror repositories
(`build-probe/1803_gap_probe.py`, log `build-probe/1803_gap_probe.log`):

| Mirror | `check_version_seam_odr.py` | #1803's consumer fixture |
|---|---|---|
| unmutated | OK, 2 seams, 18 definitions | OK, 15/15 rejected |
| **primary template defined in `SortedSet.hpp`** | **OK, exit 0** — silently 1 seam, 17 definitions | **FAIL**, 5 sites named |
| explicit *specialisation* defined in `SortedSet.hpp` | FAIL, rule 1 | FAIL, 5 sites named |
| `SortedSet<T>::state_` made public | OK, exit 0 | FAIL, 1 site named |

§8.3's vacuity case — "a repository with no declared seam → reported" — fires
only when **zero** seams are found, so one of two disappearing passes silently.
Nothing is wrong in the repository today, and the two checks together are
complete: this is the strongest available statement of why §8.5's
"they solve different failure classes" is a design property and not a
convenience. Strengthening the guard so that a seam *leaving* discovery is
reported is a separate one-rule change, opened as inactive ticket **#1804** and
deliberately not made here.

**2. §3 item 8 has a consumer-facing corollary, now measured.** That item
rejects an anonymous-namespace seam because "a class in a per-TU anonymous
namespace is a *different* class that those declarations do not befriend". The
same reasoning read the other way says that **any** class spelled
`SharpRuntime::Testing::CollectionVersionAccess<X>` *is* befriended, including
one a consumer writes. #1803 compiled it:
`build-probe/1803_probe_collectionversionaccess_specialisation.cpp` defines
`CollectionVersionAccess<List<int>>` in a consumer translation unit, reads
`list.version_`, and compiles clean under `-Wall -Wextra -Wpedantic -Werror`
against the public include surface alone. The identical trick works on
`SortedSetVersionAccess<int>`. This is a property of C++ friendship, not of
either seam, it is well-formed and therefore cannot be expressed as a
compile-negative site, and it is unsupported: it is written down in
`docs/NegativeConsumerFixtureValidation.md` §18.5 rather than assumed away. §6.3's
capability table describes what the seam *hands* a consumer — nothing — and that
statement stands; it was never a claim about what a consumer can construct for
itself.

## 15. Addendum — ticket #1804, closing §14's discovery blind spot

*Appended 2026-07-30 by ticket #1804 (`REMED-TOOLING-SEAM-DISCOVERY-VACUITY`).
Everything above this line — including #1800's own §1–§13 and #1803's §14 — is
preserved unedited. #1800 stays `done` and was not reopened; #1804 changes only
`scripts/check_version_seam_odr.py` and `test/check_version_seam_odr_test.py`,
touches no production code, and changes no seam count in the real repository
(still `2 seam(s), 18 specialisation definition(s)`).*

§14 measured a defence-in-depth gap and deliberately left it open: the checker
discovered a seam as *"a class template declared and **not defined** inside
`namespace SharpRuntime::Testing` in a `modules/*/include` header"*, so giving a
seam's **primary template** a body in that header made it stop matching the
description. Rule 1 never fired (there was no longer a seam to apply it to), the
run exited 0, and the only trace was the success-line seam count falling from 2
to 1. §8.3's vacuity guard fires only at **zero** seams, so one of two seams
disappearing passed silently. Nothing was ever broken in the tree — both
consumer-side negative fixtures already catch the mutation — but the gap was
opened as inactive #1804 so it would not be rediscovered.

### 15.1 Reproduction, measured

`build-probe/1804_gap_probe.py` (log `build-probe/1804_gap_probe.log`) drives the
**unmodified** checker against two-seam mirror repositories built from
real-style seam sources. Baseline: `OK, 2 seams`. Mutation — give
`CollectionVersionAccess`'s primary template a body in `MutationCounter.hpp`:
**`OK, exit 0`**, silently `1 seam`, `0` definitions. That is the exact false
pass §14's table row two named, reproduced in isolation.

### 15.2 The fix — one discovery rule, no hard-coded names

Discovery is extended to surface a name whose **primary class template is
defined** in that namespace, alongside the forward-declared shape it already
keys on. The extension is deliberately narrow:

* it fires only for a **class template** — the `struct`/`class` must be preceded
  by a `template < … >` head (`_is_class_template_head`), so a legitimate
  non-template `SharpRuntime::Testing` helper is never mistaken for a seam;
* it fires only for a **primary** definition (the name immediately followed by
  `{`), never for an explicit specialisation (`Name<args> {`), which rule 1
  already handled;
* it hard-codes **neither** seam name, preserving the discovery property #1800
  chose.

Once a defined primary is surfaced, the seam re-enters the inventory (so the
count cannot silently drop) and the **existing** rule 1 rejects it — a defined
primary template is a complete type a consumer can name, which is precisely what
rule 1 exists to forbid. The diagnostic is specialised for this case: *"…defines
the primary template of test-only seam 'CollectionVersionAccess' in a production
tree; a defined primary template is reachable from a consumer and would
otherwise leave seam discovery silently (ticket #1804)"*.

Against the fixed checker the mutation now **FAILs, exit 1**, with both seams
still counted (`build-probe/1804_gap_probe_afterfix.log`); the real repository is
unchanged at `OK, 2 seams, 18 definitions`.

### 15.3 Why not a friend-based rediscovery, and why not the vacuity guard

Two alternatives were weighed. **Widening the vacuity guard** to "fewer seams
than last time" needs a committed baseline count, which is exactly the
success-line number nobody diffs, and it cannot name *which* seam left — it is a
count, not a diagnosis. **Rediscovering seams through their `friend`
declarations** (which survive a body) is a larger change to a checker owned by a
closed ticket and would blur the definition-ownership remit #1800 scoped for
itself; it is recorded here as viable-but-out-of-scope, not adopted. The chosen
rule is the smallest one that makes the disappearing seam *reappear in the
inventory* so the machinery already present rejects it.

### 15.4 Tests and mutation campaign

`test/check_version_seam_odr_test.py` grows from 12 cases to **15**; the
original 12 pass unchanged. The three added cases:

* a single seam whose primary template is defined in a production header →
  **rejected**, and the seam is still in `report.seams` (it did not leave
  discovery);
* the full §14 shape — one of **two** seams gains a primary-template body → the
  count stays two and the run fails (the vacuity guard alone would have passed
  it);
* a non-template helper defined in `SharpRuntime::Testing` → **accepted**, and
  not counted as a seam (proving the template-only narrowing does not reject a
  legitimate helper — the condition §14 and #1804's acceptance criteria named as
  the wontfix trigger, shown here to be expressible).

The mutation campaign (`build-probe/1804_mutation_test.log`) injected
`struct CollectionVersionAccess { … };` into the real `MutationCounter.hpp`: the
checker failed, exit 1, naming `CollectionVersionAccess` and the file;
`git checkout` restored the header; the checker returned to `OK, 2 seams, 18
definitions`. No intentionally broken fixture is committed.

### 15.5 Scope and status

`done`. This is checker unsoundness for one construction, corrected without
production, ABI, layout, or semantic impact, and without a new SR-AUD identifier
(audit numbering stays frozen at 364). It does **not** broaden the checker into a
general repository ODR analyser — its remit remains the two named seams'
definition ownership, now including their primary templates.
