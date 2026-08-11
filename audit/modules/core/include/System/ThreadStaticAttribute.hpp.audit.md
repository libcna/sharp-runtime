# Audit: `modules/core/include/System/ThreadStaticAttribute.hpp`

## Metadata

- Audit status: AUDITED (14-line marker declaration, fully read with its
  dedicated fixture).
- Validation: `ThreadStaticAttributeTest.*` passed 3/3 in the 18-test marker
  attribute filter on 2026-07-26.
- Reference basis: local .NET `ThreadStaticAttribute.cs` field-isolation
  contract and C++ storage-duration semantics.

## SR-AUD-113 — medium — ThreadStaticAttribute has no attachment or storage mechanism and cannot provide its documented per-thread field value

The C++ class is an ordinary instantiable `Attribute` subclass with no macro,
compiler attribute, registry, or `thread_local` integration
(`ThreadStaticAttribute.hpp:9-13`). Instantiating it cannot affect any static
field, yet its public documentation says it indicates a value unique to each
thread. Current .NET applies the metadata attribute to a static field and the
runtime supplies separate storage.

The three green tests only construct two marker objects and check inheritance;
they do not declare a static field or create a second thread. Unlike the
STA/MTA headers, this file also does not state that the marker has no C++
runtime effect, so callers receive a silent contract break rather than an
explicit unsupported-feature boundary.

### Status: REMEDIATED (#2287 review, #2288 implementation, 2026-08-11)

Repaired by **the repair this finding itself names**: "Unlike the STA/MTA
headers, this file also does not state that the marker has no C++ runtime
effect." That is checkable, and it was checked — `STAThreadAttribute.hpp` says
"This is a marker attribute; it carries no data and has no effect in the C++
port", `MTAThreadAttribute.hpp` matches, and this header said the opposite.

Implementing the documented behaviour is impossible, not merely expensive:
attaching an object of a class to a declaration is not something C++ can express,
so no registry or `thread_local` plumbing *inside* this class can reach the field
a caller wanted isolated. The header now states that it carries no data and has
no effect, that nothing in the repository reads it, that C++ has no attachment
mechanism, that `thread_local` on the declaration is the facility which does what
the .NET attribute describes (with the two-line migration shown), and that the
class exists so ported code naming `System::ThreadStaticAttribute` still
compiles. The silent contract break is now an explicit boundary.

**Nothing in the compiled surface changed** — no member, base, `final`,
constructor or include; `sizeof` still equals `sizeof(Attribute)`; the vtable is
the inherited one. No executable statement was changed. `final` was deliberately
**not** added: it would forbid derivation that compiles today.

**Consumer inventory re-measured, not inherited:** zero production consumers;
three test files, five cases (`ThreadStaticAttributeTests.cpp` 3,
`Batch3TypeTests.cpp` 1, `SystemAttributeTests.cpp` 1).

**Tests: two added, none retired**, answering both bullets below.
`CarriesNoDataBeyondTheAttributeBase` pins `sizeof(ThreadStaticAttribute) ==
sizeof(Attribute)` and `is_base_of_v` — the mutation-sensitive pin, which trips
on exactly the shape a future "implementation" attempt would take; adding
`int slotIndex_ = -1;` failed this case and **only** this case, the three
pre-existing ones included.
`MarkerDoesNotIsolateStorageButThreadLocalDoes` is the concurrent static-field
isolation case: a `static` counter and a `thread_local` counter across one joined
thread, asserting the static is shared (6, having seen this thread's 5) and the
`thread_local` is not (the other thread starts from its own 0, this thread still
reads 5). It is **labelled in the source as a language-boundary demonstration,
not a behaviour pin** — no change to this header can make it pass or fail — and
is therefore not counted as a caught mutation.

**Verdict on the ranking that paired this with SR-AUD-117: not a family**, and
their causes are opposite in the respect that decides the repair. This finding
exists because C++ offers **no** mechanism; SR-AUD-117 exists because C++ offers
**exactly** the mechanism (`[[deprecated]]`) and the port declined to use it. Its
repair breaks builds and is approval-bound (#2289); this one costs nothing.

**The genuine family candidate is SR-AUD-115**, not SR-AUD-117: `ObsoleteAttribute`
"cannot mark a declaration or produce its documented compiler diagnostic" is the
same mechanism — an `Attribute`-derived class in a language with no attribute
attachment cannot deliver the effect .NET's attribute delivers — with a different
promised effect. Recorded, not acted on: SR-AUD-115 is outside this batch, its
header carries a second finding (SR-AUD-116), and **minting a CCF over the pair
needs authority this batch does not have**. `docs/CoreMarkerAttributeAndDeprecationPlan.md`.

## Other missing assertions and diagnostics

- No API provides an equivalent `thread_local` declaration pattern or rejects
  attempts to use this class as one.
- Tests omit concurrent/static-field isolation, default initialization,
  inheritance policy, and reflection/metadata availability.

## Final assessment

The marker compiles but cannot implement the behavior its public comment
describes. No source or test was modified during this audit.
