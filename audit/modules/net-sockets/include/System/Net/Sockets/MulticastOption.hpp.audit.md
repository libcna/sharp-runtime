# Audit: `modules/net-sockets/include/System/Net/Sockets/MulticastOption.hpp`

## Metadata

- AUDITED: IPv4/IPv6 multicast option construction and interface-index limits.
- Evidence: source comparison and direct index tests.

## Assessment

Range checks for interface indices match current .NET. C++ value `IPAddress`
parameters cannot represent the managed null-reference diagnostics; no option
consumer joins/leaves multicast groups in this module.

## Other missing assertions and diagnostics

- Cover group-family validation and actual join/drop behavior once an option
  consumer is added, including IPv6 scope IDs.

## Final assessment

No separate finding. No source or test changed.
