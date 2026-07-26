# Audit: `modules/security-cryptography/include/System/Security/Cryptography/Rfc2898DeriveBytes.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.
- Direct probe: `/tmp/sharp-runtime-security-cryptography-audit/dispose_probe`, compiled against the component static library.

## SR-AUD-331 — high — `Rfc2898DeriveBytes::Dispose` is inherited as a no-op and permits post-disposal derivation

`DeriveBytes::Dispose()` has an empty default implementation and this concrete
type declares no override. It retains password bytes, salt/counter bytes, the
derived-byte buffer, and derivation indices. The reproducible probe calls
`Dispose()` then `GetBytes(4)`, which returns four bytes
(`pbkdf-after-dispose=4`) instead of rejecting the disposed object. The same
probe verifies the SHA-256 PBKDF2 known vector
`120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b`,
so the live post-disposal operation is the real derivation path.

Current .NET's `Rfc2898DeriveBytes.Dispose(bool)` disposes its HMAC and clears
the buffered and salt state before delegating to the base disposal path. The
C++ object offers no equivalent invalidation or clearing mechanism.

## Missing assertions and diagnostics

- Add SHA-1/256/384/512 PBKDF2 published vectors, split-call continuation,
  reset, salt/iteration reset, invalid count/iteration/hash, and exact diagnostics.
- Add a lifecycle assertion that `GetBytes`, `Reset`, and mutators reject after
  `Dispose`, plus an instrumented zeroization assertion for password,
  salt/counter, and residual buffer.
- Correct the stale comment that says SHA-3 has not been ported; it is present
  in this component even if PBKDF2 deliberately excludes it.

## Final assessment

Confirmed high-severity lifecycle and secret-retention defect: SR-AUD-331.
