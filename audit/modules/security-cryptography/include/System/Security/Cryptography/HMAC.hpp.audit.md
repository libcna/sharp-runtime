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

---

## Remediation record — ticket #2159 (2026-08-09)

SR-AUD-332 **remediated**. Repair, evidence and mutations:
`docs/SystemSecurityCryptographyNamespaceReviewPlan.md` §14. Commit: see `plan.sqlite3` #2159.

The original evidence is retained above and reproduces exactly: the probe re-run before the repair
measured `innerPad_` 64 bytes with a 32-byte run of `key ^ 0x36`, `outerPad_` 64 with a 32-byte run
of `key ^ 0x5C`, and **32 of 32** key bytes recovered by one XOR after `Dispose()`. After the
repair every one of those readings is **0**, against a deliberately uncleared 96-byte control that
still reads 96 in both columns.

**The finding was narrower than the defect.** Direct measurement of *freed* storage found five
further sites the report does not name — `derivePads`' `keyPrime` local (the raw key, **even after
`Dispose`**), `HashFinal`'s two working buffers (both pads, on **every** `ComputeHash`), the old key
on replacement, plain destruction without `Dispose` (the ordinary C++ RAII path), and, once the
long-key path was measured, the **36-byte** tail of the key left in the internal block buffer of the
digest object that hashes an over-long key. All are 0 after the repair.

**Both "missing assertions" this report asked for are now permanent.** The "deterministic secure-zero
inspection seam" is `SharpRuntime::Testing::KeyMaterialAccess<T>`, defined in exactly one file and
guarded by `scripts/check_version_seam_odr.py` plus
`test/consumer/security_cryptography_key_material_negative.cpp` (9 sites). Key replacement,
long-key hashing, pending-message cleanup and **every** HMAC-SHA3 variant are covered, alongside the
published digest vectors, which are byte-identical before and after.
