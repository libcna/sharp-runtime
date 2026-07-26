# Audit: `modules/security-cryptography/src/System/Security/Cryptography/Rfc2898DeriveBytes.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.
- Direct probe: `/tmp/sharp-runtime-security-cryptography-audit/dispose_probe`, compiled against the component static library.

## SR-AUD-331 — high — the PBKDF2 implementation remains fully operational after `Dispose`

This implementation owns `password_`, `salt_`, `buffer_`, and the sequential
block state but provides only construction, reset, and derivation methods. No
disposal override exists in the translation unit or declaration, so calls
dispatch to the no-op base. The standalone probe obtains four more derived
bytes after disposal and reports `pbkdf-after-dispose=4`.

That diverges from the current reference implementation, which disposes the
HMAC and clears buffered/salt material. The defect is not a failed PBKDF2
vector: the same probe obtains the expected PBKDF2-HMAC-SHA256 result, proving
that sensitive derivation state remains usable.

## Missing assertions and diagnostics

- Test all supported hash modes with RFC vectors, chunked output, reset, salt
  and iteration reconfiguration, unsupported algorithms, and count boundaries.
- Make the disposal contract test-fatal: derive after disposal must produce a
  stable disposed-object diagnostic, not key material.
- Add secure-zero instrumentation for password, salt/counter, and buffer
  storage during the eventual remediation.

## Final assessment

Confirmed high-severity lifecycle and secret-retention defect: SR-AUD-331.
