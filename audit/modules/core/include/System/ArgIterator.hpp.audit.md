# Audit: `modules/core/include/System/ArgIterator.hpp`

## Metadata

- Audit status: AUDITED (96-line explicit unsupported-feature stub, fully read
  with RuntimeArgumentHandle, TypedReference, and its complete Batch12 fixture).
- Validation: `RuntimeArgumentHandleTests.*:ArgIteratorTests.*` passed 11/11
  on 2026-07-26.
- Reference basis: local .NET ArgIterator's CLR varargs dependency and C++
  object-lifetime rules.

## Assessment

The header honestly declares that CLR `__arglist` cannot be represented in the
C++ port and makes both usable constructors throw `NotSupportedException`.
It does not invent a vararg memory layout. The source of the 11 green tests,
however, reaches the non-constructing member methods through an object whose
lifetime never began.

## SR-AUD-112 — medium — ArgIterator tests invoke non-static methods on storage that is not an ArgIterator object

Since both public constructors always throw, `Batch12ArgHandleTests.cpp` makes
an aligned `char[sizeof(ArgIterator)]`, reinterpret-casts it to
`ArgIterator*`, and calls `End`, `GetHashCode`, `Equals`, and throwing methods
(`:35-82`). No placement construction or lifetime-starting operation occurs.
Calling a non-static member function through that pointer is undefined C++
behavior even though the current empty methods happen not to access state.

The 11/11 filter therefore validates a fabricated object rather than a public
state. A future nonempty stub, compiler optimization, or sanitizer/toolchain
change can invalidate the tests without any production behavior changing.

### Status: REMEDIATED (#2274 review, #2275 implementation, 2026-08-11)

Reproduced exactly. **Premise refinement:** there are **six** such tests, not
five, holding **seven** fabricated objects — `Equals_ReturnsFalse` builds two.
Enumerated: `End_DoesNotThrow`, `GetHashCode_ReturnsZero`, `Equals_ReturnsFalse`,
`GetNextArg_Throws`, `GetNextArgType_Throws`, `GetRemainingCount_Throws`.

The cause is measured rather than assumed: `is_default_constructible_v` is
`false`, because declaring the two `[[noreturn]]` constructors suppresses the
implicit default one, so there is no ordinary route to an instance and the
fixture reached for storage instead.

A legitimate route does exist and needs no production change:
`std::bit_cast<ArgIterator>(static_cast<unsigned char>(0))` is constrained only
on trivial copyability and equal size — both measured true, with `sizeof` 1 and
`is_empty_v` true — and **returns an object** of the destination type, so the
lifetime the fixture was missing actually begins. All seven fabrications are now
one such object each, and **every pre-existing assertion is retained verbatim**.

Four tests were added to pin what the mechanism depends on, so that invalidating
it fails loudly instead of tempting the next author back into raw storage: the
type is not default-constructible, it is empty and trivially copyable at
`sizeof` 1, and the handle constructor and `GetRemainingCount` carry their exact
messages. Three mutations on the production header, all caught — and giving
`ArgIterator` state is caught **at compile time**, which is exactly the loud
failure claimed.

**Sanitizers are not discriminating for this defect class, and that was
measured, not assumed.** An ASan+UBSan reproduction of the fixture's exact shape
— an empty, non-polymorphic class whose only constructor throws, reached through
a `reinterpret_cast` of raw character storage — completes with no diagnostic and
exit status 0. UBSan's lifetime-adjacent checks need a polymorphic type or a
known allocation, and ASan sees an in-bounds access to a legally sized stack
buffer; there is no instrumentation for "this pointer does not designate an
object". Running them for ceremony would have produced a green result arguing the
opposite of the truth.

The residual this report itself raises — that no public construction can succeed,
so the instance members are unreachable and "need a deliberate static or
unsupported-operation design" — is **not** answered here and **not** absorbed
into SR-AUD-112's frozen text, which is about the fixture's access route. The
header now documents the unreachability, and ticket **#2276 (`needs_user`)**
carries the choice. `docs/CoreArgIteratorTestLifetimePlan.md`.

## Other missing assertions and diagnostics

- No public construction can succeed, so End/equality/hash/next-argument
  methods need a deliberate static or unsupported-operation design rather than
  lifetime-violating test access.
- Tests omit exact exception messages, unsupported feature diagnostics, empty
  RuntimeArgumentHandle semantics, and all CLR varargs integration (which is
  intentionally unavailable).

## Final assessment

The public unsupported-feature boundary is explicit, but the direct fixture
uses undefined behavior to test unreachable instance methods. No source or
test was modified during this audit.
