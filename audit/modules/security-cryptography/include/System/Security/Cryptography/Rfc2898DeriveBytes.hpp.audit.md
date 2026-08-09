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

---

## Remediation record — ticket #2160 (2026-08-09)

SR-AUD-331 **remediated**. Repair, evidence and mutations:
`docs/SystemSecurityCryptographyNamespaceReviewPlan.md` §15.

The original evidence reproduces exactly. Before the repair: `Dispose()` then `GetBytes(4)` returned
four bytes (`pbkdf-after-dispose=4`), `password_` held all 16 password bytes, `salt_` 12 and
`buffer_` 32, and the returned bytes were **byte-identical to a fresh instance's continuation** —
confirming this report's reading that the post-disposal path is the real derivation. After: all
three buffers read 0, and `GetBytes` throws `ObjectDisposedException` naming `Rfc2898DeriveBytes`.

**A prediction this repair withdrew.** The handoff that opened this batch stated that repairing this
finding required an object-layout change. It did not. The type has a **four-byte padding hole at
offset 108**, between `blockSize_` and the 8-byte-aligned `buffer_`; the flag lives there, so
`sizeof` stays **160**, `alignof` stays **8**, **zero** pre-existing members moved, and `Dispose`
was already virtual on `DeriveBytes` so the override filled an existing vtable slot
(`build-probe/2160_layout_real.log`). Nothing about this finding was ever approval-gated.

**One defect this report does not name, found while repairing it.** `getSaltProperty()` computes
`salt_.end() - 4`. Erasing the salt on disposal without also guarding that getter would have turned
a disclosure defect into **undefined behaviour**, so the guard and the erasure had to land together;
a mutation that removes the guard aborts the test process on an out-of-bounds write. A second,
separate post-audit defect (no `SR-AUD-*` identifier, cause SC-C) is fixed with it: the constructor
moved the password into `password_` before validating `iterations` and the hash algorithm, so a
**rejected** call destroyed a fully populated password buffer without erasing it — measured as a
16-byte run of the password in freed storage, now 0.

**The assertions this report asked for are now permanent** (+21 tests): SHA-1/256/384/512 PBKDF2
published vectors including RFC 6070 at 1/2/4096 iterations and this report's own SHA-256 vector,
split-call continuation, reset, salt and iteration reset, invalid count/iteration/hash with exact
diagnostics, the lifecycle rejection at every door, and the zeroisation assertion for password,
salt/counter and residual buffer. The stale comment claiming SHA-3 "has not been ported" is
corrected: it has been, in this component; PBKDF2's SHA-3 exclusion stays, because .NET's own
`OpenHmac()` excludes it too.
