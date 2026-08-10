# Audit: `modules/security/include/System/Security/Authentication/SslProtocols.hpp`

## Metadata

- AUDITED: flags values, underlying integer representation, and bitwise
  composition.
- Validation: the security fixture passed 38/38; values and aliases were
  compared with the checked local .NET `SslEnumTypes.cs` source.

## Assessment

`None`, legacy protocol values, TLS 1.2/1.3, and `Default` retain current .NET
numeric parity.  The local OR/AND helpers make the header usable as C++ flags
metadata.  As its in-file note states, this is not evidence of an actual TLS
engine or negotiation support.

## Other missing assertions and diagnostics

- Assert every value including deprecated aliases and `Default`, plus the
  default enum width and unknown-bit round trip.
- Keep a generated parity check against the managed enum when protocol values
  are updated.

## Final assessment

No enum-value mismatch was demonstrated. No source or test was changed.
