# Audit: `modules/net-security/include/System/Net/Security/EncryptionPolicy.hpp`

## Metadata

- AUDITED: encryption-policy enum, values, and explicit obsolete-value note.
- Validation: `EncryptionPolicyTests.Values` passed in the 13/13 Net.Security fixture;
  values were compared with current .NET source.

## Assessment

All three managed numeric values are preserved.  The header explicitly warns
that the two no-encryption policies are obsolete and that no TLS implementation
in this runtime applies them; this is an appropriate metadata-only adaptation.

## Other missing assertions and diagnostics

- Tests do not record the obsolete/no-transport limitation or prevent a future
  TLS consumer from silently treating every value as RequireEncryption.

## Final assessment

No enum-level defect was demonstrated. No source or test was changed.
