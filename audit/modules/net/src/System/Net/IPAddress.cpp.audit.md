# Audit: `modules/net/src/System/Net/IPAddress.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [IPAddress.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/IPAddress.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

IPv4/IPv6 parsing, formatting, mapping, and hash behavior were compared with
the checked-in .NET source.  The raw IPv6 scope-id entry points remain outside
the validation used by the parser.

### SR-AUD-301 — medium — raw IPv6 scope-id construction and mutation silently narrow invalid values

The array-and-scope constructor and `setScopeIdProperty` cast `longcs` directly
to `uint32_t`.  The local probe observes `-1` become `4294967295` and
`4294967296` become `0`.  Current .NET rejects negative scope IDs and values
greater than `UInt32.MaxValue` before narrowing (`IPAddress.ScopeId`).

Required remediation: share range validation between the constructor and
setter, preserving the existing IPv4 `SocketException` behavior.

## Missing assertions and diagnostics

Existing tests cover parser scope overflow, not constructor/setter negative or
`UInt32.MaxValue + 1` inputs.  Add explicit exception-type and unchanged-state
assertions.

## Final assessment

Confirmed public range-validation gap.
