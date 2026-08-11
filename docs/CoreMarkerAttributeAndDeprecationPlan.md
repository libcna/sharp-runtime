<!-- SPDX-License-Identifier: MIT -->

# `ThreadStaticAttribute` and `LoaderOptimization` — SR-AUD-113 / SR-AUD-117 review

Tickets: **#2287** (review), **#2288** (SR-AUD-113 implementation),
**#2289** (SR-AUD-117, `needs_user`). Date: 2026-08-11. The audit numbering
stays frozen at 364, **no new `SR-AUD-*` identifier was created** and **no CCF
was minted**.

---

## 1. Why these two were looked at together, and why that was wrong

An inherited ranking put them in one slice because both are small
attribute-or-enum headers in `modules/core` with no first-party production
consumer. That is a shape, not a cause, and this review was told not to assume
otherwise. It does not hold.

## 2. Verdict — not a family, and they split

| | SR-AUD-113 `ThreadStaticAttribute` | SR-AUD-117 `LoaderOptimization` |
|---|---|---|
| Kind of divergence | **behavioural claim with no mechanism** — the doc-comment promises per-thread field storage | **missing language diagnostic** — two values .NET marks `Obsolete` are deprecated in prose only |
| Does C++ offer the mechanism? | **No.** There is no way for an object of a class to attach to a declaration | **Yes.** `[[deprecated]]` is exactly it, and it is simply not used |
| Consequence | the only honest repair is to state the boundary | a real repair exists and was designed (§5) |
| Cost of the repair | **zero** — documentation, no compiled surface change | **breaks builds**: measured, a use becomes a hard error under this repo's own `-Werror` |
| Disposition | **remediated** (#2288) | **confirmed (design-complete)**, approval-bound (#2289) |

The two causes are not merely different, they are opposites in the respect that
decides the repair: one finding exists because the language has no mechanism, the
other because the language has one that the port declined to use. Repairing
either teaches nothing about the other.

### 2.1 SR-AUD-113 does have a family candidate — and it is not SR-AUD-117

**SR-AUD-115** (`ObsoleteAttribute`, `confirmed`, medium) is
«stores an error flag but cannot mark a declaration or produce its documented
compiler diagnostic». That is the *same* mechanism as SR-AUD-113: an
`Attribute`-derived class in a language with no attribute attachment cannot
deliver the effect .NET's attribute delivers, and the doc-comment describes the
effect as though it could. Only the promised effect differs — per-thread storage
there, a compiler diagnostic here.

This is recorded, not acted on. SR-AUD-115 is outside this batch's assignment,
its header carries a second finding (SR-AUD-116, a nullable-string representation
question), and **minting a CCF over the pair needs authority this batch does not
have** — CCF-021 and CCF-022 remain unminted. It is ranked as next work in the
handoff instead. Nothing in #2288 forecloses a later family treatment: the honest
boundary statement is what such a treatment would put on every member anyway.

## 3. Measured state

### 3.1 Consumer inventory — measured, not inherited

`grep` over `modules/*/include`, `modules/*/src`, `modules/*/tests`, `bench/`,
`test/` and `tests/`:

| Symbol | Production consumers | Test consumers |
|---|---:|---|
| `System::ThreadStaticAttribute` | **0** | 3 files, 5 cases — `ThreadStaticAttributeTests.cpp` (3, suite `ThreadStaticAttributeTest`), `Batch3TypeTests.cpp` (1, suite `ThreadStaticAttributeNewTests`), `SystemAttributeTests.cpp` (1, suite `MarkerAttributeTests`) |
| `System::LoaderOptimization` (the type) | **1** — `LoaderOptimizationAttribute.hpp` stores and returns it | `LoaderOptimizationTests.cpp`, `SystemAttributeTests.cpp` |
| `LoaderOptimization::DomainMask` / `::DisallowBindings` (the deprecated names) | **0** | **1 file, 9 sites, 5 cases** — `LoaderOptimizationTests.cpp:33,37,41,70,88,89` |

The last row is the one that matters for §5 and it was measured rather than
assumed: `LoaderOptimizationAttribute.hpp`'s twelve `LoaderOptimization` mentions
are all the *type* name; **no production site names a deprecated enumerator.**

### 3.2 Public shapes

`class ThreadStaticAttribute : public Attribute {};` — no member, no `final`, no
declared constructor; `sizeof` equals `sizeof(Attribute)` (8, one vptr).

`enum class LoaderOptimization` with five enumerators, `DomainMask = 3` aliasing
`MultiDomainHost = 3`, `DisallowBindings = 4`. Values are correct against the
frozen reference and were not touched.

---

## 4. SR-AUD-113 — remediated by stating the boundary the port already states twice

The finding names the repair itself: «Unlike the STA/MTA headers, this file also
does not state that the marker has no C++ runtime effect, so callers receive a
silent contract break rather than an explicit unsupported-feature boundary.»
That is checkable and it was checked — `STAThreadAttribute.hpp` says "This is a
marker attribute; it carries no data and has no effect in the C++ port", and
`MTAThreadAttribute.hpp` matches. `ThreadStaticAttribute.hpp` said the opposite:
"Indicates that the value of a static field is unique for each thread."

Implementing the promise is impossible, not merely expensive: attaching an
object of a class to a declaration is not something C++ can express, so no
amount of registry or `thread_local` plumbing inside this class can reach the
field a caller wanted isolated. The header now states that, states that nothing
reads the marker, names `thread_local` as the C++ facility that does what the
.NET attribute describes, shows the two-line migration, and says the class exists
so ported code naming it still compiles.

**Nothing in the compiled surface changed.** No member, no base, no `final`, no
constructor, no include; `sizeof` and `alignof` unchanged; the vtable is the
inherited `Attribute` one, untouched. No executable statement was changed.
`final` was deliberately **not** added — it would forbid derivation that compiles
today.

### 4.1 Tests — two added, none retired

The finding's observation is accurate: the three green cases construct two
markers and check inheritance.

1. **`CarriesNoDataBeyondTheAttributeBase`** — `sizeof(ThreadStaticAttribute) ==
   sizeof(Attribute)` and `is_base_of_v`. This is the mutation-sensitive pin: it
   trips on exactly the shape a future "implementation" attempt would take, a
   slot index or registry pointer hung off the class. **Mutation** (rebuilt and
   relinked): adding `int slotIndex_ = -1;` failed this case and **only** this
   case — the other four, including the pre-existing three, still passed.
2. **`MarkerDoesNotIsolateStorageButThreadLocalDoes`** — the concurrent
   static-field isolation case the audit says is missing: a `static` counter and
   a `thread_local` counter, one joined thread, and the assertion that the marked
   static is **shared** (6, having seen this thread's 5) while the `thread_local`
   is not (the other thread starts from its own 0, this thread still reads 5).
   **Labelled honestly in the source and here: this demonstrates a language
   boundary, not a mutable behaviour of this port.** No change to
   `ThreadStaticAttribute.hpp` can make it pass or fail, so it is *not* counted as
   a caught mutation — it is executable documentation of the claim the header now
   makes, which is what the audit asked for.

Nothing was added to harden the class's shape beyond emptiness, because
withdrawing or sealing the type remains a source break someone may later want to
take.

---

## 5. SR-AUD-117 — design complete, approval-bound (#2289)

### 5.1 The finding reproduces exactly as filed

`DomainMask` and `DisallowBindings` carry Doxygen `@deprecated` prose and no
`[[deprecated]]`; both compile silently; the eleven focused tests exercise both
without any diagnostic expectation. Frozen reference basis: .NET marks both with
`Obsolete` (`System/LoaderOptimization.cs:6-16`).

### 5.2 Premise correction — this is not a two-value singleton

The finding is scoped to one enum. Measured across the production headers, the
"deprecated in prose, silent to the compiler" pattern is **five sites in three
files across two modules, in two different declaration shapes**:

| Site | Shape | Module |
|---|---|---|
| `LoaderOptimization::DomainMask` | enumerator | core |
| `LoaderOptimization::DisallowBindings` | enumerator | core |
| `AppDomain::GetCurrentThreadId()` | member function | core |
| `CultureTypes::WindowsOnlyCultures` | enumerator | globalization |
| `CultureTypes::FrameworkCultures` | enumerator | globalization |

There is **no `[[deprecated]]` anywhere in this repository** — not one occurrence
in any module `include/`, `src/` or test tree. So the question SR-AUD-117 really
asks is a repository-wide policy question — *does this port map .NET `Obsolete`
onto C++ `[[deprecated]]`, and if so, at all five sites?* — and answering it for
two enumerators alone would leave the port inconsistent with itself. #2289 is
scoped to all five.

### 5.3 Measured, not assumed: what `[[deprecated]]` actually does here

`build-probe/2287_probe1_deprecated_enumerator.cpp`, compiled with the
repository's own `-Wall -Wextra -Werror` plus the fixture checker's `-Wpedantic`:

1. **The placement matters.** `[[deprecated("…")]] DomainMask = 3,` is a **hard
   syntax error** — "expected identifier before '[' token". The attribute
   attaches *after* the enumerator name: `DomainMask [[deprecated("…")]] = 3,`.
   A design that got this backwards would have looked trivial and failed
   immediately.
2. **The declaration itself is clean** — no diagnostic at the enum, `-Wpedantic`
   included.
3. **A use is an error, not a warning, under this repository's flags:**
   `error: 'System::LoaderOptimization::DomainMask' is deprecated: Use
   MultiDomainHost instead. [-Werror=deprecated-declarations]`.
4. **Deprecation is per name, not per value.** `MultiDomainHost` — the same
   underlying `3` — produces nothing. So the alias can be deprecated without
   touching the live spelling.

Point 3 is the whole approval boundary, and it is measured rather than feared:
any consumer that names either enumerator and builds warnings-as-errors — which
is exactly what this repository does, and what `CLAUDE.md` rule 1 requires of it
— stops compiling. Zero first-party production consumers does not license that;
downstream consumers exist and this batch may not inspect them.

### 5.4 The selected repair, priced

**Route A (selected, blocked on #2289):** move all five sites to
`[[deprecated("…")]]`, keeping the Doxygen prose beside it.
- In-repo migration: **one file, nine sites, five cases** —
  `LoaderOptimizationTests.cpp` — wrapped in
  `#pragma GCC diagnostic push / ignored "-Wdeprecated-declarations" / pop`, the
  suppression pattern this repository's tests already use twice
  (`BitArrayVersionWideningTests.cpp`, `ListIndexerProxyTests.cpp`). The other
  three sites' tests must be re-measured when the ticket runs.
- New evidence: a `test/consumer/*_negative.cpp` fixture, one site per deprecated
  name, each with a `// NEGATIVE(...)`: `is deprecated` marker. This is what the
  audit's own note asks for — "the fixture does not use a compiler
  warning-as-error consumer to distinguish documentation from a language
  diagnostic" — and the repository's fixture checker compiles exactly that way.
- Cost: any consumer naming a deprecated name under `-Werror` breaks.

**Route B (rejected):** deprecate only the two enumerators SR-AUD-117 names.
Same breakage, and it leaves three sites of the identical pattern behind,
entrenching the inconsistency rather than closing it.

**Route C (rejected as a closure):** documentation only. It cannot make the
finding false — the finding *is* "emits no C++ compiler diagnostic". Taken as a
partial measure it is still worth having, and it was taken (§5.5).

### 5.5 What was done anyway, and what it does not close

The header now says, in a `@warning`, that the `@deprecated` tags are prose and
not a compiler diagnostic, that .NET's `Obsolete` does produce one, that adding
`[[deprecated]]` is measured to turn every use into a hard error under `-Werror`
and therefore needs a decision, that #2289 covers the other three sites too, and
that a reader should treat the tags as advice the compiler will not repeat.

This is correct under either outcome of #2289 *today*, and #2289's implementation
must revise it when it lands. **It does not close SR-AUD-117**: no declaration
changed, so a caller still receives no diagnostic. The finding stays open, as
`confirmed (design-complete)`.

---

## 6. Compatibility

| Dimension | SR-AUD-113 (#2288) | SR-AUD-117 (§5.5 note) |
|---|---|---|
| Public source | none | none |
| ABI / symbols | none | none |
| Layout / vtable | none — `sizeof` equals `sizeof(Attribute)`, asserted by a new case | none — five enumerators, same values |
| `noexcept` | none | none |
| Includes / component graph | none | none |
| Behaviour | none — no executable statement changed | none |

## 7. Validation

`build/` only, `--parallel 2`. `SharpRuntimeTests_Core_Base` rebuilt, relinked
and rerun for every mutation. One throwaway compile probe under
`build-probe/` (`-fsyntax-only`, single translation unit, one job), deleted once
§5.3 was transcribed here. **No sanitizer run**: both units are public API
documentation plus tests, no executable statement changed, and there is no
lifetime, arithmetic or memory-safety question anywhere in them — a sanitizer
pass would be theater. **Selective components not rerun**: no component,
dependency, module, boundary or catalogue entry changed.

## 8. Disposition

| Finding | Before | After | Owner |
|---|---|---|---|
| SR-AUD-113 | confirmed | **remediated** | #2287 review, #2288 implementation |
| SR-AUD-117 | confirmed | **confirmed (design-complete)** | #2287 review/design, #2289 `needs_user` |

No new `SR-AUD-*` identifier. No CCF minted. No public source break taken, and
the one that SR-AUD-117 needs is recorded as a decision rather than assumed away.
