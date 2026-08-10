# Audit: `modules/security-cryptography/src/System/Security/Cryptography/HMAC.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `Security.Cryptography`.
- Validation: `SharpRuntimeTests_Security_Cryptography` built and passed 80/80 on 2026-07-27.

## SR-AUD-332 — high — pad derivation leaves a recoverable HMAC key after disposal

`derivePads` writes the padded key as `keyPrime ^ 0x36` and `keyPrime ^ 0x5C`
to persistent member vectors. The only available disposal implementation is
`KeyedHashAlgorithm::Dispose`, which zeros and clears the separate
`keyValue_` vector. No code in this file or declaration clears the pads or the
buffered message.

The standalone probe has compiler access checks disabled only for audit
inspection. After HMAC-SHA256 disposal it sees 64-byte inner/outer pads and
recovers the original `secret` prefix by XORing the inner pad with `0x36`.

## Missing assertions and diagnostics

- Make pad zeroization test-fatal; the direct inspection probe sees 64/64
  retained pads and reconstructs the original key after `Dispose`.
- Exercise long-key replacement and pending-message cleanup, not only digest
  vectors, during the eventual remediation.

## Final assessment

Confirmed high-severity key-material retention defect: SR-AUD-332.
