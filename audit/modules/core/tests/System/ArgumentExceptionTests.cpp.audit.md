# Audit: `modules/core/tests/System/ArgumentExceptionTests.cpp`

## Metadata

- Audit status: AUDITED (69 lines, thirteen tests, fully read).
- Validation: `ArgumentExceptionTests.*` passed 13/13 within the 64/64
  argument-exception filter in `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The fixture verifies basic construction, inheritance, HResult, ordinary
parameter storage, and ASCII empty/space helper paths.  It proves normal
behavior but leaves the character classification and complete diagnostic
contract untested.

## Finding references

- **SR-AUD-048 (extended):** the whitespace test contains only ASCII spaces;
  it does not exercise U+00A0 or another .NET whitespace character and thus
  misses the demonstrated byte-classification validation bypass.

## Other missing assertions and diagnostics

- Add U+00A0, U+2000, tab/newline, mixed Unicode/visible text, malformed UTF-8,
  and locale-independent vectors for `ThrowIfNullOrWhiteSpace`.
- Tests use `find`/nonempty checks only. They do not assert one parameter
  suffix, exact default message/HResult on every constructor, parameter names
  with empty/embedded-NUL text, or `what()` copy/move lifetime.
- No test checks that the inner exception constructor preserves the supplied
  exception identity or distinguishes a null `exception_ptr`.

## Final assessment

All thirteen normal-path tests pass, but their ASCII-only assertions do not
protect the documented Unicode whitespace contract.  No source or test was
modified during this audit.
