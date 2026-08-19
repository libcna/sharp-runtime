<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Negative consumer fixture validation — ticket #1801

**Ticket** #1801 `REMED-TOOLING-NEGATIVE-FIXTURE-CI`, P3, size S, category
`tooling`, area *Developer experience*.
**Status** ✅ DONE.
**Validation command** `scripts/local_ci_check.sh build`.

No SR-AUD-\* identifier: the audit numbering is frozen at 364 and this gap was
found during remediation, by #1796, and confirmed by #1799 to have no ticket of
its own. **No production source, signature, symbol, layout, vtable, exception
contract or collection semantic is touched by this ticket.** It is build/test
infrastructure only.

---

## 1. The problem

Seven committed files under `test/consumer/` exist for one purpose: to prove
that a set of source spellings a remediation ticket outlawed is now **rejected
by the compiler**, rather than merely discouraged in a doc-comment. Before this
ticket:

* **no tracked job compiled any of them.** `scripts/local_ci_check.sh`,
  `scripts/run_component_tests.sh`, `scripts/check_selective_components.sh`,
  every `CMakeLists.txt` and the `components.yml` workflow contain no reference
  to any of the seven files. The only tracked mention of any of them anywhere in
  the build or CI surface was a *docstring* in
  `scripts/check_version_seam_odr.py`.
* the per-site checking logic existed for **two of the seven**
  (`build-probe/1796_check_negative.py`, `build-probe/1798_check_negative.py`),
  under the gitignored `build-probe/`, so it was never committed and never ran
  outside the session that wrote it.
* one fixture,
  `collections_object_model_readonlydictionary_negative.cpp`, named a
  `scripts/check_readonlydict_empty_negative.sh` as the thing that compiled it.
  **That script has never existed in the repository, in any commit.**

Ticket #1796 had already established *why* a whole-file check is not enough:

> "The file failed to compile" proves almost nothing. One broken line hides
> every other line.

So the guarantee had decayed to "these files were once rejected", and could
decay further to nothing without a single test failing.

---

## 2. Complete fixture inventory

The ticket description says six fixtures. The independently verified count is
**seven**: `collections_dictionary_setter_negative.cpp` was added later, by
#1798, and the description predates it.

### 2.1 Negative *compile* fixtures — the family this ticket covers

Marker syntax before migration was `// must fail:` on the line above the
offending statement; after migration it is a numbered preprocessor guard plus a
`// NEGATIVE(id):` marker (§4). Every one of the seven intentionally fails as a
whole file, and after migration every site can be enabled independently.

| Fixture (`test/consumer/…`) | Ticket | Component | Sites before | Sites after | Untracked checker before | Ran in CI before | Result now |
|---|---|---|---|---|---|---|---|
| `collections_hashtable_value_access_negative.cpp` | #1796 | `Collections.Core` | 11 | **11** | `build-probe/1796_check_negative.py` | no | 11/11 rejected |
| `collections_dictionary_enumerator_negative.cpp` | #1794 | `Collections.Core` | 10 | **10** | none | no | 10/10 rejected |
| `collections_enumerator_current_negative.cpp` | #1793 | `Collections.Core` | 6 | **6** | none | no | 6/6 rejected |
| `collections_dictionary_setter_negative.cpp` | #1798 | `Collections.Core` | 6 | **6** | `build-probe/1798_check_negative.py` | no | 6/6 rejected |
| `collections_mutation_version_negative.cpp` | #1787 | `Collections.Core` | 1 | **2** | none | no | 2/2 rejected |
| `collections_object_model_readonlydictionary_negative.cpp` | #1780 | `Collections.ObjectModel` | 1 | **1** | none (named a script that never existed) | no | 1/1 rejected |
| `collections_sorted_set_view_negative.cpp` | #1783 | `Collections.Core` | 1 | **1** | none | no | 1/1 rejected |
| **total** | | | **36** | **37** | 2 of 7 | **0 of 7** | **37/37** |

`collections_mutation_version_negative.cpp` gained a site because its one marked
statement pair was two independent claims — reading the seam and positioning
through it — sharing a local variable. Splitting them cost nothing and made both
independently compilable.

Language standard is C++23 for all seven, matching `CMAKE_CXX_STANDARD 23` in
the top-level `CMakeLists.txt`. Warning flags are `-Wall -Wextra -Wpedantic
-Werror`, matching `test/consumer/CMakeLists.txt`. Expected failure categories
and diagnostic fragments are recorded next to each site in the fixture itself;
`scripts/check_negative_consumer_fixtures.py --list` prints them all.

Per-site inventory, as the checker reports it:

```
collections_dictionary_enumerator_negative.cpp        Collections.Core       10 sites
   1 unmigrated-key-override            6 hashtable-value-write-through
   2 unmigrated-value-override          7 hashtable-key-write-through
   3 hashtable-raw-key                  8 listdictionary-raw-key
   4 hashtable-key-static-cast          9 listdictionary-value-const-cast
   5 hashtable-value-static-cast       10 listdictionary-compare-nullptr
collections_dictionary_setter_negative.cpp            Collections.Core        6 sites
   1 validated-key-construct            4 find-node-const
   2 validated-key-construct-null       5 find-node-mutable
   3 validated-key-alias                6 node-type-name
collections_enumerator_current_negative.cpp           Collections.Core        6 sites
   1 unmigrated-override                4 retain-raw-pointer
   2 nongeneric-write-through           5 compare-to-nullptr
   3 typed-bridge-write-through         6 wrong-type-reinterpretation
collections_hashtable_value_access_negative.cpp       Collections.Core       11 sites
   1 unmigrated-getitem-override        7 at-const-cast
   2 indexer-alias-bind                 8 at-alias-bind
   3 indexer-address-of                 9 getitem-raw-pointer
   4 indexer-any-cast-reference        10 getitem-static-cast
   5 indexer-bind-to-parameter         11 getitem-compare-nullptr
   6 proxy-copy
collections_mutation_version_negative.cpp             Collections.Core        2 sites
   1 version-read-incomplete            2 version-position-incomplete
collections_object_model_readonlydictionary_negative.cpp  Collections.ObjectModel  1 site
   1 empty-singleton-rebind
collections_sorted_set_view_negative.cpp              Collections.Core        1 site
   1 const-getviewbetween
```

### 2.2 Families deliberately NOT absorbed

| Family | Files | Where it is validated | Why it stays separate |
|---|---|---|---|
| Positive consumer fixtures | `collections_copyto.cpp`, `collections_dictionary_views.cpp`, `collections_enumerator_current.cpp`, `collections_hashtable_remove.cpp`, `collections_hashtable_value_access.cpp`, `collections_linked_list.cpp`, `collections_mutation_version.cpp`, `collections_object_model_readonlydictionary.cpp`, `collections_sorted_set_view.cpp`, `core_base.cpp`, `blocking_collection.cpp`, and the eight component smoke fixtures | `scripts/check_selective_components.sh`, which links and **runs** them | they must compile, link and pass at run time; a compile-only checker cannot express that |
| Negative *component-isolation* fixtures | `forbidden_text_json_collections.cpp`, `forbidden_text_json_object_model.cpp`, `forbidden_xml_diagnostics.cpp` | `scripts/check_selective_components.sh` `expect_consumer_failure`, which already asserts the failure *category* (`No such file`) | each is a single `#include` in a *selectively configured* build tree; the claim is about which include directories a component exports, which needs the CMake configure step this checker deliberately avoids. They are already executed by canonical validation and by the ten-way `components.yml` matrix, so they are not a gap |
| Runtime-negative tests (expect an exception) | `ListDictionarySetterContractTests.cpp`, `HashtableRemoveVersioningTests.cpp`, the `bad_any_cast` assertions in `collections_dictionary_views.cpp`, … | GoogleTest suites in the 13,790-test gate | they assert a throw, not a rejection; `ListDictionaryInternalSetterDesign.md` §28's "a compile-rejection fixture would be theatre" is still the correct reading |
| Sanitizer-only probes | `build-probe/1796_probe_*`, `1797_probe_*`, … | ASan/UBSan/TSan runs recorded in the owning design documents | a compile-only runner cannot observe a use-after-free |
| Seam ODR fixtures (#1800) | `test/check_version_seam_odr_test.py`'s miniature repositories | `scripts/check_version_seam_odr.py` + its own tests | §15 |

---

## 3. Proof of the previous false pass

Recorded exactly, with the commands run. All work happened in
`build-probe/1801_gap/`; no tracked file was modified to produce it.

**Step 1 — no tracked job runs a per-site check.**

```
$ grep -nE "negative|check_negative|_negative\.cpp|test/consumer" \
      scripts/local_ci_check.sh scripts/run_component_tests.sh
    NO MATCH

$ for f in <the seven fixtures>; do
      git grep -l "$f" -- '*.cmake' '**/CMakeLists.txt' 'scripts/*' '.github/*'
  done
    collections_mutation_version_negative  ->  scripts/check_version_seam_odr.py   (a docstring)
    all six others                         ->  <none>
```

**Step 2 — one site made legal in a temporary copy.**
`build-probe/1801_gap/mutated.cpp` is a copy of
`collections_hashtable_value_access_negative.cpp` with the first marked site
turned into the one spelling the fixture's own header says still compiles:

```diff
-    std::any& mutableAlias = table["alpha"];
-    mutableAlias = std::any(99);
+    const std::any& mutableAlias = table["alpha"];
+    (void)mutableAlias;
```

**Step 3 — a whole-file check calls that a PASS.**

```
$ ccache g++ -std=c++23 -Wall -Wextra -Wpedantic -Werror \
      -I modules/core/include -I modules/collections/include \
      -fsyntax-only -fmax-errors=0 build-probe/1801_gap/mutated.cpp
    compiler exit non-zero  ==>  a whole-file check reports PASS (false pass)
    distinct fixture lines carrying an error: 80 115 123 139 149 152 164 168 172
```

Nine other lines still fail, so "the compiler returned non-zero" is satisfied
while one of the eleven claims has silently become false.

**Step 4 — the retained gitignored per-site checker does catch it.**
`build-probe/1801_persite_1796.py` is `build-probe/1796_check_negative.py` with
its hard-coded path replaced by `sys.argv[1]`; the checking logic is otherwise
byte-identical.

```
$ python3 build-probe/1801_persite_1796.py build-probe/1801_gap/mutated.cpp
REJECTED line  80: [[nodiscard]] void* getItem(const void*) const override
ACCEPTED line 111: const std::any& mutableAlias = table["alpha"];
REJECTED line 115: std::any* slotAddress = &table["alpha"];
…
10/11 marked sites rejected (10 distinct flagged lines)
    exit=1
```

**Step 5 — nothing tracked caught it.** Step 1 is that proof: the only tracked
reference to any negative fixture was a docstring.

---

## 4. Selected convention: numbered preprocessor guards with inline markers

Five alternatives were considered.

| Alternative | Per-site precision | False-positive resistance | Maintainability | Portability | Tracked-source clarity | Compile cost | Complexity | Verdict |
|---|---|---|---|---|---|---|---|---|
| **A** inline markers, runner *generates* one variant per marker by excising the others | good | good | good | good | good | 1 per site | needs statement-extent inference; excising a class member leaves a dangling brace, and excising a virtual override makes the class abstract, so the "isolated" variant fails for an unrelated reason | rejected |
| **B** external JSON/TOML manifest | good | good | **poor** — every line edit silently invalidates it | good | poor: the claim is not next to the code | 1 per site | a parser plus a drift check | rejected |
| **C** one file per negative expression | perfect | perfect | poor: 37 files, 37 CMake entries | good | poor: 37 near-duplicate preambles | 37 whole preambles | low | rejected |
| **D** preprocessor-selectable cases | **perfect** | **perfect** | good | good | **good** | 1 + N per file | **low** | **selected** |
| **E** compiler-`verify` comments (Clang `-verify`) | good | good | good | **poor** — `-Xclang -verify` is Clang-only; the repository's verified baseline is GCC | good | 1 per file | low | rejected |

**Alternative D, with A's inline expected-diagnostic markers**, is what shipped.
A fixture carries one directive and one numbered guard per site:

```cpp
// NEGATIVE-FIXTURE: component=Collections.Core

#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif

#if SHARP_RUNTIME_NEGATIVE_SITE == 2
    // NEGATIVE(indexer-alias-bind): cannot bind non-const lvalue reference of type 'std::any&'
    //     | cannot bind non-const lvalue reference
    std::any& mutableAlias = table["alpha"];
    mutableAlias = std::any(99);
#else
    // One tracked insert-or-replace, which is what the alias was reaching for.
    table["alpha"] = std::any(99);
#endif
```

Four properties earned it the selection:

1. **No generated source at all.** The tracked file is handed to the compiler
   as-is with a `-D`; nothing is written anywhere, so there is no temporary
   source to clean up and diagnostics name real, clickable tracked lines.
2. **A clean baseline.** `SHARP_RUNTIME_NEGATIVE_SITE == 0` — the value the
   fixture defaults to itself — must compile with **zero diagnostics**. That is
   the soundness argument for every per-site verdict: enabling a guard can only
   *add* uses of the surrounding scaffolding, never remove one, so any
   diagnostic in a single-site variant is caused by that site. It also means the
   fixtures are honest C++ that an IDE does not flag, and it removes the need for
   any CMake exclusion machinery.
3. **The `#else` branch documents the migration in compilable form**, and is
   compiled by the baseline, so a migration note cannot rot into a lie.
4. **Multi-line and member-declaration sites need no special handling.** A
   statement written over four lines, a `using` alias at namespace scope, and a
   virtual override inside a class body are all just regions.

Line stability: the marker's own line number is never used, only the region, so
inserting a comment inside a fixture cannot break a check. Uniqueness is by
`id`, enforced per fixture. Conditional code inside a region nests correctly —
the region scanner tracks `#if`/`#ifdef`/`#ifndef`/`#elif`/`#else`/`#endif`
depth.

**How a future ticket adds a negative case** — the whole recipe:

1. Add or edit a `test/consumer/<area>_negative.cpp` with the
   `// NEGATIVE-FIXTURE: component=<Component>` directive and the `#ifndef`
   prelude.
2. Add `#if SHARP_RUNTIME_NEGATIVE_SITE == <next number>` … `#endif`, with a
   `// NEGATIVE(<kebab-id>): <fragment>` marker and, optionally, `//     |
   <alternative fragment>` continuation lines.
3. Keep the all-sites-off baseline warning-free; add `(void)x;` where disabling a
   site orphans a local, and use `#else` for the migrated spelling where one
   exists.
4. Run `scripts/check_negative_consumer_fixtures.py --fixture '<name>' --verbose`.

Nothing else — no CMake entry, no manifest, no CI edit.

---

## 5. Checker architecture

`scripts/check_negative_consumer_fixtures.py`, 1,003 lines including its
docstring, no third-party dependency.

```
discover  test/consumer/*_negative.cpp
          └─ parse directive, prelude, guard regions, markers, fragments
resolve   component -> include directories, from the repository's own CMake
          metadata (generate_component_catalog._load_metadata)
compile   1 baseline + N sites per fixture, at most 3 concurrent processes
judge     baseline must succeed silently; each site must fail, in its own
          region, with one of its own declared fragments
report     one line per problem: fixture, marker, and what was wrong
```

Rules enforced, and the self-test that pins each:

| # | Rule | Self-test |
|---|---|---|
| 1 | every `*_negative.cpp` carries a `NEGATIVE-FIXTURE` directive — never skipped | `test_a_fixture_without_a_directive_is_rejected` |
| 2 | the directive names a component CMake knows | `test_an_unknown_component_is_rejected` |
| 3 | the fixture defaults the site macro itself | `test_a_fixture_without_the_prelude_is_rejected` |
| 4 | site numbers are exactly `1..N` | `test_a_gap_in_the_site_numbering_is_rejected` |
| 5 | one marker per region, with ≥1 fragment, ids unique | `test_a_site_without_a_marker_is_rejected`, `test_a_duplicate_marker_id_is_rejected` |
| 6 | a marker outside every region is stale | `test_a_stale_marker_outside_every_region_is_rejected` |
| 7 | the baseline compiles with no diagnostic | `test_a_broken_baseline_is_rejected`, `test_a_baseline_warning_is_rejected` |
| 8 | every site compile fails | `test_a_site_that_compiles_is_rejected` |
| 9 | every diagnostic located in the fixture is inside the enabled region | `test_an_error_outside_the_site_region_is_rejected` |
| 10 | at least one diagnostic is attributed to the region — so "stopped being compiled" ≠ "passed" | covered by rule 9's machinery; a fixture that no longer compiles at all trips rule 7 |
| 11 | one declared fragment matches the error text | `test_a_stale_expected_diagnostic_is_rejected` |
| 12 | at least one fixture is discovered | `test_an_empty_fixture_directory_is_rejected`, `test_a_filter_matching_nothing_is_rejected` |

Command line:

```
scripts/check_negative_consumer_fixtures.py
    [--root PATH] [--compiler CXX] [--jobs N]        # N in 1..2
    [--fixture PATTERN]...                           # development filter
    [--timeout SECONDS] [--no-ccache]
    [--log-directory PATH]                           # full log of FAILING cases
    [--list] [--verbose]
```

Exit codes: **0** every site rejected, **1** a fixture-contract failure, **2** a
usage or environment failure (compiler missing, metadata unreadable, or a job
value that is zero, negative, malformed, or above the ceiling). A missing
compiler or an unregistered component **fails**; there is no skip path.

---

## 6. Diagnostic matching

A site declares an **ordered set of acceptable fragments**: the first is the
wording GCC 14 emits today, the rest are semantic fallbacks. A match against any
one of them satisfies the rule, and `--verbose` prints *which* one matched, so a
compiler upgrade that changes wording shows up as "the second alternative is now
in use" rather than as a failure.

Matching is a substring test after normalisation that

* **collapses whitespace**, so a wrapped diagnostic still matches; and
* **folds directional quotes** (`‘’“”«»`) to ASCII, because GCC quotes
  identifiers with U+2018/U+2019 in a UTF-8 locale and with `'` under `LC_ALL=C`.

Compilation additionally forces `LC_ALL=C`, `LANG=C`, `LANGUAGE=C` and
`-fdiagnostics-color=never`, so one message is one message on every machine that
runs the gate. `--verbose` prints the compiler identity and version
(`g++ (Debian 14.2.0-19) 14.2.0` here).

Fragments in use are all stable semantic phrases: *cannot bind non-const lvalue
reference*, *discards qualifiers*, *use of deleted function*, *invalid
`static_cast` from type `std::any`*, *invalid `const_cast`*, *cannot convert
`std::any` to `void*`*, *conflicting return type specified for*, *incomplete
type*, *is private within this context*, *no match for `operator!=`*, *taking
address of rvalue*, *must be constructible from an rvalue*. No multi-line GCC
diagnostic is hard-coded anywhere.

**Attribution** is by region, not by line, and covers template instantiation:
the checker treats both `<fixture>:LINE:COL: error:` and
`<fixture>:LINE:COL:   required from here` as diagnostics located in the
fixture. The `std::any_cast<std::string&>` site is reported as a
`static_assert` failure inside `<any>`, reached from a `required from here` note
naming the fixture line — that site is proved through the note, exactly as
#1796's checker did.

**Brittleness, stated plainly.** Three things could make this report a false
failure after a toolchain change: a compiler that renames a diagnostic in a way
no listed alternative covers; a compiler that reports a site's error only in a
system header with *no* instantiation note naming the fixture; and Clang's
different phrasing throughout (the fallback alternatives were chosen with Clang
in mind but **were not measured against Clang** — the repository's verified
baseline is Linux/GCC and no Clang run is claimed here). Each shows up as a loud
`FAIL` naming the site and the expected fragments, never as a silent pass.

---

## 7. Component and compiler configuration

Include directories are **derived, never duplicated**. The checker imports
`_load_metadata` from `scripts/generate_component_catalog.py` — the same reader
that produces `docs/ComponentCatalog.md` — and resolves a component to its own
`INCLUDE_DIRECTORY` plus, transitively, that of every **PUBLIC** dependency.
Private dependencies are deliberately not followed: they are not part of the
consumer include surface, which is the entire point of a consumer fixture.
`vendor/` is added as `-isystem`, mirroring `SharpRuntime::Headers`.

```
Collections.Core         -> modules/collections/include  modules/core/include
Collections.ObjectModel  -> modules/collections-object-model/include
                            modules/collections/include  modules/core/include
                            modules/component-model/include
```

`test_the_real_repository_registers_the_fixture_components` pins that agreement.

Per compile: `-std=c++23 -Wall -Wextra -Wpedantic -Werror -fsyntax-only
-fmax-errors=0 -fdiagnostics-color=never`, plus `-isystem vendor`, the resolved
`-I` list, any `define=` from the directive, and
`-DSHARP_RUNTIME_NEGATIVE_SITE=<n>`. `-Werror` is **not** weakened anywhere.
`-fmax-errors=0` becomes `-ferror-limit=0` when the compiler reports itself as
Clang.

**Compile-only, never linked.** `-fsyntax-only` is sufficient for every one of
the 37 claims, so no component library is built, no object file is produced, and
no artifact of any kind lands in the source tree. Nothing here needs a
configured build directory.

---

## 8. Parallelism policy

Ticket #1935 moved the values and parsing into
`scripts/job_count_policy.py`, the repository's one source of truth.
Precedence is explicit `--jobs`, then `SHARP_RUNTIME_BUILD_JOBS`, then the safe
repository default **2**. Only 1 or 2 is accepted. Zero, negative, malformed,
and excessive values are **refused** with exit 2 rather than clamped, because
silently accepting a value outside the policy would hide a violation. An
explicit argument takes precedence even when the environment is malformed.

No CPU-count detection appears anywhere in the checker: no `nproc`, no
`os.cpu_count()`, no `hardware_concurrency`. Concurrency is a fixed
`ThreadPoolExecutor(max_workers=jobs)`, and a `JobMeter` counts live compiler
processes so the peak is measured rather than assumed. The peak appears in the
success line, and `test_peak_concurrency_never_exceeds_the_ceiling` asserts it
for a five-invocation fixture; `test_a_job_count_above_the_ceiling_is_refused`
asserts the refusal; `test_a_single_job_is_permitted` asserts that lower is
allowed.

A per-process `--timeout` (default 300 s) turns a hung compiler into a named
failure rather than a stalled gate
(`test_a_timeout_is_reported_rather_than_hanging`).

---

## 9. Where it runs

`scripts/local_ci_check.sh` — the repository gate, and the `full` job of
`.github/workflows/components.yml` — runs the checker immediately after
#1800's seam block and **before** `cmake -S . -B build`:

```bash
echo "==> Validating negative consumer fixtures (ticket #1801)"
python3 scripts/check_negative_consumer_fixtures.py --jobs "$BUILD_JOBS"
python3 test/check_negative_consumer_fixtures_test.py
```

Local CI and the selective-component script obtain `BUILD_JOBS` from the same
shared resolver and export `SHARP_RUNTIME_BUILD_JOBS` before invoking nested
tools. The self-tests therefore inherit the already-resolved budget rather
than choosing another pool. The workflow also sets the variable explicitly to
2. The selective script does not invoke the checker; sharing the resolver is
what prevents its own CMake builds from defining a competing policy.

That position is deliberate. The checker needs only the tracked sources and the
CMake *metadata text*; it needs no configured build directory and no generated
header, so a broken compile-rejection contract is reported in about thirteen
seconds instead of after a six-minute build.

Considered and rejected, with reasons:

* **`scripts/check_selective_components.sh`** — no. It would repeat the same 44
  compiles inside each of ten component jobs for no new information, and the
  fixtures' claims are about a component's *public headers*, which the checker
  already resolves without configuring anything. The three `forbidden_*.cpp`
  component-isolation fixtures stay there, where the selective configure step
  they depend on exists.
* **A dedicated CTest target** — no. It would require the build tree to be
  configured first, which is precisely the dependency this checker does not have,
  and would move the failure later in the gate.
* **CMake integration of any kind** — not needed. The baseline branch makes each
  fixture valid C++ with no `-D`, so there is no landmine for an IDE to trip over
  and no target to exclude. `test/consumer/CMakeLists.txt` is unchanged.

---

## 10. The checker's own tests

`test/check_negative_consumer_fixtures_test.py`, **37 cases, 2.1 s**. Each
builds a miniature repository on disk — a CMake module list, two module
registrations with a `PUBLIC_DEPENDENCIES` edge between them, two public
headers, and the fixtures under test — and compiles it for real.

Coverage, in the order the ticket asked for it:

1. one correctly failing site → accepted;
2. three sites failing independently in one file, reported in site order;
3. a site that unexpectedly **compiles** → rejected, named;
4. an unrelated error at a line outside the region → rejected (a `#define` set
   inside the region breaks a later line, so the baseline is clean and only the
   single-site variant is polluted);
5. a site with no marker → rejected;
6. a duplicate marker id → rejected;
7. a fragment no compiler emits → rejected;
8. a wrong first fragment with a correct third alternative → accepted, and the
   matched alternative is asserted;
9. a statement written over four lines, whose only error GCC reports on the
   statement's **second** physical line → accepted, and the line is asserted
   (#1796's line-scanning heuristic looked at the wrong line here);
10. a fixture whose directive carries `define=…`, with an `#error` that fires if
    the definition is not passed;
11. a fixture including a header reachable only through a **transitive public**
    dependency;
12. every case runs under a directory whose name contains a space;
13. two identical runs produce identical problem lists and case order;
14. a failing run leaves the fixture repository byte-for-byte unchanged;
15. `--jobs 4` refused, peak concurrency ≤ 3 measured, `--jobs 1` permitted;
16. an empty fixture directory, and a `--fixture` filter matching nothing, both
    **fail** rather than pass vacuously;
17. a nonexistent compiler → `EnvironmentProblem`;
18. a 1 ms timeout → a named timeout failure, not a hang.

Plus: a broken baseline; a baseline warning; a numbering gap; a fixture with no
site; a missing prelude; an unknown component; a requested log directory
receiving *only* the failing case; quote/whitespace normalisation;
`required from here` parsing; a warning's notes not being absorbed into the error
text; the GCC/Clang error-limit option; and the resolver agreeing with the real
repository's component graph.

Case 19 of the list — the permanent regression proof on a **real** tracked
fixture — is `RealFixtureMutationTests`: it mirrors the real tree by symlink,
copies `collections_sorted_set_view_negative.cpp`, asserts the unaltered copy
passes, then makes the marked expression legal and asserts the checker fails
naming `const-getviewbetween`. One site, two compiles, so it is cheap enough to
run on every gate. No tracked file is modified.

Temporary repositories go under the repository-local
`build-tmp/negative-fixture-selftest/`, never `/tmp`, and are removed on
teardown.

---

## 11. Migration of each fixture

| Fixture | What changed |
|---|---|
| `collections_hashtable_value_access_negative.cpp` | 11 `// must fail:` markers → 11 guards; `#else` branches added for the migrated `getItem` override, the tracked `table[k] = v` insert-or-replace, the `at()` snapshot and the `getItem` snapshot; `(void)proxy` / `static_cast<std::any>(proxy)` added so the baseline is warning-free; header note about `build-probe/1796_check_negative.py` replaced by the tracked checker |
| `collections_dictionary_enumerator_negative.cpp` | 10 markers → 10 guards; `#else` for both migrated `std::any` overrides and for the `any_cast` key read; long "must fail" comments moved inside their regions |
| `collections_enumerator_current_negative.cpp` | 6 markers → 6 guards; `#else` for the migrated `getCurrentProperty` override and for the migrated `any_cast<int>` read |
| `collections_dictionary_setter_negative.cpp` | 6 markers → 6 guards; the five previously uncalled anonymous-namespace functions are now called from `main` and `(void)`-guard their locals, so the baseline is warning-free; the header's "KNOWN CI GAP" paragraph is replaced by a statement that #1801 closed it |
| `collections_mutation_version_negative.cpp` | 1 marker → **2** guards (seam read, seam positioning), each self-contained |
| `collections_object_model_readonlydictionary_negative.cpp` | 1 marker → 1 guard; `(void)empty; (void)nonEmpty;` for the baseline; the reference to the never-existing `scripts/check_readonlydict_empty_negative.sh` corrected |
| `collections_sorted_set_view_negative.cpp` | 1 marker → 1 guard; the `return view.getCountProperty() == 3` tail replaced by `(void)view` so the site is self-contained |

Every fixture keeps its SPDX header, its ticket attribution, its expected-
diagnostic documentation and its migration guidance. No claim was dropped: 36
marked sites became 37 guarded sites, and every one is now proved individually.

---

## 12. Temporary mutation campaign

`build-probe/1801_mutation_campaign.py`. For each of the seven fixtures it makes
**one** marked site legal, runs the tracked checker, and requires that the
checker fail and name *exactly* that site. The other 36 sites stay invalid, which
is what proves a second failing site cannot mask the one that started compiling.

No tracked file is ever modified: the checker is pointed at a mirror root whose
`cmake/`, `modules/` and `vendor/` are symlinks to the real tree and whose
`test/consumer/` holds throwaway copies.

```
CAUGHT  collections_hashtable_value_access_negative.cpp [indexer-alias-bind]        exit=1 problems=1
CAUGHT  collections_dictionary_enumerator_negative.cpp  [hashtable-key-static-cast] exit=1 problems=1
CAUGHT  collections_enumerator_current_negative.cpp     [retain-raw-pointer]        exit=1 problems=1
CAUGHT  collections_mutation_version_negative.cpp       [version-read-incomplete]   exit=1 problems=1
CAUGHT  collections_object_model_readonlydictionary_negative.cpp [empty-singleton-rebind] exit=1 problems=1
CAUGHT  collections_sorted_set_view_negative.cpp        [const-getviewbetween]      exit=1 problems=1
CAUGHT  collections_dictionary_setter_negative.cpp      [validated-key-construct]   exit=1 problems=1
OK      unmutated mirror exit=0: 7 fixtures, 37 sites, every site rejected

7/7 mutations attempted, 0 failure(s)          (95 s, 8 checker runs, 352 compiles)
```

`problems=1` on every line is the load-bearing detail: one mutated site produced
exactly one problem, so the ten still-broken siblings neither masked it nor
produced a false report of their own.

The mutations used, all reverted, none committed:

| Fixture | Site | Made legal by |
|---|---|---|
| hashtable value access | `indexer-alias-bind` | `std::any&` → `const std::any&`, write → `(void)` |
| dictionary enumerator | `hashtable-key-static-cast` | the `static_cast` replaced by `nullptr` |
| enumerator current | `retain-raw-pointer` | the accessor call replaced by `nullptr` |
| mutation version | `version-read-incomplete` | the seam call replaced by `list.getCountProperty()` |
| readonly dictionary | `empty-singleton-rebind` | the assignment replaced by `(void)nonEmpty` |
| sorted set view | `const-getviewbetween` | `frozen.GetViewBetween(2, 4)` → `frozen` |
| dictionary setter | `validated-key-construct` | `ValidatedKey key(&gKey)` → `const int* key = &gKey` |

---

## 13. Cost

| Measure | Value |
|---|---|
| Fixtures / sites | 7 / 37 |
| Compiler invocations per run | **44** (7 baselines + 37 sites) |
| Wall clock, `--jobs 3` | **12.5 s** |
| Wall clock, `--jobs 1` | 32.8 s |
| Peak concurrent compiler processes | **3**, measured |
| Self-test wall clock | **2.1 s**, 37 cases, ~25 tiny compiles |
| Added to the canonical gate | **≈ 15 s** on a gate whose build alone is ~340 s |
| Temporary disk used by a passing run | **0 bytes** — `-fsyntax-only`, no log written unless `--log-directory` is given |
| Temporary disk used by the self-test | ≈ 40 kB peak under `build-tmp/negative-fixture-selftest/`, removed on teardown |
| ccache | measured over one run: **+7 hits, +37 uncacheable**. The seven succeeding baselines are cached; the 37 failing site compiles are not. Warm-cache wall clock is unchanged (12.5 s vs 12.6 s), and `--no-ccache` costs 13.1 s, i.e. nothing either way. ccache stays enabled because repository policy says so and it is free. |

No build tree is configured per site — none is configured at all.

---

## 14. Old checkers' disposition

| Script | Status |
|---|---|
| `build-probe/1796_check_negative.py` | **superseded.** Its per-site rule is subsumed and strengthened. Retained as historical probe source; `build-probe/1801_superseded_checkers.md` records that it must not be run. |
| `build-probe/1798_check_negative.py` | **superseded**, same disposition. |
| `build-consumer/1796_negative.log`, `1798_negative.log` | retained as the evidence behind `HashtableValueAccessSafetyDesign.md` §35 (11/11) and `ListDictionaryInternalSetterDesign.md` §37.6 (6/6). Those figures stand. |
| `scripts/check_readonlydict_empty_negative.sh` | **never existed** in any commit. The fixture that named it now names the tracked checker. |

Both superseded scripts are *inoperative*, not merely redundant, and this is
worth recording because it is a trap: they compile their fixture with no
`-DSHARP_RUNTIME_NEGATIVE_SITE`, which is now the clean baseline, so each prints
`FAIL: the negative fixture COMPILED` — the opposite of the truth. Worse, their
final comparison is `rejected == len(marked)`, and a migrated fixture has zero
`// must fail:` markers, so on a file where the compile *did* fail they would
have started passing `0 == 0` — **vacuously**. The tracked checker refuses that
case explicitly (rules 5, 12). Nothing about this ticket's correctness depends on
deleting them; they are gitignored and were never committed.

---

## 15. Relationship to ticket #1800

**#1800 stays `done` and was not reopened, reimplemented, or merged into this.**
The two checkers solve different failure classes and share no code:

| | #1800 `check_version_seam_odr.py` | #1801 `check_negative_consumer_fixtures.py` |
|---|---|---|
| Input | source **text** of the whole repository | seven tracked fixtures |
| Compiles | nothing | 44 translation units |
| Failure class | two definitions of one test-only seam specialisation in one program (IFNDR) | a spelling a ticket outlawed becoming legal again |
| Runtime | ~1 s | ~13 s |

They meet at exactly one point, and it is a dependency rather than an overlap:
`collections_mutation_version_negative.cpp` is the consumer-side half of #1800's
rule — it proves the seam is unreachable from outside the test tree, which is why
#1800's rule 1 forbids defining a seam under `modules/*/include` or
`modules/*/src`. #1801 now makes that fixture's two claims run in CI for the
first time, so #1800's guarantee is strictly better covered than when it closed.
`CollectionVersionTestSeamDesign.md` §8.5 predicted this ticket's scope exactly
and is updated rather than contradicted.

No generic process helper is shared. The duplication is a few lines of
`argparse` and `--root` boilerplate, and inventing a shared utility module for it
would have been scope the ticket did not ask for.

---

## 16. Implementation-complete results

| File | Change |
|---|---|
| `scripts/check_negative_consumer_fixtures.py` | **new** — the tracked per-site checker |
| `test/check_negative_consumer_fixtures_test.py` | **new** — 37 fixtures for the checker, including the real-fixture mutation proof |
| `scripts/local_ci_check.sh` | runs both, in the pre-configure validation phase |
| `test/consumer/*_negative.cpp` (7 files) | migrated to numbered guards; 36 markers → 37 sites |
| `docs/NegativeConsumerFixtureValidation.md` | **new** — this record |
| `CLAUDE.md` | architecture invariant for negative consumer fixtures |
| `docs/CollectionVersionTestSeamDesign.md`, `docs/HashtableValueAccessSafetyDesign.md`, `docs/ListDictionaryInternalSetterDesign.md` | the "not run by CI" statements reconciled, with their historical form preserved |
| `NEXT.md`, `plan.md`, `plan.sqlite3`, `audit/*` | closure records |

### 16.1 Validation

| Check | Command | Result |
|---|---|---|
| Negative fixtures | `scripts/check_negative_consumer_fixtures.py` | **7 fixtures, 37 sites, all rejected**, 44 invocations, peak 3, 12.5 s |
| Checker fixtures | `test/check_negative_consumer_fixtures_test.py` | **37/37**, 2.1 s |
| Mutation campaign | `build-probe/1801_mutation_campaign.py` | **7/7 caught, 0 failures** |
| Fresh configure + clean-first rebuild | `cmake --fresh -S . -B build` then `cmake --build build --clean-first --parallel 3` | 0 warnings, 0 errors |
| Full test gate | `scripts/run_component_tests.sh build` | **13,790 tests across 37 executables** |
| `Collections.Core` alone | `./build/SharpRuntimeTests_Collections_Core` | **2,504 passed** |
| Full selective matrix | `scripts/check_selective_components.sh` | all 10 components passed, 3 forbidden fixtures rejected |
| Module boundaries | `scripts/validate_module_boundaries.py --root .` | **41 modules / 90 edges** |
| Validator fixtures | `test/validate_module_boundaries_test.py` | 7/7 |
| Component catalogue | `scripts/generate_component_catalog.py --check` | current |
| Seam ODR | `scripts/check_version_seam_odr.py` | OK, 2 seams, 17 definitions |
| Seam checker fixtures | `test/check_version_seam_odr_test.py` | 12/12 |
| Database consistency | `scripts/db_consistency_check.py --db plan.sqlite3` | no problems |
| Doxygen | `scripts/check_doxygen_warnings.sh` | **1,940** of the 1,942 ceiling |
| Whitespace | `git diff --check` | clean |
| Local CI gate | `scripts/local_ci_check.sh build` | passed, and executes the new checker automatically |

### 16.2 Sanitizers

**Not applicable, and none was created.** The deliverable is a Python checker
plus compile-only fixture validation: there is no new runtime code, no new
allocation, no new thread. ASan, UBSan, LSan and TSan cannot observe a
compile-rejection contract. `test/consumer/CMakeLists.txt` and every module's
CMake metadata are unchanged, so normal test compilation is unaffected and the
existing `Collections.Core` sanitizer coverage from #1796/#1798/#1800 stands
without being re-measured. Building a sanitizer variant of a Python script would
be meaningless.

### 16.3 Build-resource accounting

| Directory | Purpose | Max parallelism |
|---|---|---|
| `build/` | fresh configure, clean-first rebuild, full gate | **3** |
| `build-probe/` | this ticket's gap reproduction, mutation campaign and notes, all `1801_` prefixed | **3** inside the checker; 1 for the raw probe compiles |
| `build-consumer/` | retained #1796/#1798 logs only; nothing new written | — |
| `build-tmp/` | repository-local `TMPDIR`, and the self-test's miniature repositories | — |

**No new build directory was created**; CLAUDE.md rule 10's name set is closed
and this ticket separates its work by the `1801_` file-name prefix inside the
shared `build-probe/`. **No compilation exceeded three jobs**, including inside
the new checker, which refuses a higher request. `scripts/check_selective_components.sh`
still needs a repository-local `TMPDIR` because it calls `mktemp -d`; it caps
itself at `--parallel 3` internally.

### 16.4 Risks and residual limitations

1. **GCC-only measurement.** Every fragment was measured against
   `g++ (Debian 14.2.0-19) 14.2.0`. The Clang fallback alternatives are
   reasoned, not measured, and `-ferror-limit=0` is untested here. A Clang run
   would show up as a named fragment mismatch, never as a silent pass.
2. **The baseline argument is about *warnings*, not about GCC's error recovery.**
   Enabling a site can, in principle, make GCC report a follow-on error at a line
   outside the region; that is rule 9, and it fails the site rather than
   accepting it. None of the 37 sites does so today.
3. **`-fsyntax-only` does not link.** A claim that can only fail at link time
   (a missing symbol, a deleted out-of-line definition) cannot be expressed in
   this framework. None of the seven fixtures makes such a claim; a future one
   that does would need a `link=yes` extension.
4. **`SortedSetVersionAccess` has no consumer-side fixture.** #1800 covers both
   seams' *ownership*; only `CollectionVersionAccess` has a fixture proving it is
   unreachable from a consumer. Adding one is a two-site edit and is recorded as
   inactive ticket **#1803** rather than smuggled into this one.
5. **Site numbering is manual.** Renumbering after deleting a middle site is a
   mechanical edit the checker enforces (rule 4) but does not perform.
6. **13 seconds is not free.** It is proportionate today. If the fixture set
   doubles, `--fixture` filtering exists for development, and the natural next
   step would be to compile the seven baselines once rather than per fixture —
   which is already the case — and nothing more.

---

## 17. Inventory changes after ticket #1801

*Everything above this line is ticket #1801's own record and is preserved
unedited; its figures describe the repository on 2026-07-29 at that ticket's
closure. This section is the running inventory, appended by each later ticket
that adds a tracked negative fixture.*

| Date | Ticket | Fixture added | Sites | Running total |
|---|---|---|---|---|
| 2026-07-29 | **#1801** | — (the convention and the checker itself) | — | **7 fixtures / 37 sites** |
| 2026-07-29 | **#1791** | `collections_list_indexer_negative.cpp` (`Collections.Core`) | **14** | **8 fixtures / 51 sites** |
| 2026-07-29 | **#1803** | `collections_sorted_set_version_negative.cpp` (`Collections.Core`) | **15** | **9 fixtures / 66 sites** |
| 2026-07-31 | **#1923** | `collections_floating_comparer_negative.cpp` (`Collections.Core`) | **8** | **10 fixtures / 74 sites** |

### 17.1 #1791's fourteen sites

`REMED-COLL-LIST-INDEXER-VERSION-IMPLEMENT` made the non-const `List<T>` /
`IList<T>` indexer return a tracked proxy and removed the mutable
`List<T>::ToVector()`. The fixture proves each outlawed spelling is now rejected
by the compiler rather than merely discouraged in a doc-comment.

| # | Marker | What it proves is rejected |
|---|---|---|
| 1 | `unmigrated-indexer-override` | a hand-written `IList<T>` still returning `int&` |
| 2 | `indexer-bind-mutable-ref` | `int& r = list[0];` |
| 3 | `indexer-bind-auto-ref` | `auto& r = list[0];` |
| 4 | `indexer-address-of` | `&list[2]` |
| 5 | `indexer-std-addressof` | `std::addressof(list[2])` |
| 6 | `indexer-bind-to-parameter` | passing `list[0]` to a `T&` parameter |
| 7 | `indexer-swap` | `std::swap(list[0], list[1])` |
| 8 | `indexer-member-write` | `points[0].x = 42;` — the C# CS1612 case |
| 9 | `indexer-member-call` | `points[1].sum()` |
| 10 | `const-proxy-write` | writing through a `const` copy of the proxy |
| 11 | `tovector-structural-mutation` | `list.ToVector().push_back(4)` |
| 12 | `tovector-bind-mutable-ref` | `std::vector<int>& v = list.ToVector();` |
| 13 | `tovector-mutable-data` | `int* d = list.ToVector().data();` |
| 14 | `readonly-mutable-alias` | `int& r = readOnlyCollection[0];` |

Three markers needed their expected fragment corrected against what GCC actually
emits, which is exactly the failure mode the per-site checker exists to surface:
sites 2, 6 and 14 are rejected at the **const-qualification** step (`binding
reference of type 'int&' to 'const int' discards qualifiers`,
`invalid user-defined conversion`) rather than for want of any conversion, because
the proxy converts to `const T&` and never to `T&`. Guessing `cannot bind
non-const lvalue reference` would have been wrong, and a whole-file check would
have hidden it.

Two spellings deliberately still compile and are therefore **not** marked:
`auto r = list[0];` (copying the proxy copies the alias, like copying a pointer)
and `*list.begin() = v;` (the explicitly unsafe STL-interop surface). Both are
documented residual hazards, pinned by permanent runtime tests rather than by
compile rejection.

### 17.2 Cost after #1791

| Measure | #1801 | #1791 |
|---|---|---|
| Fixtures / sites | 7 / 37 | **8 / 51** |
| Compiler invocations per run | 44 | **59** (8 baselines + 51 sites) |
| Wall clock, `--jobs 3` | 12.5 s | **≈ 16 s** |
| Peak concurrent compiler processes | 3, measured | **3, measured** |
| Self-test | 37/37, 2.1 s | **37/37, 2.2 s** — unchanged, the checker itself did not change |

---

## 18. Ticket #1803 — the SortedSet version seam's consumer-side guard

*Ticket #1803 (`REMED-TOOLING-SORTEDSET-SEAM-NEGATIVE-FIXTURE`), P3, size XS,
category `tooling`, area *Developer experience*. Branch
`feature/remediation-sortedset-seam-negative-fixture`, 2026-07-29. Opened
INACTIVE and BLOCKED by #1801 on the same day; activated and closed here.*

§16.4 item 4 above is this ticket's own charge sheet and is preserved unedited:

> **`SortedSetVersionAccess` has no consumer-side fixture.** #1800 covers both
> seams' *ownership*; only `CollectionVersionAccess` has a fixture proving it is
> unreachable from a consumer.

It now has one. **No production source, signature, symbol, layout, vtable,
exception contract or collection semantic is touched** — the whole change is one
new file under `test/consumer/`, this record, and planning text. No `SR-AUD-*`
identifier: the audit numbering is frozen at 364.

The ticket's row predicted "two guarded sites in an existing fixture". The
inventory of §18.2 found **fifteen** distinct supported restrictions worth
pinning and a clearer home for them, so a dedicated fixture carries all fifteen.
That is a correction to the row's own estimate, recorded rather than hidden.

### 18.1 What the seam is, and what the intended restriction is

`SharpRuntime::Testing::SortedSetVersionAccess<T>` is declared — and never
defined — by `modules/collections/include/System/Collections/Generic/SortedSet.hpp`
(lines 33-34), and befriended by `SortedSet<T>` (line 291). It exists so
ticket #1786's permanent regressions can position the shared 64-bit mutation
counter at the 2^32 Count-cache horizon and the 2^64 wrap without performing
that many real mutations, and so #1784's atomic Count-cache pair can be read
directly. Its single definition is
`modules/collections/tests/System/Collections/Generic/SortedSetVersionOverflowTests.cpp`
lines 62-88 — a **test translation unit**, not a header, which is why it has no
entry in `modules/collections/tests/support/CollectionVersionSeam.hpp` and why
`scripts/check_version_seam_odr.py` counts it as one of its 18 specialisation
definitions rather than as a seam header.

**The intended restriction, stated exactly:**

> An ordinary consumer — one that compiles against a component's declared public
> include surface, with no compiler flag that disables access control, without
> including anything under `modules/*/tests`, and without authoring a
> specialisation of a namespace it does not own — can neither name a *complete*
> `SortedSetVersionAccess<T>` nor reach, by any other route, the `SortedSet<T>`
> state that seam exists to reach.

Both halves matter. The first alone would be satisfied by an undefined seam
sitting next to public state; the second alone would be satisfied by private
state next to a defined seam. Sites 2-6 pin the first half, sites 7-15 the
second, and site 1 pins that the defining translation unit is not on the
include path.

### 18.2 Complete exposure inventory, measured

`build-probe/1803_threat_probe.py` compiles **29** candidate consumer
expressions, one per translation unit, against the resolved `Collections.Core`
consumer include surface (`modules/collections/include`, `modules/core/include`)
with the same flags the tracked checker uses. Log:
`build-probe/1803_threat_probe.log`, `g++ (Debian 14.2.0-19) 14.2.0`.

| # | Attempt by an ordinary consumer | Verdict | GCC 14.2.0 says |
|---|---|---|---|
| 1 | `using Seam = …SortedSetVersionAccess<int>;` | **ACCEPTED** | — (naming an incomplete declared type is legal and harmless) |
| 2 | `…SortedSetVersionAccess<int>* p = nullptr;` | **ACCEPTED** | — (same) |
| 3 | `…SortedSetVersionAccess<int> a;` | rejected | `aggregate '…' has incomplete type and cannot be defined` |
| 4 | `sizeof(…SortedSetVersionAccess<int>)` | rejected | `invalid application of 'sizeof' to incomplete type` |
| 5 | `…::version(set)` | rejected | `incomplete type '…' used in nested name specifier` |
| 6 | `…::positionVersion(set, 1)` | rejected | same |
| 7 | `…::cachedTag(set)` | rejected | same |
| 8 | `…::cachedCount(set)` | rejected | same |
| 9 | `…::maxCacheableVersion()` | rejected | same |
| 10 | `…::tagFor(0)` | rejected | same |
| 11 | `typename …SortedSetVersionAccess<int>::Set*` | rejected | `invalid use of incomplete type 'struct …'` |
| 12 | `…SortedSetVersionAccess<SortedSet<int>>::version(set)` | rejected | `incomplete type '…<System::…::SortedSet<int> >' used in nested name specifier` |
| 13 | unqualified `version(set)` — ADL | rejected | `'version' was not declared in this scope` |
| 14 | `using namespace SharpRuntime::Testing;` then `SortedSetVersionAccess<int>::version(set)` | rejected | `incomplete type … used in nested name specifier` |
| 15 | `set.state_` | rejected | `'…SortedSet<int>::state_' is private within this context` |
| 16 | `SortedSet<int>::State*` | rejected | `'struct …SortedSet<int>::State' is private within this context` |
| 17 | `set.cachedCount_.load()` | rejected | `'std::atomic<int> …cachedCount_' is private within this context` |
| 18 | `set.cachedCountVersion_.load()` | rejected | `'std::atomic<unsigned int> …cachedCountVersion_' is private within this context` |
| 19 | `SortedSet<int>::kMaxCacheableVersion` | rejected | `'constexpr const SharpRuntime::ulongcs …' is private within this context` |
| 20 | `SortedSet<int>::kCountNotCached` | rejected | `'constexpr const SharpRuntime::uintcs …' is private within this context` |
| 21 | `SortedSet<int>::countCacheTag(0)` | rejected | `'static constexpr … countCacheTag(…)' is private within this context` |
| 22 | `set.bumpVersion()` | rejected | `'void …bumpVersion() [with T = int]' is private within this context` |
| 23 | `iterator.version_` | rejected | `'SharpRuntime::ulongcs …Iterator::version_' is private within this context` |
| 24 | a consumer-authored explicit specialisation of the seam | **ACCEPTED** | — see §18.5 |
| 25 | `#include "CollectionVersionSeam.hpp"` | rejected | `fatal error: … No such file or directory` |
| 26 | `#include "support/CollectionVersionSeam.hpp"` | rejected | same |
| 27 | `#include "System/Collections/Generic/SortedSetVersionSeam.hpp"` | rejected | same |
| 28 | `#include "tests/support/CollectionVersionSeam.hpp"` | rejected | same |
| 29 | *(control)* `#include "System/Collections/Generic/SortedSet.hpp"` | accepted | the declaration, and nothing more |

Rows 1 and 2 are **intended** and are deliberately not fixture sites: naming an
incomplete type, or declaring a pointer to it, obtains nothing and is exactly
what a forward declaration is for. Row 24 is the one genuine limitation and is
§18.5. Everything else is a restriction, and every restriction is now pinned.

### 18.3 The seam family, side by side

| | `CollectionVersionAccess<TOwner>` | `SortedSetVersionAccess<T>` |
|---|---|---|
| Declared in | `modules/collections/include/System/Collections/detail/MutationCounter.hpp` | `modules/collections/include/System/Collections/Generic/SortedSet.hpp` |
| Namespace | `SharpRuntime::Testing` | `SharpRuntime::Testing` |
| Defined in | `modules/collections/tests/support/CollectionVersionSeam.hpp` (one header, 17 specialisations) | `modules/collections/tests/System/Collections/Generic/SortedSetVersionOverflowTests.cpp` (one translation unit, 1 definition) |
| Befriended by | `detail::BasicMutationCounter` + 15 collections | `SortedSet<T>` only |
| Capabilities | `version`, `positionVersion`, `read`, `write` | `version`, `positionVersion`, `cachedTag`, `cachedCount`, `maxCacheableVersion`, `notCached`, `tagFor` |
| Reachable through a public include path | no | no |
| Installed / exported | **nothing in this repository is installed** — there is no `install()` or `export()` call anywhere; a component's consumer surface is exactly its `$<BUILD_INTERFACE:modules/<m>/include>` | same |
| Available in a selective component build | no — `modules/*/tests` is never added to any target's include directories | same |
| Test-only compile definitions / seam macros leaked to a consumer | none — `SHARP_RUNTIME_COLLECTION_VERSION_SEAM` is `#undef`ed by its own header | none — the seam has no macro |
| #1800 ownership check | yes | yes |
| Consumer-side negative fixture | `collections_mutation_version_negative.cpp`, 2 sites, since #1801 | **`collections_sorted_set_version_negative.cpp`, 15 sites, since #1803** |

**Why only one of them had a fixture.** Not a decision — an accident of order.
`collections_mutation_version_negative.cpp` was written by #1787, whose subject
*was* the counter seam it had just introduced across fourteen collections.
#1786 introduced `SortedSetVersionAccess` a ticket earlier, as one detail of a
counter-widening ticket whose consumer fixture (`collections_sorted_set_view.cpp`,
positive) was about live views. #1800 then pinned both seams' ownership and
noted the asymmetry (§12 item 4, "now covered but was never broken"); #1801
inventoried it as a gap and, under an explicit instruction not to widen its own
claim set, opened this ticket rather than absorbing it.

### 18.4 What #1800's checker does and does not catch — measured, not argued

`build-probe/1803_gap_probe.py` runs **both** tracked checkers against the
**same** four mirror repositories, so the division of responsibility is a
measurement. Log: `build-probe/1803_gap_probe.log`.

| Mirror | `check_version_seam_odr.py` (#1800) | `check_negative_consumer_fixtures.py` (#1803's fixture) |
|---|---|---|
| unmutated | **OK**, 2 seams, 18 definitions | **OK**, 15/15 rejected |
| the seam's **primary template** given a definition in `SortedSet.hpp` | **OK, exit 0** — silently 1 seam, 17 definitions | **FAIL**, 5 sites named |
| an explicit **specialisation** defined in `SortedSet.hpp` | **FAIL** — rule 1, names the file and the seam | **FAIL**, 5 sites named |
| `SortedSet<T>::state_` made public | **OK, exit 0** | **FAIL**, 1 site named |

Row two is the important one, and it is a **newly measured limitation of
#1800's checker**, disclosed here rather than left implicit. That checker
*discovers* seams as "a class template declared and **not defined** inside
`namespace SharpRuntime::Testing` in a `modules/*/include` header". Give the
primary template a body in that header and it stops being a seam by
construction: rule 1 never fires, the run exits 0, and the only visible trace is
the seam count dropping from 2 to 1 in a success line nobody diffs. Its vacuity
guard fires only when **zero** seams are found, so one of two disappearing is
not reported.

Nothing is broken today and #1800 is **not reopened**: its rules concern
*definition ownership in the repository's own text*, and it never claimed to
detect a seam that leaves discovery. The hole is *covered* — by this ticket's
fixture, which fails loudly on exactly that mutation, which is precisely the
complementarity both tickets describe. Strengthening the vacuity guard is a
separate, precise, one-rule change and is recorded as inactive ticket **#1804**
rather than smuggled in here.

Row four is the other half of the argument for this fixture existing: making
private state public is entirely outside #1800's remit and is caught only by
compilation.

### 18.5 The one restriction that cannot be expressed, and why

**A consumer that writes its own explicit specialisation of the seam obtains the
access the friend declaration grants.** Measured, not suspected
(`build-probe/1803_probe_consumer_authored_specialisation.cpp`):

```cpp
namespace SharpRuntime::Testing {
template<> struct SortedSetVersionAccess<int> {
    static SharpRuntime::ulongcs version(const SortedSet<int>& set) { return set.state_->version; }
    static void positionVersion(SortedSet<int>& set, SharpRuntime::ulongcs v) { set.state_->version = v; }
};
}
```

compiles clean under `-Wall -Wextra -Wpedantic -Werror` against the public
include surface alone. So does the identical trick against
`CollectionVersionAccess<List<int>>`
(`build-probe/1803_probe_collectionversionaccess_specialisation.cpp`), which is
the point: **this is a property of friendship, not a SortedSet-specific hole.**

It cannot be made a negative site, because it is not an error. It is
well-formed ISO C++, and no C++ mechanism prevents a third party from defining a
class that a header befriends by name — a `friend class X;` is open to whoever
writes `X`. The alternatives were considered and are all worse: an
anonymous-namespace seam is a *different* class the friend declaration does not
befriend (`docs/CollectionVersionTestSeamDesign.md` §3 item 8, already rejected
on those grounds); a friend *function* has the same property; and removing the
friendship deletes #1786's entire near-boundary matrix.

What the restriction therefore is, precisely: the seam is not *handed* to a
consumer, and no ordinary consumer expression reaches it. A consumer that
deliberately reopens a namespace it does not own, to specialise a template a
public header documents as "test-only … never defined in production code", has
written the C++ equivalent of reflecting into a private field. It is
unsupported, it is outside every guarantee this repository makes, and it is now
written down instead of being quietly assumed away. `SortedSet.hpp`'s own
doc-comment — "nothing in this library, and nothing a consumer *links against*,
can observe or call it" — remains literally true and is not weakened by this.

### 18.6 The fixture

`test/consumer/collections_sorted_set_version_negative.cpp`, component
`Collections.Core`, **15 sites**, baseline clean.

| # | Marker | What it proves is rejected | Matched fragment (GCC 14.2.0) |
|---|---|---|---|
| 1 | `seam-definition-not-on-include-path` | `#include "System/Collections/Generic/SortedSetVersionOverflowTests.cpp"` — the defining TU's `include/`-rooted shadow path | `No such file or directory` |
| 2 | `seam-version-read-incomplete` | `SortedSetVersionAccess<int>::version(set)` | `incomplete type` |
| 3 | `seam-version-position-incomplete` | `…::positionVersion(set, 1)` | `incomplete type` |
| 4 | `seam-count-cache-read-incomplete` | `…::cachedTag(view)` | `incomplete type` |
| 5 | `seam-object-incomplete` | defining an object of the seam type | `has incomplete type and cannot be defined` |
| 6 | `seam-nested-type-incomplete` | naming the seam's `::Set` member type | `invalid use of incomplete type` |
| 7 | `state-handle-private` | `set.state_` | `is private within this context` |
| 8 | `state-type-private` | `SortedSet<int>::State` | `is private within this context` |
| 9 | `bump-version-private` | `set.bumpVersion()` | `is private within this context` |
| 10 | `cached-count-field-private` | `view.cachedCount_` | `is private within this context` |
| 11 | `cached-count-tag-field-private` | `view.cachedCountVersion_` | `is private within this context` |
| 12 | `count-cache-tag-function-private` | `SortedSet<int>::countCacheTag(0)` | `is private within this context` |
| 13 | `max-cacheable-version-private` | `SortedSet<int>::kMaxCacheableVersion` | `is private within this context` |
| 14 | `count-not-cached-private` | `SortedSet<int>::kCountNotCached` | `is private within this context` |
| 15 | `iterator-version-snapshot-private` | `iterator.version_` — the enumerator's own snapshot | `is private within this context` |

Fragment lists are ordered alternatives, as §6 prescribes: the first is the
GCC 14 wording measured here, and each site carries a Clang-shaped fallback
(`named in nested name specifier`, `is a private member of`, `file not found`).
**Those fallbacks are reasoned, not measured** — the repository's verified
baseline is Linux/GCC and no Clang run is claimed, exactly as §16.4 item 1
already states for the earlier fixtures.

Three sites carry an `#else` branch documenting the supported alternative in
compilable form: site 2 holds an iterator and lets the fail-fast contract report
modification, site 4 calls `getCountProperty()` (the public surface the cache
exists to serve), and site 9 calls `Add`, the only thing that legitimately
advances the counter. The other twelve have no public replacement, which is the
claim, so they have no `#else`; every local is `(void)`-guarded at its
declaration so the all-sites-off baseline is warning-free.

**Placement.** A dedicated fixture, not two more sites in
`collections_mutation_version_negative.cpp`. Both options were evaluated as the
ticket's acceptance criteria offers both. The existing file is #1787's, its
header comment is entirely about `CollectionVersionAccess` and `List<int>`, and
its component-level claim is about the *counter* seam; appending fifteen
SortedSet sites would have put two tickets' contracts and two seams' evidence in
one file with one ownership line. A sibling file — the spelling the acceptance
criteria names — keeps `--list` output, blame, and diagnostics attributable to
one ticket each, at a cost of one extra baseline compile (0.3 s).

**One correction to the ticket's own text.** Its acceptance criteria asks for
sites over `SharpRuntime::Testing::SortedSetVersionAccess<SortedSet<int>>`. The
seam is parameterised by the **element** type, not the set type: the correct
spelling is `SortedSetVersionAccess<int>`, which is what
`SortedSetVersionOverflowTests.cpp` uses (`using Access = …<int>;`) and what the
fixture pins. The row's spelling is *also* rejected — row 12 of §18.2 measures
it — but it would have pinned a misspelling rather than the contract, so it is
not a site.

### 18.7 Mutation campaign — the fixture is load-bearing

`build-probe/1803_mutation_campaign.py`, log
`build-probe/1803_mutation_campaign.log`. Each mutation shadows **one** path in
a mirror root whose `cmake/`, `vendor/` and every module file are symlinks to
the real tree, runs the tracked checker with `--root`, and requires it to fail
naming **exactly** the expected site set — no more and no fewer. No tracked file
is modified at any point.

```
CAUGHT  seam defined in the public header (all members)          5 sites, exactly the seam family
CAUGHT  the defining TU copied under modules/collections/include 1 site  [seam-definition-not-on-include-path]
CAUGHT  SortedSet<T>::state_ made public                         1 site  [state-handle-private]
CAUGHT  SortedSet<T>::bumpVersion() made public                  1 site  [bump-version-private]
CAUGHT  SortedSet<T>::kMaxCacheableVersion made public           1 site  [max-cacheable-version-private]
CAUGHT  SortedSet<T>::Iterator::version_ made public             1 site  [iterator-version-snapshot-private]
CAUGHT  the nested SortedSet<T>::State type made public          1 site  [state-type-private]
CAUGHT  SortedSet<T>::countCacheTag() made public                1 site  [count-cache-tag-function-private]
CAUGHT  SortedSet<T>::kCountNotCached made public                1 site  [count-not-cached-private]
CAUGHT  both Count-cache fields made public                      2 sites [cached-count-field-private,
                                                                          cached-count-tag-field-private]
OK      unmutated mirror exit=0: 1 fixture, 15 sites, every site rejected

10 mutation(s) attempted, 0 failure(s)
```

Ten mutations cover **all fifteen** sites; the exact-set requirement is what
proves a still-failing sibling site cannot mask the one that started compiling,
and that the mutation did not accidentally break something else.

**Two campaign runs failed before this one, and the reason is worth keeping.**
The first two mutations were written as naive line edits — inserting `private:`
after the *first* physical line of a multi-line declaration, and defining the
seam before `SortedSet` had been declared at all. Both produced a header that is
not valid C++, so the mirror's **baseline** failed, and the checker correctly
reported "the fixture does not compile with every site disabled, so no site
result can be attributed to its own source" instead of reporting the site. Rule
7 of §5 doing its job on the campaign itself is the strongest evidence available
that a mutation which merely breaks the build cannot be mistaken for a mutation
that exposed the seam.

### 18.8 Production impact — none

| Surface | Change |
|---|---|
| Production headers and sources | **none** — not one file under any `modules/*/include` or `modules/*/src` was touched |
| Public signatures, vtables, object layout, mangled symbols | none |
| `SortedSet<T>` layout, live-view behaviour, version-counter semantics | none |
| Collection behaviour of any kind | none |
| CMake metadata, component graph, include directories | none |
| Consumer source or binary compatibility | none |
| Consumer rebuild requirement | none |

`git diff --stat` for this ticket touches `test/consumer/` (one new file),
`docs/`, `audit/`, `CLAUDE.md`, `NEXT.md`, `plan.md` and `plan.sqlite3`. The
`build/` tree needed no reconfigure for the fixture itself: `test/consumer/*.cpp`
is referenced by explicit file name from `scripts/check_selective_components.sh`
and is not globbed into any target, and the tracked checker compiles
`-fsyntax-only` with include directories derived from CMake *metadata text*, so
it needs no configured build tree at all.

### 18.9 Validation

| Check | Command | Result |
|---|---|---|
| The new fixture alone | `check_negative_consumer_fixtures.py --fixture collections_sorted_set_version_negative.cpp --verbose` | **15/15 rejected**, baseline clean, 16 invocations, peak 3, 4.2 s |
| All negative fixtures | `scripts/check_negative_consumer_fixtures.py` | **9 fixtures, 66 sites, every site rejected**, 75 invocations, peak 3, 20.4 s |
| Checker self-tests | `test/check_negative_consumer_fixtures_test.py` | **37/37**, 2.1 s — unchanged, the checker itself was not modified |
| Mutation campaign | `build-probe/1803_mutation_campaign.py` | **10/10 caught, 0 failures** |
| Checker-responsibility probe | `build-probe/1803_gap_probe.py` | §18.4 |
| Threat-model probe | `build-probe/1803_threat_probe.py` | §18.2, 29 expressions |
| Seam ODR | `scripts/check_version_seam_odr.py` | OK, **2 seams, 18 specialisation definitions** |
| Seam checker self-tests | `test/check_version_seam_odr_test.py` | **12/12** |
| Module boundaries | `scripts/validate_module_boundaries.py --root .` | **41 modules / 90 edges** |
| Validator self-tests | `test/validate_module_boundaries_test.py` | 7/7 |
| Component catalogue | `scripts/generate_component_catalog.py --check` | current |
| Database consistency | `scripts/db_consistency_check.py --db plan.sqlite3` | no problems |

The build, full test gate, selective matrix, Doxygen and whitespace results are
in §18.11.

### 18.10 Sanitizers

**Not applicable, and none was built.** The deliverable is one compile-only
fixture and documentation: no production code, no new runtime code, no new
allocation, no new thread, and no new CTest case. ASan, UBSan, LSan and TSan
cannot observe a compile-rejection contract, and #1784's and #1786's existing
`SortedSet` sanitizer coverage is untouched and is not re-measured here.
Building a sanitizer variant for this ticket would consume hundreds of megabytes
to prove nothing.

### 18.11 Cost, build accounting and results

| Measure | after #1791 | after #1803 |
|---|---|---|
| Fixtures / sites | 8 / 51 | **9 / 66** |
| Compiler invocations per run | 59 | **75** (9 baselines + 66 sites) |
| Wall clock, `--jobs 3` | ≈ 16 s | **20.4 s** |
| Peak concurrent compiler processes | 3, measured | **3, measured** |
| Self-test | 37/37, 2.2 s | **37/37, 2.1 s** — unchanged |

| Directory | Purpose | Max parallelism |
|---|---|---|
| `build/` | incremental reconfigure, build and full test gate via `scripts/local_ci_check.sh build` | **3** |
| `build-probe/` | this ticket's threat probe, gap probe, mutation campaign, mirror roots and logs, all `1803_` prefixed | **3** inside the checker; 1 for the raw probe compiles |
| `build-tmp/` | repository-local `TMPDIR` for every `mktemp`-based script | — |
| `build-consumer/` | untouched | — |

**No new build directory was created** — CLAUDE.md rule 10's name set is closed
and this ticket separates its work by the `1803_` file-name prefix inside the
shared `build-probe/`. **No compilation exceeded three jobs**, including inside
the tracked checker, which refuses a higher request.

**Why the build was validated incrementally rather than from scratch.**
CLAUDE.md rule 12 says not to clean, delete or reconfigure a build tree unless it
is genuinely broken or the configuration genuinely changed. Neither happened: no
file under `modules/`, no `CMakeLists.txt`, and no component metadata changed, so
no object file in `build/` can be stale with respect to this ticket. A
clean-first rebuild would have written a full tree's worth of objects to the SSD
to re-derive an unchanged answer, which is the exact cost the rule exists to
avoid. `scripts/local_ci_check.sh build` was run in full, and it reconfigures and
rebuilds incrementally before running the whole gate.

### 17.2 #1923's eight sites — a fixture that pins a boundary in **both** directions

Ticket #1919 (delivered as #1921–#1924) changed the backing `std::` container of
seven Collections templates for **floating-point element and key types only**.
For every non-floating type the alias is token-identical to the standard
default, so nothing changes at all.

That makes this fixture different in kind from the nine before it. The others
prove that a set of outlawed spellings is rejected. This one has to prove
**both halves of a boundary at once**: that the floating spellings are rejected
*and* that the non-floating ones are still accepted. A whole-file "does it fail
to compile" check cannot express that — and neither can a fixture whose sites
are all rejections.

| # | Marker | What it proves |
|---|---|---|
| 1 | `readonlyset-double-raw-unordered-set` | `ReadOnlySet<double>` no longer takes `shared_ptr<std::unordered_set<double>>` |
| 2 | `readonlydict-double-raw-unordered-map` | `ReadOnlyDictionary<double,int>` no longer takes `shared_ptr<std::unordered_map<double,int>>` |
| 3 | `frozenset-double-createfromset-raw` | `FrozenSet<double>::CreateFromSet` no longer takes the raw set |
| 4 | `frozendict-double-createfrommap-raw` | `FrozenDictionary<double,int>::CreateFromMap` no longer takes the raw map |
| 5 | `dictionary-double-tomap-raw-reference` | `const std::unordered_map<double,int>&` no longer binds to `ToMap()` |
| 6 | `dictionary-double-maptype-must-not-be-raw` | a `static_assert` that the **floating** map type is still `std::unordered_map<double,int>` — must FAIL, i.e. the repair is still in place |
| 7 | `dictionary-int-maptype-must-stay-raw` | a `static_assert` that the **non-floating** map type is *not* `std::unordered_map<int,int>` — must FAIL, i.e. no non-floating consumer was broken |
| 8 | `frozenset-longdouble-iterator-raw` | `std::unordered_set<long double>::const_iterator` no longer receives `FrozenSet<long double>::begin()` |

Sites 6 and 7 are the unusual ones: each is a `static_assert` that must be
**rejected**, so the fixture fails loudly if either half of the boundary moves.
Site 6 catches a silent revert of the repair; site 7 catches a non-floating
consumer being broken by a future change to the aliases.

Site 8 replaced an earlier draft that asserted
`std::unordered_set<double>::iterator it = hashSet.begin();` is rejected. It
**is** rejected — but it was rejected before #1919 too, because
`HashSet<T>::iterator` has always been the version-checked wrapper rather than a
raw node iterator, so that site pinned a pre-existing fact and not the migration
boundary. Measurement then showed the `double` and `float` node iterators do not
move at all (only `long double` does, because its hash-code cache is switched
off), so the site was rewritten over `long double` — the one case that genuinely
moved — and two baseline `static_assert`s were added recording the
non-movement of `double` in the same file. This is recorded rather than quietly
fixed because a site that pins the wrong fact is exactly the false-pass mode
ticket #1801 exists to prevent, and it survived a first passing run of the
checker.

The `#else` branch of every site is the **migrated** spelling, so the clean
baseline this file must compile with no site selected is simultaneously a
positive fixture for the migration. Guide:
`docs/Migration-CollectionsFloatingComparers.md`.

**Running totals after #1923: 10 fixtures, 74 sites, 84 compiler invocations,
peak 2 jobs.**

---

## 19. Ticket #1935 — one compilation-job policy (2026-08-01)

### 19.1 Exact defect and safe reproduction

The #1932 handoff recorded a direct invocation of
`scripts/check_negative_consumer_fixtures.py` with no `--jobs`. The old CLI and
`check_repository()` API both used `DEFAULT_JOBS = 3`, so the run accurately
reported a peak of three compiler processes even though that batch's aggregate
ceiling was two. The corrected invocation used `--jobs 2` and reported peak 2.
This was a tooling/process-safety defect, not a runtime defect.

Ticket #1935 did **not** reproduce that violation with three real compilers.
The permanent resolution tests establish the old decision path from the
retained source/history and prove the corrected omitted path at unit level. A
fake executable exercises subprocess failure and a five-invocation pool; it
reports an exact peak of 2 without launching a C++ compiler. The real checker
and self-test validation in this batch always receives the ceiling explicitly.

### 19.2 Caller inventory and canonical contract

Before #1935 there were two competing defaults:

- the checker CLI and Python API defaulted directly to 3 and ignored
  `SHARP_RUNTIME_BUILD_JOBS`;
- `local_ci_check.sh` and `check_selective_components.sh` independently
  defaulted that variable to 3 and accepted 1..3.

Local CI passed its value to the checker, but then launched the Python
self-tests, whose direct API calls selected the checker's separate default.
The GitHub workflow called both wrappers without configuring the variable.
`run_component_tests.sh`, `test/consumer/CMakeLists.txt`, and
`test/consumer/InjectFixture.cmake` do not invoke this checker and add no nested
compiler pool.

The single rule now lives in `scripts/job_count_policy.py`:

1. an explicit `--jobs`/API value wins;
2. otherwise use `SHARP_RUNTIME_BUILD_JOBS` when it is set;
3. otherwise use the deterministic safe default 2;
4. accept exactly 1 or 2; reject zero, negative, malformed, and excessive
   values with a clear error; never detect CPU count and never clamp silently.

Both shell wrappers use that resolver and export its result to nested tools.
Local CI additionally passes the same value explicitly to this checker. The
GitHub workflow sets `SHARP_RUNTIME_BUILD_JOBS: "2"`. Thus an omitted checker
argument cannot start three compilers, explicit 1 and 2 remain supported, and
nested scripts inherit one budget instead of multiplying independent defaults.

### 19.3 Permanent proof and consequences

`test/check_negative_consumer_fixtures_test.py` now has **45 cases**. In
addition to the existing child failure, timeout, hygiene, deterministic-order,
and real-fixture mutation coverage, it permanently proves:

- explicit numeric/text 1 and 2;
- omitted-value default 2 and configured environment values 1 and 2;
- explicit argument precedence over a malformed environment;
- refusal of zero, negative, 3, larger values, empty text, nonnumeric text,
  fractional text, whitespace-padded text, and signed text;
- a fake child process returning 86 is surfaced as a compiler failure; and
- fake subprocess instrumentation reaches exactly two active jobs, while the
  real compilation path never exceeds two.

Ticket #1935 changes only repository tooling, tests, workflow configuration,
and documentation. It changes no production header/source, accepted or emitted
value, exception, public alias, iterator, symbol, ABI, layout, vtable,
`noexcept`, `constexpr`, or component edge. No `SR-AUD-*` identifier was
issued; audit numbering remains frozen at 364.

### 19.4 Final validation and retained evidence

- direct final checker: **10 fixtures / 74 sites / 84 invocations / peak 2**;
- checker self-tests: **45/45**, including exact fake peak 2;
- socket-enabled selective matrix: green, including WebSockets **24/24**;
- socket-enabled full local CI: zero build warnings/errors and
  **15,071/15,071 tests across 37 executables**;
- module graph **41/91**, seams **2/18**, Doxygen **1,937/1,942**.

`build-probe/1935_job_policy_selftest.log` and
`build-probe/1935_negative_fixture_final.log` retain the permanent self-test
and real-harness summaries. Their SHA-256 values are respectively
`6ea63b8771a42eca0cf74df53f583e0a8162db6665f310541edf41f783e7d381`
and `2c4a3e2fd0f7e3ea82abd08ce0288e5748c58e48f87783a0f7ff515cc356ecff`.
The real run was explicitly configured through both
`SHARP_RUNTIME_BUILD_JOBS=2` and `--jobs 2`.

## 20. Ticket #1925 — direct nullable-floating migration sites (2026-08-01)

The approved direct nullable-floating alias transition adds sites 9–15 to
`collections_floating_comparer_negative.cpp`:

- raw `ReadOnlySet<optional<double>>` and
  `ReadOnlyDictionary<optional<double>,int>` constructor arguments;
- raw `FrozenSet<optional<double>>::CreateFromSet` and
  `FrozenDictionary<optional<double>,int>::CreateFromMap` arguments;
- raw binding of `Dictionary<optional<double>,int>::ToMap()`;
- the assertion that nullable-double `MapType` must not remain the raw standard
  map;
- the measured nullable-long-double raw iterator spelling.

The baseline branches use the owning collection aliases. Static controls pin
that nullable-double node iterator identity remains unchanged on libstdc++ 14,
while nullable-long-double iterator identity moves because hash-code caching
changes. The final bounded checker result is **10 fixtures / 81 sites / 91
compiler invocations / peak 2 jobs**. The first invocation without a writable
`CCACHE_DIR` failed before compiling the fixtures; the canonical rerun used
repository-local `build-tmp/ccache` and is retained in
`build-probe/1925_negative_fixture.log`.

---

## 21. Ticket #2054 — the System::Buffers generic-requirement fixture (2026-08-04)

`test/consumer/buffers_generic_requirements_negative.cpp`, **component `Buffers`, 13 sites**.
It is the first fixture in this repository whose sites were **already rejected before the
ticket that added them**, and that difference is worth stating precisely, because it changes
what the fixture proves.

### 21.1 What this fixture is for

#2054 changed no runtime code. Six public `System::Buffers` generic surfaces already required
more of `T` than any public text said — default-constructibility and copy-assignability through
`std::vector<T>`, a usable `std::hash<T>` through `std::unordered_set<T>` — and every one of
those requirements was enforced only by an error deep inside libstdc++. The ticket states each
requirement in the header and adds a `static_assert` **at the point where it was already
enforced**, so exactly the same set of programs compiles.

An ordinary negative fixture pins *"the spelling a ticket outlawed is rejected"*. This one pins
two different claims:

1. every hostile instantiation is **still** rejected — an assert placed in a body that is never
   instantiated would silently relax a requirement, and only compiling proves it did not; and
2. it is rejected **by the new message**, which is the ticket's only observable effect.

The `#else` branches are therefore not migrated spellings — nobody has to migrate — but the
same call with a `T` that meets the requirement, i.e. what a caller writes today and will keep
writing.

### 21.2 The converse claim lives elsewhere, and must

"…and naming `ArrayBufferWriter<NoDefault>`, taking its `sizeof`, and every well-behaved
instantiation **still compile**" is a positive claim. A negative fixture cannot hold it: every
compile it performs either has a site enabled or is the baseline. The baseline covers part of
it, and `modules/buffers/tests/System/Buffers/BuffersGenericRequirementsTests.cpp` (9 tests)
covers the rest. The measured before/after acceptance table is
`docs/BuffersNamespaceReviewPlan.md` §23.4.

### 21.3 A limit of per-site attribution over `virtual` members — measured

Two sites do **not** spell what a caller writes, and the reason is a real limit of the checker
rather than a convenience.

`ArrayPool<T>::Shared().Rent(n)` and `MemoryPool<T>::Shared().Rent(n)` are rejected for a
non-default-constructible `T`, before and after #2054. But the failing member is `virtual` and
is reached from inside `Shared()`'s own body, so GCC roots the instantiation chain at a header
line:

```
ArrayPool.hpp: In instantiation of 'std::vector<T> SharedArrayPool<T>::Rent(intcs) [with T = NoDefault]':
ArrayPool.hpp:122:24:   required from here
ArrayPool.hpp:124:22: error: static assertion failed: …
```

No diagnostic names a line of the fixture, so rule 10 fires:

```
FAIL  …[memorypool-rent-nondefault]: the compile failed but no diagnostic names lines
      155-157; the fixture may no longer be compiled at all
```

That is the **right** answer. An unattributable rejection is exactly what rule 10 exists to
refuse, and weakening it to "the compile failed somewhere" would reintroduce the whole-file
check that ticket #1801 replaced. The fixture adapts instead: each of the two sites names the
concrete pool type and declares it `extern` at block scope, so no vtable is required and the
**call** is the first instantiation — which roots the chain on the fixture line. Same
requirement, same assert, attributable proof:

```cpp
extern System::Buffers::SharedArrayPool<NoDefault> heapArrayPool;
(void)heapArrayPool.Rent(4);
```

Both spellings were verified rejected against the pre-#2054 headers as well, so the site pins
a requirement that predates the assert rather than one the assert invented.

**Rule for a future fixture:** a `static_assert` in a `virtual` member's body is attributable
only when the fixture line is what first instantiates that member. If the member is reached
through another header-defined function — a factory, a `Shared()`, a `make_unique` inside the
library — the site must instantiate it directly, or the claim belongs in a positive test.

### 21.4 Site inventory

| # | id | Surface | Hostile `T` | Requirement |
|---|---|---|---|---|
| 1 | `arraybufferwriter-capacity-ctor-nondefault` | `ArrayBufferWriter<T>(intcs)` | `NoDefault` | default-constructible |
| 2 | `arraybufferwriter-getspan-nondefault` | `GetSpan` | `NoDefault` | default-constructible |
| 3 | `arraybufferwriter-getmemory-nondefault` | `GetMemory` | `NoDefault` | default-constructible |
| 4 | `arraybufferwriter-clear-nondefault` | `Clear` | `NoDefault` | default-constructible |
| 5 | `arraybufferwriter-clear-noncopyassignable` | `Clear` | `NoCopyAssign` | copy-assignable |
| 6 | `memorypool-rent-nondefault` | `MemoryPool` heap owner | `NoDefault` | default-constructible |
| 7 | `arraypool-rent-nondefault` | `SharedArrayPool<T>::Rent` | `NoDefault` | default-constructible |
| 8 | `arraypool-return-noclear-nondefault` | `Return(array, false)` | `NoDefault` | default-constructible (via the vtable) |
| 9 | `arraypool-noncopyassignable` | `Return(array, true)` | `NoCopyAssign` | copy-assignable |
| 10 | `searchvalues-initializer-list-equality-only` | `SearchValues(std::initializer_list<T>)` | `EqualityOnly` | usable `std::hash<T>` |
| 11 | `searchvalues-vector-equality-only` | `SearchValues(const std::vector<T>&)` | `EqualityOnly` | usable `std::hash<T>` |
| 12 | `sequencereader-tryread-nondefault` | `SequenceReader<T>::TryRead` | `NoDefault` | default-constructible |
| 13 | `sequencereader-trypeek-nondefault` | `SequenceReader<T>::TryPeek` | `NoDefault` | default-constructible |

Site 8 is deliberately the `clearArray = false` call: `Return` is `virtual`, so instantiating
the pool at all already required a default-constructible `T`, and pinning the call that needs
*nothing* is what makes that visible.

### 21.5 Result

**11 fixtures / 94 sites.** The bounded checker run over this fixture alone:
`OK: 1 negative consumer fixture(s), 13 negative site(s), every site rejected (g++ 13.3.0,
14 compiler invocation(s), peak 2 job(s), 5.4s)`.

## 22. Ticket #1894 — the CCF-019 fixtures that have nothing to reject (2026-08-19)

**#1894 closes with its negative-fixture half NOT APPLICABLE and its sanitizer half re-measured.**
It is the only ticket in this document whose fixtures were never written, and the reason is worth
recording rather than leaving as an absence.

### 22.1 Why there is nothing to pin

#1894 exists to add *"one negative consumer fixture site per spelling the CCF-019 implementation
tickets outlaw"*. Its own 2026-07-31 analysis concluded that **no CCF-019 repair has outlawed any
spelling** in `text-json` or `xml-linq` — every landed repair there (#1886, #1887, #1890, #1891,
#1895, #1898) is source-compatible by construction. The two that *would* have created outlawed
spellings are **#1888** (delete `JsonNode`'s copy/move and make `DetachParent` non-public) and
**#1899** (return non-owning view types from `Ancestors`/`AncestorsAndSelf`).

**Both are now declined.** #1888 was declined earlier; **#1899 was declined on 2026-08-19**, which
is the condition #1894's own note set for this classification: *"close the negative-fixture half as
not applicable if #1899 is declined/wontfix; do not invent Text.Json sites while #1888 remains
declined."*

**Verified rather than taken on trust.** A probe compiled against the shipped headers confirms all
four spellings are still legal today:

| spelling | ticket that would outlaw it | status |
|---|---|---|
| `JsonArray` copy-constructible | #1888 | **legal** |
| `JsonArray` move-constructible | #1888 | **legal** |
| `JsonNode::DetachParent()` callable | #1888 | **legal** |
| `Extensions::Ancestors` returns `std::vector<XElement*>` | #1899 | **legal** |

A negative fixture asserts that a spelling is *rejected by the compiler*. With nothing rejected,
writing one would mean **inventing an outlawed spelling to pin**, which is the opposite of what
these fixtures are for. Neither `test/consumer/text_json_node_lifetime_negative.cpp` nor
`test/consumer/xml_linq_object_lifetime_negative.cpp` exists, and neither should.

**This is not a claim that CCF-019 produced no fixtures at all.** Its *async* members did: #1959
landed a public source break in three spellings with
`test/consumer/threading_borrowed_callback_negative.cpp`. What #1894 scoped — the two owned-tree
modules — is where nothing was outlawed.

### 22.2 The sanitizer half, re-measured because the recorded figures were stale

#1894's note recorded a clean ASan+UBSan+LSan run over **218/218** `SharpRuntimeTests_Text_Json`
and **184/184** `SharpRuntimeTests_Xml_Linq`, from 2026-07-31. Both suites have grown a great deal
since — through #1897, #2115, #2117, #2118, #2119, #2199, #2200, #2201, #1896, #2350 and others —
so the recorded figures are evidence about a tree that no longer exists. Re-run on 2026-08-19:

| suite | ASan + LSan | UBSan |
|---|---|---|
| `SharpRuntimeTests_Text_Json` | **302 / 302, 0 reports** | **302 / 302, 0 reports** — after one repair, §22.2b |
| `SharpRuntimeTests_Xml_Linq` | **349 / 349, 0 reports** | **349 / 349, 0 reports** |

ASan was run with `detect_leaks=1`, `detect_stack_use_after_return=1`, `strict_string_checks=1`,
`check_initialization_order=1` and `detect_odr_violation=2`. UBSan was built with
`-fno-sanitize-recover=undefined`, so a violation aborts rather than printing and continuing.

### 22.2b The re-measurement found a real defect that the stale figures had hidden

**UBSan was not clean on `SharpRuntimeTests_Text_Json`, and that is the whole reason for
re-measuring rather than citing the 2026-07-31 note.** The suite **aborted** — the tree is built
with `-fno-sanitize-recover=undefined`, so a violation ends the run rather than printing and
continuing:

```
/usr/include/c++/14/bits/char_traits.h:793:20: runtime error: reference binding to misaligned
address 0x... for type 'const char_type', which requires 2 byte alignment
  #0 std::char_traits<char16_t>::length(char16_t const*)
  #1 std::u16string::basic_string(char16_t const*, allocator const&)
  #2 JsonEncodedTextTests_BOTHOverloadsAgreeOnEveryInputClassTheyCanBOTHExpress_Test::TestBody()
```

**It is a defect in a test, not in production code**, and finding *which* construction caused it
took measurement rather than reading: the array was copied verbatim into a standalone probe and
**did not reproduce**, and so did every individual literal in it. The manifestation is
**translation-unit-layout dependent** — the reported address holds a run of NUL bytes inside the
narrow string pool, which is the empty `u""` literal merged into a **1-byte-aligned** mergeable
section alongside narrow literals. `char_traits<char16_t>::length` then binds a
`const char16_t&` to an odd address to read the terminator. The value it computes is correct; the
reference binding is not.

The repair is one line — `u""` becomes `std::u16string()`, which is the same empty UTF-16 string
with no literal to misalign, so the case's coverage is unchanged:

| | before | after |
|---|---|---|
| `SharpRuntimeTests_Text_Json` under UBSan | **aborted, 1 runtime error** | **302 / 302, 0 errors** |
| the same suite in the ordinary build | 302 / 302 | 302 / 302 |

**Mutation**: restoring the `u""` literal reproduces the report (1 error). Caught.

### 22.3 The instrumentation was shown able to report

A clean run is only evidence about the *code* if a dirty run is evidence about the *sanitizer* —
the lesson #1957/SR-AUD-204 recorded when a silent TSan turned out to say nothing. So each
sanitizer was given a deliberate defect built with the same flags:

| probe | result |
|---|---|
| heap-buffer-overflow under ASan | **1 report — instrumentation live** |
| unfreed allocation under LSan | **1 report — instrumentation live** |
| shift exponent ≥ width under UBSan | **1 report — instrumentation live** |
| no defect (control) | **0 reports — correctly silent** |

### 22.4 A correction to this document's running total — and a correction to that correction

§21.5 records **11 fixtures / 94 sites**, and that has been stale since 2026-08-04: the measured
total today is **45 fixtures / 231 sites**. #1894 adds no fixture of its own, so the total is
unchanged by this section. (§23 later takes it to **47 fixtures / 240 sites**.)

**The first version of this subsection claimed the intervening fixtures "were each recorded in
their own ticket's migration note rather than here, so the record exists". That was asserted rather
than measured, and it is false.** Audited on 2026-08-19 by grepping `docs/` for every fixture
filename: **17 of the 45 had no record in `docs/` at all** — not in this document, not in any
migration note.

Two things the audit got right only on the second pass, recorded because the first pass was
misleading in the *alarming* direction:

* A first attempt searched for each **filename** inside ticket text and reported eight fixtures as
  having "no ticket record either". That was an artefact of the search: tickets describe their
  fixture without naming its file. Reading each fixture's own header instead shows **every one of
  the 45 names a real ticket** — there are **no orphans**.
* So the gap was never traceability. It was this document's coverage, and it is closed below rather
  than merely reported.

#### The 17 fixtures that had no `docs/` record

| fixture | ticket(s) | subject |
|---|---|---|
| `core_appdomain_switch_nullable_negative.cpp` | #2250 | ticket #2250 (SR-AUD-103, switch half) |
| `core_applicationid_dotnet_shape_negative.cpp` | #2291 | ticket #2291 (SR-AUD-117) |
| `core_argument_out_of_range_guard_domain_negative.cpp` | #2213, #2254 | ticket #2254 (finding SR-AUD-091) |
| `core_attribute_protected_ctor_negative.cpp` | #2339 | ticket #2339 (SR-AUD-114) |
| `core_deprecated_members_negative.cpp` | #2289 | ticket #2289 (SR-AUD-117) |
| `core_func_nonvoid_result_negative.cpp` | #2299 | ticket #2299 (SR-AUD-126) |
| `core_integer_style_validation_negative.cpp` | #2269 | ticket #2269 (SR-AUD-178) |
| `core_localdatastoreslot_no_public_ctor_negative.cpp` | #2298 | ticket #2298 (SR-AUD-129), route B |
| `core_marshalbyrefobject_protected_ctor_negative.cpp` | #2297, #2374 | ticket #2297 (SR-AUD-128) |
| `core_obsoleteattribute_nullable_negative.cpp` | #2295 | ticket #2295 (SR-AUD-116) |
| `core_resolveeventhandler_optional_negative.cpp` | #2325 | ticket #2325 (SR-AUD-123) |
| `core_runtimetype_removed_negative.cpp` | #2333, #2334 | ticket #2334 (SR-AUD-110, approval-gated clause |
| `core_tuple_getter_only_negative.cpp` | #2330 | ticket #2330 (SR-AUD-063) |
| `core_unityserializationholder_removed_negative.cpp` | #2281 | ticket #2281 (SR-AUD-137) |
| `core_valuetype_protected_ctor_negative.cpp` | #2322 | ticket #2322 (SR-AUD-068) |
| `numerics_complex_abs_return_negative.cpp` | #2172 | ticket #2172 (SR-AUD-277 remainder) |
| `numerics_generic_math_negative.cpp` | #2168 | ticket #2168 (SR-AUD-278): proves that every static member of the |

Each is validated by `scripts/check_negative_consumer_fixtures.py` exactly like the others — the
checker is the enforcing mechanism and it never fell behind; only the prose did. With this table
the audit re-runs clean: **0 fixtures without a `docs/` record.**

Build directories used: `build-asan` (reused) and `build-ubsan` (created), both `--parallel 2`,
both with `ccache`.

---

## 23. Ticket #2208 — the isolated-storage stream confinement fixture (2026-08-19)

`test/consumer/io_isolated_storage_stream_confinement_negative.cpp`, **4 sites**, component
`IO.IsolatedStorage`. Running total: **47 fixtures / 240 sites**, measured by running
`scripts/check_negative_consumer_fixtures.py` rather than derived from §22.4 -- whose 45/231 was
itself already stale, #1888/#1889 having taken it to 46/236 earlier the same day.

### 23.1 What is unusual about this one

Almost every fixture in this document pins a **spelling a ticket outlawed**. This one pins
almost none, and the reason is a premise correction worth recording.

#2208 was written as *"remove `IsolatedStorageFileStream`'s path constructor and take the owning
store instead"*, which would have been a public source break with an outlawed spelling to pin.
The reference says otherwise: .NET publishes eight constructors, all beginning
`(string path, FileMode mode, ...)`, with the store an **optional trailing** parameter
(`IsolatedStorageFileStream.cs:21-56`); its storeless form defaults to
`GetUserStoreForDomain()` and resolves through `isf.GetFullPath(path)` like every other
(`:82-118`). So the confinement landed **without removing an overload**, and on POSIX there is
no source break at all — `std::filesystem::path` converts to `std::string` implicitly there, so
the old call still compiles and only its *meaning* changes.

What is left to pin is therefore not a spelling but a **structure**: the ways a consumer could
step around the resolver.

### 23.2 The four sites

| # | id | what it proves |
|---|---|---|
| 1 | `store-fullpath-is-not-consumer-reachable` | `IsolatedStorageFile::fullPath()` is private and the stream is its only friend, so the friendship the repair relies on grants a consumer nothing |
| 2 | `resolving-ctor-is-private` | the four-argument constructor — the only one taking its `paramName` from the caller rather than from the door — is unreachable |
| 3 | `resolve-is-private` | `Resolve()` is private, so nothing advertises that the check is separable from the construction |
| 4 | `store-first-overload-does-not-exist` | the `(store, path, mode)` order .NET does not have |

**Site 4 is the one this section exists for.** An earlier cut of #2208 used exactly that order,
before the reference was read. The wrong order is not a compile error waiting to happen — it
compiles perfectly well as an *addition*, and would leave this port with two spellings where
.NET has one. A behavioural test cannot see it, because both orders confine correctly.
