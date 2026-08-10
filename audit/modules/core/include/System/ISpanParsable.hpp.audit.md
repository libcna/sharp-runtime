# Audit: `modules/core/include/System/ISpanParsable.hpp`

## Metadata

- Audit status: AUDITED (67-line public template interface, fully read).
- Supporting validation: `ISpanParsableTests.*` and `ISpanParsableTests2.*`
  passed 18/18 in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

`ISpanParsable<TSelf>` correctly derives from the local `IParsable<TSelf>`
adapter and restores the inherited string overloads with `using` declarations.
That avoids C++ name hiding at the interface type and makes both string and
`ReadOnlySpan<char>` parsing operations reachable via an interface pointer.
The two added operations have no default implementation; the concrete parser
therefore owns syntax, provider, range, and malformed-span handling.

This virtual-instance CRTP design cannot reproduce .NET's static-abstract
generic contract exactly, but the header records that limitation explicitly.
The interface itself performs no signed-length conversion or raw read.  The
already-confirmed malformed-span issue (`SR-AUD-043`) belongs to concrete
consumers that turn untrusted signed span metadata into byte/string lengths.

## Positive findings

- Tests cover inheritance, all four string/span parser operations, normal and
  invalid decimal input, and virtual dispatch through an
  `ISpanParsable<IntParser>*`.
- The interface's `using` declarations compile with both the string and span
  overrides in the concrete fixture, which validates the intended overload-set
  preservation.

## Other missing assertions and diagnostics

- Invalid `TryParse` tests pass default-constructed result objects and never
  assert the result after failure.  They cannot detect a future contract choice
  to preserve stale output versus assign a default value; any repair should
  make that local adaptation decision explicit and test a pre-populated result.
- Only `nullptr` providers are used.  No fixture demonstrates whether a
  concrete parser honors, rejects, or intentionally ignores a non-null
  `IFormatProvider`.
- No direct fixture supplies malformed `ReadOnlySpan<char>` metadata.  Such a
  test must be coordinated with the already-recorded Span constructor and
  signed-length defects rather than silently relying on a string conversion.

## Final assessment

The interface's overload visibility and virtual dispatch behave as documented.
No new defect is confirmed in this declaration; no source or test was modified
during this audit.
