# Audit: `modules/net-sockets/include/System/Net/Sockets/LingerOption.hpp`

## Metadata

- AUDITED: mutable linger value and equality/hash behavior.
- Evidence: current .NET source and direct value tests.

## Assessment

The simple mutable value tracks the managed type; current .NET likewise stores
the supplied signed linger time without constructor/setter range validation.

## Other missing assertions and diagnostics

- Add native socket-option round trips for disabled, zero, negative, and large
  linger values on each supported platform.

## Final assessment

No separate finding. No source or test changed.
