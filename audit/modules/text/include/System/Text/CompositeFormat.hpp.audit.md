# Audit: `modules/text/include/System/Text/CompositeFormat.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET CompositeFormat.Parse](https://learn.microsoft.com/en-ie/dotnet/api/system.text.compositeformat.parse?view=net-9.0)
  documents `FormatException` for malformed format text.

## Assessment

The parser detects a small grammar subset but delegates a large index to
`std::stoi` and skips alignment/format grammar after the initial digits. The
direct probe reports `std::out_of_range` (`stoi`) for `{2147483648}` and
successfully parses `{0,not-a-width}` with minimum argument count 1.

### SR-AUD-298 — medium — CompositeFormat leaks native parse diagnostics and accepts malformed format items

Invalid public composite format text must deterministically report the runtime
format diagnostic. Native exceptions and acceptance of invalid alignment
grammar let malformed templates enter subsequent formatting paths.

## Finding references

- SR-AUD-298 — medium — malformed composite format grammar is not normalized.

## Other missing assertions and diagnostics

- Test integer boundaries, signed/whitespace indexes, alignment and format
  grammar, escaped braces, and exception taxonomy.

## Final assessment

SR-AUD-298 applies.
