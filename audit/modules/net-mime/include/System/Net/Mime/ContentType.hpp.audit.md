# Audit: `modules/net-mime/include/System/Net/Mime/ContentType.hpp`

## Metadata

- AUDITED: ContentType public constructors, properties, mutable parameters,
  formatting, equality, hash, and documented MIME-decoding limitation.
- Validation: `SharpRuntimeTests_Net_Mime` passed 26/26 on 2026-07-27. Direct
  C++20/current-.NET 10 probes compared trailing semicolons and empty Boundary.

## Assessment

The public surface consistently separates this MIME type from HTTP header
parsing, and documents its lack of encoded-word decoding/caching.  The tested
parser and setters agree with current .NET for trailing parameter separators
and removal via empty Boundary.

## Other missing assertions and diagnostics

- Test comments/CFWS, escaped quotes/backslashes, control/non-ASCII bytes,
  duplicate/case-variant parameters, parameter ordering, invalid mutation via
  the returned dictionary, encoded-word Name behavior, and hash/equality after
  all mutations.

## Final assessment

No evidence-backed finding was confirmed.  No source or test was changed.
