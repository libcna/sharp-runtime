<!-- SPDX-License-Identifier: MIT -->

# `ObsoleteAttribute` — SR-AUD-115 / SR-AUD-116 review

Tickets: **#2293** (review), **#2294** (SR-AUD-115 implementation),
**#2295** (SR-AUD-116, `needs_user`). Date: 2026-08-11. The audit numbering
stays frozen at 364, **no new `SR-AUD-*` identifier was created** and **no CCF
was minted**.

---

## 1. Why these two were looked at together

They are the only two findings on `modules/core/include/System/ObsoleteAttribute.hpp`,
and #2287 named SR-AUD-115 as the genuine mechanism-boundary relative of
SR-AUD-113. Both parts of that framing were checked rather than inherited.

## 2. Verdict — they share a header, not a cause

| | SR-AUD-115 | SR-AUD-116 |
|---|---|---|
| Claim | the attribute "cannot mark a declaration or produce its documented compiler diagnostic" | the three nullable `string?` properties collapse into indistinguishable empty `std::string` |
| Kind of divergence | **mechanism boundary** — the doc-comment promises a compiler effect the object cannot have | **representation boundary** — the port has no state for "absent" |
| Does C++ offer the mechanism? | **No.** An object of a class cannot attach to a declaration | **Yes.** `std::optional<std::string>`, or a presence flag, expresses it exactly |
| Cost of the faithful repair | **zero** — it is documentation, because nothing else is possible | **a public source break or a shape divergence** — measured in §5 |
| Disposition | **remediated** (#2294) | **confirmed (design-complete)**, approval-bound (#2295) |

This is the same split shape #2287 found in SR-AUD-113 / SR-AUD-117, one file
later: one finding exists because the language has *no* mechanism, the other
because the language has one whose adoption costs something a user must agree
to. **115 is the analogue of 113; 116 is the analogue of 117.** Repairing either
teaches nothing about the other, and no CCF is warranted — a shared header and a
shared "no first-party consumer" shape are not a shared cause.

### 2.1 The SR-AUD-113 analogy held, and SR-AUD-115 does **not** run through #2289

#2287 predicted that SR-AUD-115 is "the same mechanism as SR-AUD-113 … only the
promised effect differs". That is confirmed: in both, an `Attribute`-derived
object in a language with no attribute attachment cannot deliver what .NET's
attribute delivers, and the doc-comment described the effect as though it could.

The obvious worry — that SR-AUD-115 is really the `[[deprecated]]` question and
therefore blocked behind **#2289** — was tested and is false, for two
independent reasons:

1. **`ObsoleteAttribute` is not one of #2289's five sites.** Those are five
   *declarations* carrying Doxygen `@deprecated` prose
   (`LoaderOptimization::DomainMask`, `::DisallowBindings`,
   `AppDomain::GetCurrentThreadId()`, `CultureTypes::WindowsOnlyCultures`,
   `::FrameworkCultures`). `ObsoleteAttribute` is the *class that carries the
   metadata*, and it is deprecated-prose-free.
2. **Neither outcome of #2289 changes anything here.** Marking all five
   declarations `[[deprecated]]` would still leave an `ObsoleteAttribute` object
   inert; refusing to mark them likewise. There is no edit to any declaration
   anywhere that makes constructing this object emit a diagnostic.

So SR-AUD-115 is closable now, by the only repair that exists, and #2289 stays
untouched and undecided. **No `[[deprecated]]` was added to any production
declaration by this batch.** The one `[[deprecated]]` introduced is a
file-local function inside `ObsoleteAttributeTests.cpp` (§4.2), which no
consumer can name and which changes no public surface.

---

## 3. Measured state

### 3.1 Consumer inventory — measured, not inherited

`grep -rn ObsoleteAttribute` over `modules/*/include`, `modules/*/src`,
`modules/*/tests`, `bench/`, `test/` and `tests/`:

| Symbol | Production consumers | Test consumers |
|---|---:|---|
| `System::ObsoleteAttribute` | **0** | **2 files, 14 cases** — `ObsoleteAttributeTests.cpp` (6, suite `ObsoleteAttributeTest`), `SystemAttributeTests.cpp` (8, suite `ObsoleteAttributeTests`) |

Physically corroborated: an incremental build after the header edit recompiled
exactly two translation units, both of them those test files. The header is
nonetheless part of `Core.Base`'s public include tree, so **a downstream
consumer can name every member of it**, and the zero first-party count licenses
nothing.

### 3.2 Public shape and layout, measured

`class ObsoleteAttribute : public Attribute` with four data members —
`std::string message_`, `bool isError_`, `std::string diagnosticId_`,
`std::string urlFormat_` — three constructors, four getters, two setters, no
`final`, no `.cpp`.

| | bytes |
|---|---:|
| `sizeof(Attribute)` | 8 |
| `sizeof(std::string)` | 32 |
| **`sizeof(ObsoleteAttribute)`** | **112**, `alignof` 8 |

112 is `8 + 32 + (1 + 7 padding) + 32 + 32`. The seven padding bytes beside
`isError_` matter to §5 and were measured, not assumed.

---

## 4. SR-AUD-115 — remediated by stating the boundary

### 4.1 The finding reproduces exactly as filed, and its repair is documentation

`build-probe/2293_probe2_mechanism.cpp`, compiled with this repository's own
`-Wall -Wextra -Werror` plus the fixture checker's `-Wpedantic`, then deleted:

1. **The finding's own claim, confirmed.** A `const ObsoleteAttribute a("Use
   modern() instead.", true);` describing `legacy()` compiles with **no
   diagnostic**, and `legacy()` stays callable. `isError = true` — which the
   header said "makes use a compile-time error" — changed nothing.
2. **`[[deprecated("…")]]` on the same declaration is what the compiler reacts
   to:** `error: 'int legacyMarked()' is deprecated: Use modern() instead.
   [-Werror=deprecated-declarations]`.
3. **Premise correction the finding does not state, and it survives #2289.** The
   warning/error choice is **not a property of the declaration**. The identical
   `[[deprecated]]` declaration yields `warning: … [-Wdeprecated-declarations]`
   when compiled with `-Wno-error=deprecated-declarations`. Severity is chosen
   by the compiler's flags. So `IsError` — the one member whose whole documented
   purpose is to pick warning versus error — **has no C++ counterpart even if
   #2289 is approved**. The finding says the object cannot "produce its
   documented compiler diagnostic"; fact 3 says the *distinction* it encodes is
   not expressible either.

Implementing the promise is impossible, not merely expensive: attaching an
object of a class to a declaration has no C++ spelling, so nothing done inside
this class can reach the declaration a caller wanted marked. The repair is
therefore the same one SR-AUD-113 took, and it makes the finding false because
the finding is a false-claim defect: the header now states that constructing one
deprecates nothing, that nothing in the repository reads any of the four values,
that `[[deprecated]]` on the declaration is the C++ facility (migration shown),
that severity is a compiler flag rather than a declaration property, that #2289
owns whether this port marks its own obsolete declarations and that neither
outcome changes this class, that .NET's `sealed`/`AttributeUsage` target and
inheritance restrictions are not expressible here, and that the class exists so
ported code naming it still compiles.

`final` was deliberately **not** added, on #2288's reasoning: it would reject
derivation that compiles today. The `AttributeUsage` restrictions the audit lists
under "target and non-inheritance" are documented for the same reason they cannot
be tested — there is no declaration to restrict.

### 4.2 Tests — four added, none retired

`ObsoleteAttributeTest` goes 6 → 10.

1. **`CarriesItsFourDeclaredMembersAndNoSideChannel`** — the mutation-sensitive
   pin. `sizeof(ObsoleteAttribute)` equals `sizeof(Attribute) + 3 *
   sizeof(std::string) + sizeof(bool)` rounded up to `alignof`, computed rather
   than hardcoded, plus `is_base_of_v`. It trips on exactly the shape an attempt
   to "implement" the attachment would take — a registry pointer or slot handle.
   **Mutation M1** (rebuilt and relinked): adding `void* registry_ = nullptr;`
   failed this case and **only** this case — 17 of 18, including all six
   pre-existing, still passed. Stated honestly: a further *`bool`* would fit in
   the measured seven padding bytes and would not trip it; a pointer-sized side
   channel, which is what a registry needs, does.
2. **`AttributeObjectDeprecatesNothingButDeprecatedDoes`** — the audit's missing
   "never compile a deprecated declaration" case, made executable: an
   `ObsoleteAttribute` with `isError = true` describing a file-local function
   that is then called normally, beside a `[[deprecated]]` file-local function
   called under a `#pragma GCC diagnostic push / ignored
   "-Wdeprecated-declarations" / pop` — the suppression pattern this
   repository's tests already use. **Labelled in the source and here as a
   language-boundary demonstration, not a behaviour pin**: no edit to
   `ObsoleteAttribute.hpp` can make it pass or fail, so it is *not* counted as a
   caught mutation. **Mutation M2** is real and is counted separately: deleting
   the suppression fails the build with `error: … is deprecated: … [-Werror=
   deprecated-declarations]`, which is what makes the demonstration evidence
   rather than decoration.
3. **`CopyAndMovePreserveEveryComponent`** — the report's missing copy/move
   case: every component travels, and a copy is independent of its source.
4. **`TextComponentsAreByteTransparent`** — the report's missing UTF-8 case:
   non-ASCII message and url-format text round trip byte-for-byte and are not
   re-encoded or truncated at a lead byte.

**Deliberately not added:** any case pinning that a default-constructed
attribute and an explicitly empty one are indistinguishable. That is precisely
the state SR-AUD-116 disputes, and #2295 may change it; the two pre-existing
`.empty()` assertions are left as they are rather than deepened.

---

## 5. SR-AUD-116 — design complete, approval-bound (#2295)

### 5.1 The finding reproduces exactly as filed

Measured (`build-probe/2293_probe1_layout.cpp`, deleted): `ObsoleteAttribute
def;` and `ObsoleteAttribute empty(std::string{});` produce
`getMessageProperty()` values that compare **equal**, and every getter returns a
`const std::string&` into the object. There is no state anywhere in the class in
which "the caller never supplied a message" could be recorded. Reference basis:
.NET's `Message`, `DiagnosticId` and `UrlFormat` are `string?`, per the frozen
report's `System/ObsoleteAttribute.cs:9-40` (`/rv` is absent; nothing beyond the
frozen basis was invented).

The same boundary appears on the way in as well as out, which the finding does
not say: `explicit ObsoleteAttribute(const std::string&)` and both setters take
`const std::string&`, so a caller cannot *supply* null either, and cannot return
a component to the absent state once set.

### 5.2 The options, priced by measurement

`build-probe/2293_probe3_repr.cpp` built both candidate shapes and compiled the
four call shapes that appear in the existing suites against them.

| | **A — `optional<string>`** | **B — presence flags beside `isError_`** | **C — documentation only** |
|---|---|---|---|
| Storage | `std::optional<std::string>` ×3 | `std::string` ×3 + `bool` ×3 | unchanged |
| Getters | return `const std::optional<std::string>&` | unchanged; **add** `getHasMessageProperty()` etc. | unchanged |
| Distinguishes absent from empty | **yes** | **yes** | **no** |
| `sizeof` | 112 → **136** (+24) | **112 — unchanged**, the flags fit the measured padding | 112 |
| Public source break | **yes, wide** (§5.3) | **none** — purely additive | none |
| Matches .NET's shape | closest available | no — .NET has no `HasMessage` | no |
| Closes SR-AUD-116 | yes | yes | **no** |

### 5.3 The source break in option A, measured rather than feared

Of the four call shapes that exist today, three stop compiling:

| Call shape | Under option A |
|---|---|
| `attr.getMessageProperty().empty()` | **error** — `optional<string>` has no member `empty` |
| `const std::string& s = attr.getMessageProperty();` | **error** — no such conversion |
| `f(attr.getMessageProperty())` where `f` takes `const std::string&` | **error** — no matching call |
| `EXPECT_EQ(attr.getMessageProperty(), "m")` | compiles — `optional`'s heterogeneous `operator==` |

The first three are the ordinary ways to use a returned string, and the first is
written twice in the current suites. Any downstream that touches a component as a
string breaks. Zero first-party production consumers does **not** license that:
the header is public in `Core.Base` and downstream consumers exist and were not
inspected by this batch.

Option B costs no bytes and breaks no source, and that is exactly why it must not
be chosen unilaterally: it buys compatibility by shipping a shape .NET does not
have, permanently, in a public header. Which of "faithful to .NET" and
"compatible with existing callers" wins here is a project decision, not a
reviewer's.

### 5.4 What was done anyway, and what it does not close

The header now states that .NET's three properties are nullable, that this port
stores non-nullable `std::string` and therefore cannot tell an absent value from
an empty one, that .NET's parameterless attribute leaves `Message` null, and that
the representation is under decision at #2295. This is true under every outcome.

**It does not close SR-AUD-116**: the states are still collapsed, and a caller
still cannot distinguish them. The finding stays open, as
`confirmed (design-complete)`.

---

## 6. Compatibility

| Dimension | SR-AUD-115 (#2294) | SR-AUD-116 (§5.4 note) |
|---|---|---|
| Public source | none | none |
| ABI / exported symbols | none — header-only, all bodies already inline | none |
| Layout / vtable | none — `sizeof` still 112, `alignof` 8, asserted by a new case; vtable inherited from `Attribute`, untouched | none |
| `noexcept` | none | none |
| Includes / component graph | none | none |
| Behaviour | none — no executable statement changed | none |

The only compiled change anywhere in this unit is inside the test binary.

## 7. Validation

`build/` only, `cmake --build build --parallel 2`, maximum two jobs.
`SharpRuntimeTests_Core_Base` rebuilt, relinked and rerun for each mutation;
`ObsoleteAttribute*` reads 18/18 (14 → 18). Three throwaway probes under
`build-probe/` (`2293_probe1_layout`, `2293_probe2_mechanism`,
`2293_probe3_repr`), each a single translation unit compiled one at a time and
deleted once §3.2, §4.1, §5.1 and §5.2 were transcribed here.

**No sanitizer run.** Classified before deciding: SR-AUD-115's unit is public API
documentation plus tests with no executable statement changed, and its one
compile-domain claim is settled by a compiler diagnostic, which is what M2
exercises; SR-AUD-116's unit is documentation only. There is no lifetime,
arithmetic, UB or memory-safety question in either, so a sanitizer pass would be
theater. **Selective components not rerun**: no component, dependency, module
boundary or catalogue entry changed, and the header's include set is unchanged.

## 8. Disposition

| Finding | Before | After | Owner |
|---|---|---|---|
| SR-AUD-115 | confirmed | **remediated** | #2293 review, #2294 implementation |
| SR-AUD-116 | confirmed | **confirmed (design-complete)** | #2293 review/design, #2295 `needs_user` |

No new `SR-AUD-*` identifier. No CCF minted — CCF-021 and CCF-022 remain
unminted, and §2 states why this pair does not warrant one. No public source
break taken, and the one SR-AUD-116 may need is recorded as a decision rather
than assumed away. #2289 is untouched and still `needs_user`.
