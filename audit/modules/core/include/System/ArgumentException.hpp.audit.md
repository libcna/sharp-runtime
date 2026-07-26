# Audit: `modules/core/include/System/ArgumentException.hpp`

## Metadata

- Audit status: AUDITED (121-line public declaration, fully read).
- Validation: the combined `ArgumentExceptionTests.*`,
  `ArgumentNullExceptionTests.*`, and `ArgumentOutOfRangeExceptionTests.*`
  filter passed 64/64 in `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference: local .NET `System/ArgumentException.cs` was compared, including
  the nullable/Unicode `ThrowIfNullOrWhiteSpace` contract.

## Assessment

The declaration exposes the expected inheritance, message/parameter and inner
exception constructor forms, and separates fully composed derived messages to
avoid duplicating the base suffix.  Its `std::string` adaptation cannot receive
a null managed string, but the stated whitespace helper does not disclose that
it classifies UTF-8 bytes rather than .NET characters.

## Finding references

- **SR-AUD-048 (extended):** `ThrowIfNullOrWhiteSpace` promises whitespace
  semantics but its implementation uses byte `std::isspace`; UTF-8 U+00A0 is
  accepted, where `string.IsNullOrWhiteSpace` rejects it.  CCF-015 records the
  independent repetition with `MemoryExtensions` trim.

## Other missing assertions and diagnostics

- No direct test supplies Unicode .NET whitespace, malformed UTF-8, locale
  changes, tabs/newlines, or a non-whitespace multibyte value to either guard.
- Constructor tests check substring presence rather than the complete
  parameter suffix, empty parameter behavior, `what()` stability after copy or
  move, and the retained inner exception identity.
- The public `std::string` adaptation cannot distinguish null from empty; the
  header documents that limitation but offers no overload or diagnostic for a
  nullable C-style string input.

## Final assessment

The exception surface is structurally complete, but its whitespace wording
overstates the byte-oriented implementation.  No source or test was modified
during this audit.
