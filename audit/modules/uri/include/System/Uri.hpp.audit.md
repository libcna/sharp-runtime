# Audit: `modules/uri/include/System/Uri.hpp`

## Metadata

- AUDITED: 158-line public declaration, fully read.
- Validation: `UriTests.*` passed 57/57 on 2026-07-27.
- Reference/probe: local current .NET Uri source plus
  `/tmp/sharp-runtimervc-uri-audit-probe` built against the C++ Uri module and
  a matching C# `mcs`/`mono` probe.

## Assessment

The header makes explicit that percent encoding/decoding and canonical
AbsoluteUri reconstruction are not implemented adaptations. Those documented
limits are not classified here. However, it promises a lower-case parsed
Scheme and .NET-shaped construction/equality operations that the implementation
does not deliver; see SR-AUD-142 through SR-AUD-145 in the companion source
report. `UriCreationOptions`, `UriPartial`, and `UriHostNameType` also name
public Uri operations that this header does not expose; see SR-AUD-149 through
SR-AUD-151.

## Other missing assertions and diagnostics

- The public `UriKind` enum permits arbitrary C++ casts, but neither the
  declaration nor direct fixture states invalid-value rejection.
- Relative query-only, fragment-only, and network-path references are part of
  the two-Uri construction contract but not represented in the test suite.
- Header prose calls `getAbsoluteUriProperty` the entire input, while the
  managed name normally carries canonical semantics; this is documented as an
  adaptation but needs a prominent API-baseline decision before a repair.

## Final assessment

The declared partial boundary is useful, but several ordinary supported URI
forms remain incompatible. No source or test was modified during this audit.
