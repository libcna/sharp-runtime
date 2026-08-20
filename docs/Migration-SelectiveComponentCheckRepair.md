<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `check_selective_components.sh` was red, and built into `/tmp` — #2415

**No production code changed.** This is a repair of a gate, and of the build-policy violation that
gate was committing on every run. The test count is unmoved at **17,700 / 38**.

## Defect 1 — red since 2026-08-19, behind a green test count

`scripts/check_selective_components.sh` failed:

```
FAIL: forbidden consumer fixture forbidden_text_json_collections compiled
```

and it failed **identically on a clean tree** — verified by stashing the in-flight change and
re-running, so it was not caused by the work that surfaced it.

**Cause.** `test/consumer/forbidden_text_json_collections.cpp` included
`System/Collections/Generic/List.hpp` beside `JsonDocument.hpp` and asserted the pair must not
compile in a selective `Text.Json` build. **#1889 made that boundary legitimate**: `JsonArray` and
`JsonObject` gained fail-fast enumeration, which needs
`System::Collections::detail::MutationCounter`, and the module-boundary validator **rejected the
private declaration outright** — so `modules/text-json` took `Collections.Core` as a **public**
dependency. The fixture then asserted a boundary that had deliberately been moved.

**Why it went unnoticed for a day.** This script is **not part of CLAUDE.md rule 2's gate**, so a
green 17,68x reading said nothing about it. Every checkpoint since #1889 was recorded as green while
this was red.

**The repair retargets rather than deletes**, because CLAUDE.md names the fixture as an invariant:
*"BlockingCollection<T> belongs to Collections.Blocking; do not add its Threading requirements back
to Collections.Core or weaken the Text.Json isolation fixture."* `List.hpp` was only ever a **proxy**
for "Collections"; the proxy moves to the type that sentence is actually about, and the file is
renamed so its name still says what it asserts.

**The new proxy is strictly stronger.** `Collections.Blocking` declares `PUBLIC_DEPENDENCIES
Collections.Core Core.Base Threading`, so the include can only compile if `Text.Json` has acquired a
`Threading` requirement — which is exactly what the surrounding `assert_target_absent
sharp_runtime_threading` exists to prevent, now pinned **by compilation** rather than by a target
name alone.

## Defect 2 — the script built into `/tmp`

`MATRIX_ROOT="$(mktemp -d)"`, with **nothing in the repository setting `TMPDIR`**. CLAUDE.md's
build-resource policy says in terms: *"Never create a build tree under `/tmp`, `/var/tmp`, or
`/dev/shm` … Redirect `mktemp`-based scripts through a repository-local `TMPDIR` (this repository
uses `build-tmp/`)"* — and `build-tmp/` is in the closed list of build directories, described as
exactly this. **The mechanism was designed and never wired up.**

This script configures and builds **one selective component tree per matrix entry**, so every run
was putting eight build trees in the one place the policy exists to keep builds out of. Now:

```bash
mkdir -p "$REPO_ROOT/build-tmp"
MATRIX_ROOT="$(TMPDIR="$REPO_ROOT/build-tmp" mktemp -d)"
```

The script's existing `EXIT` trap still removes its own tree, so nothing accumulates.

## Defect 3 — nothing ran it

The proximate cause of defect 1 surviving a day is that **no routine invoked this script**.
`scripts/local_ci_check.sh` — which its own header calls a pre-push gate — ran the boundary
validator, the seam checker and the negative-fixture checker, but not this.

It now runs, **last**, so the cheap checks and the full suite still report first. **It costs about
ten minutes** (measured 2026-08-20 at two jobs), and that is stated in the script rather than left
as a surprise. It is deliberately **not** behind an opt-out: a check that can be skipped is the
check that rotted.

## Evidence

`scripts/check_selective_components.sh` exits **0**, with the log showing the renamed fixture
genuinely exercised:

```
negative fixture forbidden_text_json_collections_blocking rejected
negative fixture forbidden_text_json_object_model rejected
```

Gate unchanged at **17,700 / 38, 0 failed, 0 skipped** — no production code was touched. Module
graph **41 / 95**. Negative fixture set **53 / 269**, unchanged.
