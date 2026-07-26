# Audit: `modules/security-cryptography/include/System/Security/Cryptography/HMAC.hpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## SR-AUD-332 — high — HMAC disposal leaves invertible key-derived pads resident

`HMAC` stores `innerPad_ = keyPrime ^ 0x36` and
`outerPad_ = keyPrime ^ 0x5C`. It inherits `KeyedHashAlgorithm::Dispose`,
which clears only `keyValue_`; it does not override disposal to clear either
pad or `pendingMessage_`. Either pad recovers the padded HMAC key with a fixed
XOR.

The direct inspection probe constructs HMAC-SHA256 with `secret`, calls
`Dispose()`, and reports `hmac-direct-key-cleared=1`,
`hmac-pad-reconstructs-key-after-dispose=1`, and
`hmac-pads-after-dispose=64/64`. Access control is disabled solely for this
audit probe; the result directly observes the implementation's private state.
A normal public post-disposal `ComputeHash` does throw, so this is
secret-retention rather than a post-disposal computation defect.

## Missing assertions and diagnostics

- Add a deterministic secure-zero inspection seam or allocator hook for all
  direct and transformed HMAC key storage after disposal.
- Cover key replacement, long-key hashing, pending-message cleanup, and every
  HMAC-SHA3 variant in addition to published digest vectors.

## Final assessment

Confirmed high-severity key-material retention defect: SR-AUD-332.
