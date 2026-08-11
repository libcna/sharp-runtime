# Audit: `modules/core/include/System/CrashReason.hpp`

## Metadata

- AUDITED: 26-line public enum declaration, fully read.
- Validation: `CrashReasonTests.*` passed 5/5 in the combined 17-test
  `CrashReasonTests.*:DateTimeKindTests.*:DayOfWeekTests.*` Core.Base filter
  on 2026-07-26.
- Reference basis: local NativeAOT
  `System/CrashInfo.cs:13-22,116-124`.

## SR-AUD-127 — medium — CrashReason publishes a private nested NativeAOT implementation enum as a public System API

The counterpart in current .NET is `internal` and nested as
`System.CrashInfo.CrashReason`; it is only used to encode NativeAOT triage
data.  This header instead publishes an independently includable
`System::CrashReason` in the Core.Base public include tree.  Repository search
finds no first-party production consumer — only the dedicated test — so the
public surface has no current runtime role to justify the visibility or lost
nesting.

The numerical values happen to match, but the public name reserves a `System`
identifier for an implementation detail and promises stable consumer access
where .NET intentionally makes none.  Move it into an internal NativeAOT
crash-diagnostic implementation or explicitly designate/document it as a
project-specific public diagnostic type rather than calling it a .NET
counterpart.

### Status: REMEDIATED (#2279 review, #2280 implementation, 2026-08-11)

Repaired by **this finding's own second alternative**, taken deliberately and
recorded as such. The first alternative — moving the enum into an internal
implementation — withdraws a published, independently includable `System::`
header from the `Core.Base` include tree, which is a public source break and an
approval boundary this batch does not cross; downstream consumers exist and this
batch may not inspect them.

The finding's substantive complaints are both statements about what the header
says — it "promises stable consumer access where .NET intentionally makes none"
and "provides no visibility or NativeAOT-only diagnostic explaining why an
internal runtime concept is exposed from Core.Base" — and both are now false. The
doc-comment states that .NET publishes no `System.CrashReason`, that the
counterpart is the `internal` nested `System.CrashInfo.CrashReason`, that this
enumeration is published on the project's own authority so that a later change to
.NET's internal enum is not by itself a defect here, that there is no first-party
production consumer so the constants are a porting vocabulary rather than a
runtime contract, why the type stays published, and that the enumeration is not
closed over its underlying type.

**Nothing in the compiled surface changed** — no enumerator, value, underlying
type, name or include — so there is no ABI, layout, vtable, `noexcept`, symbol or
component-dependency consequence.

The consumer inventory was re-measured rather than inherited: **zero** production
consumers, one test file.

The missing vectors this report lists were added, 5 tests → 9, none retired. The
"no full pairwise-distinctness vector" observation was accurate: the five old
tests check each enumerator once and compare **two of the six** pairs, so two
enumerators could have collided unnoticed. Added: all six pairs, contiguity from
zero, the `int` underlying type and scopedness, and an unlisted-value vector for
`4` and `-1`. Two mutations, both caught by the new tests as well as an old one.

**Verdict on the ranking that paired this with SR-AUD-137: not a family.** They
share a characteristic — a .NET-internal type republished publicly with no
first-party consumer — not a cause: this one is republished *verbatim*, that one
with a *different* shape plus a false source-compatibility claim, and SR-AUD-137
has no available compatible repair (#2281, `needs_user`). No CCF minted; the same
characteristic also runs through SR-AUD-124/125/126/128/129/136.
`docs/CoreCrashReasonAndUnityHolderPlan.md`.

## Other missing assertions and diagnostics

- No production crash/FailFast path consumes the enum, so no test validates
  serialization into a triage record or mapping from native failure reasons.
- The header provides no visibility or NativeAOT-only diagnostic explaining
  why an internal runtime concept is exposed from Core.Base.
- No invalid-value or full pairwise-distinctness vector exists.

## Final assessment

The constant values match the internal reference but its public placement is
the confirmed SR-AUD-127 contract drift. No source or test was modified during
this audit.
