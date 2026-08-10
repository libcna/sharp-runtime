# Audit: `modules/uri/tests/System/UriTests.cpp`

## Metadata

- AUDITED: 403-line dedicated fixture, fully read.
- Validation: `UriTests.*` passed 57/57 on 2026-07-27.

## Findings

The fixture covers normal lowercase HTTP/HTTPS/FTP parsing, explicit ports,
basic relative paths, and simple dot-segment merging. It omits all four proven
defects in SR-AUD-142 through SR-AUD-145: canonical equality, opaque mailto
Port, query/fragment/network-path resolution, and malformed structural/enum
input validation.

## Missing assertions and diagnostics

- Missing case/default-port equality and hash vectors such as
  `HTTP://EXAMPLE.COM:80/Path` versus `http://example.com/Path`.
- Missing `mailto:user@example.com`.Port expectation of 25 and analogous
  opaque built-in scheme coverage.
- Missing `?query`, `#fragment`, and `//authority/path` references against a
  base URI that already has path/query/fragment state.
- Missing malformed IPv6 brackets and arbitrary `UriKind` cast rejection.

## Final assessment

The extensive happy-path suite is useful but leaves essential URI identity and
resolution boundaries unguarded. No source or test was modified.
