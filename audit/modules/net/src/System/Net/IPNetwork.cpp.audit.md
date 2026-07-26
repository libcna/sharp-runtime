# Audit: `modules/net/src/System/Net/IPNetwork.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [IPNetwork.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/IPNetwork.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

Prefix masking and IPv4-mapped membership behavior were reviewed against the
reference implementation.  Reconstructing the masked address loses IPv6 scope
metadata even when no address byte changes.

### SR-AUD-303 — medium — `IPNetwork` discards an already-normalized IPv6 base address's scope ID

`IPNetwork(IPAddress::Parse("fe80::%7"), 64)` reports base scope `0` and
formats as `fe80::/64`; .NET's `ClearNonZeroBitsAfterNetworkPrefix` returns the
original `IPAddress` when the bytes are already normalized, preserving scope
identity and formatting.  C++ always rebuilds from bytes, whose constructor
defaults the scope to zero.

Required remediation: return the original address when masking makes no byte
change, and preserve scope when a reconstructed IPv6 address is required.

## Missing assertions and diagnostics

`NetTests.cpp` asserts ordinary IPv4 and unscoped IPv6 networks only.  Add
scoped normalized and masked IPv6 base-address/format/equality coverage.

## Final assessment

Confirmed IPv6 scope-metadata loss.
