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
