# Audit: `modules/core/include/System/LoaderOptimization.hpp`

## Metadata

- Audit status: AUDITED (44-line enum declaration, fully read).
- Validation: `LoaderOptimizationTests.*` passed 11/11 in the 77-test focused
  Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/LoaderOptimization.cs:6-16`.

## SR-AUD-117 — low — Deprecated LoaderOptimization values have documentation-only deprecation and emit no C++ compiler diagnostic

Current .NET marks `DomainMask` and `DisallowBindings` with `Obsolete`, so
source use produces a compiler diagnostic.  The C++ declarations use Doxygen
`@deprecated` prose only (`LoaderOptimization.hpp:29-41`), not C++
`[[deprecated]]`; both values compile silently.  The focused tests exercise
both deprecated values without any diagnostic expectation.

### Status: STILL CONFIRMED — DESIGN-COMPLETE, approval-bound (#2287 review/design, #2289 `needs_user`, 2026-08-11)

The finding **reproduces exactly as filed**: both values carry Doxygen prose and
no `[[deprecated]]`, both compile silently, and the focused tests exercise them
with no diagnostic expectation.

**Premise correction — this is not a two-value singleton.** Measured across every
production header, the "deprecated in prose, silent to the compiler" pattern is
**five sites in three files across two modules, in two declaration shapes**:
`LoaderOptimization::DomainMask`, `LoaderOptimization::DisallowBindings`,
`AppDomain::GetCurrentThreadId()` (a member function),
`CultureTypes::WindowsOnlyCultures` and `CultureTypes::FrameworkCultures`. There
is **no `[[deprecated]]` anywhere in this repository**. So the real question is a
repository-wide policy one — does this port map .NET `Obsolete` onto C++
`[[deprecated]]`, and if so at all five sites? — and answering it for two
enumerators alone would leave the port inconsistent with itself. #2289 is scoped
to all five.

**Measured, not assumed** (`build-probe/2287_probe1_deprecated_enumerator.cpp`,
compiled under this repository's `-Wall -Wextra -Werror` plus `-Wpedantic`, then
deleted once transcribed): (1) the attribute must follow the enumerator name —
`DomainMask [[deprecated("…")]] = 3,`; the leading spelling is a **hard syntax
error**; (2) the declaration itself draws no diagnostic; (3) **a use is an error,
not a warning**, under these flags —
`error: 'System::LoaderOptimization::DomainMask' is deprecated: … [-Werror=deprecated-declarations]`;
(4) deprecation is per **name**, not per value — `MultiDomainHost`, the same
underlying `3`, produces nothing, so the alias can be deprecated without touching
the live spelling.

Point (3) is the approval boundary, measured rather than feared: any consumer
naming either enumerator and building warnings-as-errors — which is exactly what
this repository does, and what `CLAUDE.md` rule 1 requires — stops compiling.
Zero first-party production consumers of the two names (measured: **0**
production sites; **9** sites in one test file, `LoaderOptimizationTests.cpp`)
does **not** license that, because downstream consumers exist and this batch may
not inspect them.

**Selected repair, recorded in full for #2289:** move all five sites to
`[[deprecated("…")]]` beside the retained prose; migrate the one in-repo test
file with the `#pragma GCC diagnostic push/ignored/pop` pattern this repository's
tests already use twice; and add a `test/consumer/*_negative.cpp` fixture with one
site per deprecated name, which is precisely the "compiler warning-as-error
consumer" the note below asks for. Deprecating only the two values this finding
names was rejected: same breakage, three sites of the identical pattern left
behind.

**What was done anyway, and what it does not close:** the header now states in a
`@warning` that the `@deprecated` tags are prose rather than a compiler
diagnostic, that .NET's `Obsolete` does produce one, that `[[deprecated]]` is
measured to make every use a hard error under `-Werror` and therefore needs a
decision, and that #2289 covers the other three sites. **No declaration changed,
so a caller still receives no diagnostic and the finding is not closed.**

**Not a family with SR-AUD-113**, ranked beside it: shared shape (a small
`modules/core` attribute/enum header with no first-party production consumer), not
a shared cause — and the causes are opposite in the respect that decides the
repair. SR-AUD-113 exists because C++ offers **no** mechanism for what its header
promised; this finding exists because C++ offers **exactly** the mechanism and
the port declined to use it. No CCF minted.
`docs/CoreMarkerAttributeAndDeprecationPlan.md`.

## Other missing assertions and diagnostics

- Numeric values, including the `DomainMask == MultiDomainHost` alias, are
  correctly covered, but the fixture does not use a compiler warning-as-error
  consumer to distinguish documentation from a language diagnostic.
- The enum cannot influence loader domain behavior.  Modern .NET itself has a
  single AppDomain, and this port documents the corresponding legacy status.

## Final assessment

Numeric compatibility is correct, but callers receive no compile-time warning
for the two publicly deprecated choices.  No source or test was modified during
this audit.
