# Audit: `modules/net/include/System/Net/WebUtility.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [WebUtility.cs](/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Net/WebUtility.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

URL percent handling is self-contained and malformed percent sequences remain
literal.  The documented HTML subset is materially narrower than the public
`WebUtility` contract.

### SR-AUD-309 — medium — HTML encoding/decoding is an incomplete byte-oriented subset of `WebUtility`

The probe shows UTF-8 copyright text passes through `HtmlEncode` unchanged and
`HtmlDecode("&euro;")` leaves the named entity literal.  Current .NET handles
non-ASCII scalar values and its complete named-entity table.  The C++ header
documents the omission but callers cannot request compatible behavior.

Required remediation: use UTF-8 scalar decoding/validation and a complete
HTML entity mapping, or expose the existing five-entity behavior under a
separately named restricted helper.

## Missing assertions and diagnostics

Current Net tests cover URL behavior but no non-ASCII HTML encoding, named
entity decoding, malformed scalar, or supplementary-plane cases.

## Final assessment

Confirmed compatibility subset.
