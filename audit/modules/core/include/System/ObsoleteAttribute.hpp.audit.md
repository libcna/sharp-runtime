# Audit: `modules/core/include/System/ObsoleteAttribute.hpp`

## Metadata

- Audit status: AUDITED (47-line value attribute, fully read with its
  dedicated fixture).
- Validation: `ObsoleteAttributeTest.*` passed 6/6 in the 77-test focused
  Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/ObsoleteAttribute.cs:9-40`.

## SR-AUD-115 — medium — ObsoleteAttribute stores an error flag but cannot mark a declaration or produce its documented compiler diagnostic

`ObsoleteAttribute` is an ordinary runtime object with no declaration wrapper,
compiler attribute, or registry (`ObsoleteAttribute.hpp:11-45`).  Its public
comment says `isError = true` makes use a compile-time error (`:26-30`), but
constructing or mutating the object cannot alter use of any C++ symbol.  .NET
attaches this metadata to a declaration and its compiler emits a warning or
error based on `IsError`.

The green tests assert only stored strings and booleans.  They never compile a
deprecated declaration, distinguish warning from error, or test target and
non-inheritance restrictions.

### Status: REMEDIATED (#2293 review, #2294 implementation, 2026-08-11)

**The finding reproduces exactly as filed**, measured under this repository's own
`-Wall -Wextra -Werror` plus `-Wpedantic`: a `const ObsoleteAttribute a("Use
modern() instead.", true);` describing `legacy()` draws no diagnostic and leaves
`legacy()` callable. `isError = true` — which this header said "makes use a
compile-time error" — changed nothing.

Implementing the documented behaviour is impossible, not merely expensive:
attaching an object of a class to a declaration has no C++ spelling, so nothing
done inside this class can reach the declaration a caller wanted marked. This is
the **same mechanism boundary as SR-AUD-113**, as #2287 predicted, and the repair
is the same one: the header now states that constructing one deprecates nothing,
that no code in this repository reads any of the four values, that
`[[deprecated]]` written on the declaration is the C++ facility that emits the
diagnostic (migration shown), that .NET's `sealed` and `AttributeUsage` target
and inheritance restrictions are not expressible where there is no declaration to
carry them, and that the class exists so ported code naming
`System::ObsoleteAttribute` still compiles.

**Premise correction, measured, that outlives the repair:** the warning/error
choice is **not a property of the declaration** in C++. One `[[deprecated]]`
declaration yields `error: … [-Werror=deprecated-declarations]` under this
repository's flags and `warning: … [-Wdeprecated-declarations]` under
`-Wno-error=deprecated-declarations`. Severity is a compiler flag. So `IsError`,
the member whose whole documented purpose is to choose between the two, has no
C++ counterpart **even if #2289 is approved**.

**This finding does not run through #2289**, which was tested rather than
assumed. `ObsoleteAttribute` is not one of that ticket's five prose-deprecated
declarations — it is the class that carries the metadata — and neither outcome of
#2289 changes anything here, because an `ObsoleteAttribute` object stays inert
either way. No `[[deprecated]]` was added to any production declaration.

**Nothing in the compiled surface changed** — no member, base, `final`,
constructor, setter or include; `sizeof` still 112, `alignof` 8; the vtable is
the inherited `Attribute` one. No executable statement was changed. `final` was
deliberately **not** added: it would reject derivation that compiles today.

**Consumer inventory measured, not inherited:** zero production consumers —
corroborated physically, since the header edit recompiled exactly two translation
units, both test files — and 2 test files, 14 cases
(`ObsoleteAttributeTests.cpp` 6, `SystemAttributeTests.cpp` 8). The header is
public in `Core.Base`, so the zero first-party count licenses nothing.

**Tests: four added, none retired**, answering this section's third paragraph and
two of the "Other missing assertions" bullets below.
`CarriesItsFourDeclaredMembersAndNoSideChannel` is the mutation-sensitive pin —
`sizeof` equals the declared members rounded to `alignof`, computed rather than
hardcoded — and adding `void* registry_ = nullptr;` failed this case and **only**
this case, all six pre-existing ones included; stated honestly, a further `bool`
would fit the measured seven padding bytes and would not trip it, while the
pointer-sized side channel a registry needs does.
`AttributeObjectDeprecatesNothingButDeprecatedDoes` supplies the "never compile a
deprecated declaration" case: an attribute object with `isError = true` beside a
file-local `[[deprecated]]` function called under a scoped
`-Wdeprecated-declarations` suppression. It is **labelled in the source as a
language-boundary demonstration, not a behaviour pin** — no edit to this header
can make it pass or fail — so it is not counted as a caught mutation; the counted
mutation is that deleting the suppression fails the build with the diagnostic it
claims. `CopyAndMovePreserveEveryComponent` and `TextComponentsAreByteTransparent`
answer the copy/move and UTF-8 bullets.

"Distinguish warning from error" is **not** addressed by a test and cannot be:
per the premise correction above, that distinction is a compiler flag, not
anything this port can express or observe at runtime. It is documented instead.

`docs/CoreObsoleteAttributeBoundaryPlan.md`.

## SR-AUD-116 — medium — ObsoleteAttribute collapses nullable .NET string properties into indistinguishable empty strings

In current .NET `Message`, `DiagnosticId`, and `UrlFormat` are nullable
`string?` properties, so a default attribute exposes `null` and callers can
distinguish it from an explicitly supplied empty string.  This port stores
three non-nullable `std::string` fields initialized empty (`:12-15`) and
returns references to them (`:33-39`), erasing that observable state without a
documented `optional<string>` adaptation.  The test fixture enshrines empty
strings for the default `Message`, `DiagnosticId`, and `UrlFormat`.

### Status: CONFIRMED (DESIGN-COMPLETE) — approval-bound, #2295 `needs_user` (#2293 review, 2026-08-11)

**The finding reproduces exactly as filed:** `ObsoleteAttribute def;` and
`ObsoleteAttribute empty(std::string{});` yield `getMessageProperty()` values
that compare equal, and there is no state in the class in which "never supplied"
could be recorded. The boundary also exists on the way *in*, which the finding
does not say: the message constructor and both setters take
`const std::string&`, so a caller can neither supply an absent value nor return a
component to that state.

**Not a family with SR-AUD-115.** They share this header, not a cause. SR-AUD-115
is a mechanism boundary whose faithful repair is free because nothing else is
possible; this is a representation boundary whose faithful repair is entirely
possible and costs a public source break. That is the SR-AUD-113 / SR-AUD-117
split one file later. No CCF was minted.

**Options priced by probe, not argument.** Both candidate shapes were built and
the four call shapes now in the suites were compiled against them:

| | A — `optional<string>` | B — presence flags | C — documentation only |
|---|---|---|---|
| Distinguishes absent from empty | yes | yes | no |
| `sizeof` | 112 → **136** | **112, unchanged** (flags fit the measured padding) | 112 |
| Public source break | **yes, wide** | none — purely additive | none |
| Matches .NET's shape | closest available | no | no |
| Closes this finding | yes | yes | **no** |

Under option A, three of the four existing call shapes stop compiling —
`.empty()` (written twice in the current suites), binding the result to
`const std::string&`, and passing it to a function taking `const std::string&`;
only equality against a literal survives. Any downstream using a component as a
string breaks, and zero first-party production consumers does not license that.
Option B breaks nothing and costs no bytes, but buys that by shipping a shape
.NET does not have in a public header, permanently. Which of faithful-to-.NET and
compatible-with-callers wins is a project decision, so **no representation was
selected**.

**Taken anyway, because it is true under every outcome:** the header now states
that .NET's three properties are nullable, that this port stores non-nullable
`std::string` and so cannot tell an absent value from an empty one, and that
.NET's parameterless attribute leaves `Message` null. **This does not close the
finding** — the states are still collapsed. No test was added pinning the
collapse, deliberately: #2295 may change it, and the two pre-existing `.empty()`
assertions were left as they are rather than deepened.

`docs/CoreObsoleteAttributeBoundaryPlan.md` §5.

## Other missing assertions and diagnostics

- No test verifies construction with an empty message separately from default
  construction, UTF-8 diagnostics, copy/move state, or derived-type policy.
- Equal-valued object comparisons follow SR-AUD-114 rather than .NET's default
  fieldwise `Attribute.Equals` behavior.

## Final assessment

The scalar payload round-trips, but diagnostic behavior and nullable state are
not faithfully represented.  No source or test was modified during this audit.
