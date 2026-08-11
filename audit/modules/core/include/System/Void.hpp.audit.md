# Audit: `modules/core/include/System/Void.hpp`

## Metadata

- AUDITED: 35-line public placeholder value type, fully read.
- Validation: `VoidTest.*` passed 5/5 in the combined 12-test
  `VoidTest.*:UnitySerializationHolderTest.*` Core.Base filter on 2026-07-26.
  The separately pending `SystemTypesRemainingTests.cpp` has duplicate plural
  Void smoke cases and is not marked audited here.
- Reproduction: local C# probe attempting construction, `ToString`, or
  `List<System.Void>` fails with CS0673 (and construction also CS0143).
- Reference basis: local .NET `System/Void.cs:8-12` and C# compiler output.

## SR-AUD-136 — medium — Void is presented as a normal C++ value/generic type although C# forbids the documented use cases

The header says its ordinary struct permits porting generic patterns such as
`Nullable<Void>` and `Task<Void>`, and it adds instantiation, equality, and an
empty-string `ToString`. C++ tests construct values, place them in vectors, and
assign them to Nullable. The local C# compiler rejects all analogous ordinary
`System.Void` uses with CS0673: the public runtime metadata type represents the
`void` return type, not a usable C# value or generic argument. The same source
has no declared fields or methods, while this header creates a user-observable
value API.

If a C++ unit/absence type is needed, make it a clearly project-specific type
and remove the claimed C# generic portability. If reflection compatibility is
the goal, retain only metadata-oriented handling and document that ordinary
construction, equality, and value text are C++ extensions.

### Status: REMEDIATED (#2285 review, #2286 implementation, 2026-08-11)

Repaired by **this finding's own first alternative** — "make it a clearly
project-specific type and remove the claimed C# generic portability" — with one
refinement recorded rather than glossed over. That phrase cannot be taken
wholesale: unlike `SR-AUD-127`'s `CrashReason`, `System.Void` **is** a public
.NET type and the empty field-less shape **is** .NET's. What is project-specific
is the *use model*, and that is what the header now says. The second alternative
was unavailable: reflection is a permanent project deviation (`CLAUDE.md`), so
there is no "metadata-oriented handling" to retain, and stripping the type back
to it would delete three public members.

The finding's substantive complaints are both statements about what the header
presents, and both are now false. The doc-comment states that .NET's
`System.Void` is a public *metadata* type naming the `void` return type with no
declared field; that C# rejects construction (CS0143) and rejects the type as a
value or generic argument (CS0673), naming the type object with `typeof(void)`;
that **no compiling C# source therefore exists** for the `Nullable<System.Void>`
/ `Task<System.Void>` patterns, so the header no longer claims to help port them;
that the C++ value API — declaration, copy, comparison, container storage,
template-argument use, `ToString()`, `operator==`, `operator!=` — is a project
extension with no .NET counterpart, the empty string included; that this port's
`Void` has no base class and therefore no inherited object text, `GetHashCode`,
`Equals(object)`, ordering or `std::hash`; that the only property shared with
.NET is the compiler-visible shape; and that there is no first-party production
consumer.

**Premise correction — the comment was wrong in two opposite directions.** This
finding names the over-claim. The same comment also carried an *under*-claim:
"You cannot use System::Void in C++ code directly as a variable type". `Void v;`
compiles, and this file's own `VoidTest.DefaultConstruct` has been proving so
since the type was written — the note was the C# CS0673 restriction transliterated
into a language that does not impose it. Both are repaired by the same edit, so
this is recorded here rather than given a separate ticket. A third, smaller
inaccuracy was corrected in passing: `ToString`'s comment said "System.Void has no
documented ToString in .NET", which understates this report's own evidence —
the .NET struct declares **no member at all**.

**Nothing in the compiled surface changed** — no member added, removed, renamed,
re-signed or re-qualified; `sizeof` 1, `alignof` 1, non-polymorphic, both
operators still `noexcept`, still `<string>` only, still `Core.Base` — so there is
no ABI, layout, vtable, `noexcept`, symbol or component-dependency consequence.
No executable statement was changed.

**Consumer inventory re-measured, not inherited:** zero production consumers
(nothing in the repository declares, returns or stores a `Void`), two test files
holding ten cases in two suites whose names differ only by a trailing `s`
(`VoidTest` in `VoidTests.cpp`, `VoidTests` in `SystemTypesRemainingTests.cpp`),
both linking into `SharpRuntimeTests_Core_Base`. The duplication this report's
metadata already flagged is pre-existing and was left untouched. Zero consumers
did **not** license withdrawing the header or a member: downstream consumers
exist and this batch may not inspect them. Withdrawal stays a live approval
boundary, deliberately not foreclosed.

**Tests: one added, none retired.** The existing ten already pin construction,
equality, `ToString`, `vector<Void>` and `Nullable<Void>` twice over; adding more
pins there would harden a surface a later approved change may need to withdraw,
so none were. `VoidTest.IsAnEmptyFieldlessTagType` pins the one property this
port genuinely shares with .NET and that nothing covered: `is_empty_v`,
`!is_polymorphic_v`, `is_standard_layout_v`, trivial default-construction, copy
and destruction, and `sizeof == 1`. The existing
`sizeof(Void) == sizeof(unsigned char)` is not a substitute — a one-byte data
member keeps that equality while destroying emptiness, which mutation 1 confirmed:
adding `char pad_ = 0;` left every pre-existing case passing and failed only the
new one. Mutation 2 (`ToString()` returning `" "`) was caught by the two
pre-existing `ToString` cases. Both mutations were rebuilt and relinked before
running.

**SR-AUD-127 analogy: superficial.** Both findings are about what a header says
and neither type has a first-party consumer, but that characteristic also runs
through SR-AUD-124/125/126/128/129/137 and is not a cause. `CrashReason` is a
.NET-`internal` concept republished under a name .NET does not publish;
`System.Void` is a name .NET *does* publish, given a use model C# forbids. No CCF
minted. `docs/CoreVoidValueSurfacePlan.md`.

## Other missing assertions and diagnostics

- No test distinguishes C++ adaptation behavior from a usable C# source/API
  contract or checks reflection/type-name handling.
- The empty `ToString` claim is not compared with the inherited object/type
  text behavior and has no documented consumer.
- No test covers type traits, hash/container behavior, serialization, or the
  interactions with Task/Nullable that the header advertises.

## Final assessment

The empty C++ struct is mechanically safe, but its advertised .NET/generic
meaning is the confirmed SR-AUD-136 contract drift. No source or test was
modified during this audit.
