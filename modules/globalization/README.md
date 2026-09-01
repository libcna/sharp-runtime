<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::Globalization

Compiled globalization component for calendars, cultures, regions, and text
elements. Public dependency: `Core.Base`.

This component deliberately implements a deterministic practical subset rather than an ICU-backed
locale database:

- `StringInfo` and `TextElementEnumerator` operate on Unicode scalar boundaries. They do not
  implement UAX #29 grapheme clusters; returned positions in a UTF-8 string are byte positions.
- `CompareInfo` supports ordinal comparison and invariant Unicode simple case folding. Linguistic
  `CompareOptions` are rejected explicitly, and a stored culture name does not select collation.
- `TextInfo` uses the generated UCD 16.0 simple-case tables without culture tailoring or
  multi-scalar special-casing expansions.
- `IdnMapping::AllowUnassigned` follows the same pinned UCD 16.0 assignment data. This is not a
  claim that the rest of IDNA Nameprep is implemented.
- `Calendar` is an abstract base; callers must select a concrete calendar implementation.

See `docs/ComponentCatalog.md` for authoritative dependency metadata.
