# Audit: `modules/security-cryptography/include/System/Security/Cryptography/Rfc2898DeriveBytes.hpp`

## Metadata

- Audit status: AUDITED (63 lines, full read).
- Runtime evidence: three SHA-1 RFC 6070 vectors, buffering, reset, and basic
  accessors passed in the focused 13-test integration filter.

## Assessment

The public declaration deliberately supports SHA-1/256/384/512 and excludes
MD5/SHA-3.  It preserves state needed for buffered sequential derivation and
returns salt by value.

## Missing assertions

No current test proves the documented SHA-2 variants, unknown-hash rejection,
zero/negative byte count or iteration rejection, or setter-induced reset.  The
copy-return salt accessor is checked for equality but not for defensive-copy
behavior after caller mutation.

## Final assessment

The API’s supported-hash contract is clear; regression coverage is
disproportionately limited to SHA-1.
