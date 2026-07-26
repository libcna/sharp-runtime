# Audit: `modules/uri/tests/System/UriBuilderTests.cpp`

## Metadata

- AUDITED: 179-line dedicated fixture, fully read.
- Validation: `UriBuilderTest.*` passed 27/27 on 2026-07-27.

## Findings

The fixture protects ordinary default/absolute construction, port bounds,
query/fragment prefix normalization, and same-text equality. It does not cover
the four reproducible compatibility defects in SR-AUD-138 through SR-AUD-141:
copied credential splitting, relative string promotion, Uri semantic identity,
or Scheme/IPv6 Host normalization.

## Missing assertions and diagnostics

- Missing a constructor-from-credentialed-Uri case that reads UserName and
  Password, then replaces Password and checks the final serialization.
- Missing relative `www.example.com/path` construction, empty/unknown scheme
  rendering, uppercase normalization, and invalid-scheme rejection.
- Missing IPv6 Host bracketing and equality/hash vectors that differ only by
  user info or fragment; local .NET functional tests cover the latter two.
- Existing `ToString` cases use only ordinary ASCII components and do not make
  the documented percent-encoding adaptation boundary explicit.

## Final assessment

The green suite validates a narrow happy-path subset and leaves all four
confirmed UriBuilder regressions unguarded. No source or test was modified.
