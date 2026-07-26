# Audit: `modules/net/src/System/Net/IPEndPoint.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [IPEndPoint.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/IPEndPoint.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

Port range checks and normal IPv4/IPv6 parsing agree with the reference.  The
bracketed parser does not consume the grammar exactly.

### SR-AUD-302 — medium — bracketed endpoint parser accepts and discards text between `]` and the port delimiter

For `[::1]ignored:80`, `TryParse` locates the closing bracket and then searches
for any later colon, silently discarding `ignored`; the probe returns true and
produces `[::1]:80`.  The reference parser passes the complete address slice to
`IPAddressParser` and returns false for the same non-endpoint text.

Required remediation: require either end-of-input immediately after `]` or a
colon immediately after it, then consume the complete decimal port.

## Missing assertions and diagnostics

The endpoint tests cover valid bracketed input and invalid ports, but not
trailing/interstitial text after a closing IPv6 bracket.

## Final assessment

Confirmed permissive parser mismatch.
