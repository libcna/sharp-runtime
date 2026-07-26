# Audit: `modules/net/src/System/Net/Dns.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [Dns.cs](/rv/tmp/runtime/src/libraries/System.Net.NameResolution/src/System/Net/Dns.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

The POSIX/WinSock paths are scoped and released correctly on successful calls.
The local literal fast path duplicates parsing and bypasses the common .NET
literal policy.

### SR-AUD-304 — medium — DNS IPv4 literal fast path accepts invalid text, wildcard addresses, and wrong-family results

`tryParseIPv4` uses `sscanf("%u")`, so `-0.0.0.1` is accepted as `0.0.0.1` on
the audited libc; an out-of-range conversion is undefined by the C scanf
contract.  `GetHostAddresses("0.0.0.0")` returns the wildcard address although
current .NET rejects `IPAddress.Any`, and an IPv4 literal queried with
`AddressFamily::Unix` returns one IPv4 result instead of the empty
family-filtered result.  The shared .NET route first parses with `IPAddress`
and then applies wildcard and family policy.

Required remediation: remove the duplicate `sscanf` parser, use
`IPAddress::TryParse`, reject wildcard literals consistently, and apply the
requested family filter before returning a literal.

## Missing assertions and diagnostics

DNS tests exercise ordinary IPv4/IPv6 literals but omit negative text,
oversized numeric fields, wildcard literals, and a literal with a mismatched
or non-IP `AddressFamily`.

## Final assessment

Confirmed fast-path validation and family-filter mismatch.
