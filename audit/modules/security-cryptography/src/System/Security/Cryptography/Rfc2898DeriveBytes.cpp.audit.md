# Audit: `modules/security-cryptography/src/System/Security/Cryptography/Rfc2898DeriveBytes.cpp`

## Metadata

- Audit status: AUDITED (145 lines, full read).
- Runtime evidence: all three in-scope SHA-1 RFC 6070 vectors and continuation
  behavior passed in the focused integration filter.

## Assessment

The PBKDF2 loop correctly writes a big-endian block index, computes U1, XORs
subsequent iterations, and retains unused derived bytes for later calls.  The
constructor and iteration setter reject zero/negative iteration counts before
the narrowing assignment is used.  The block counter limit has an explicit
exception rather than silently wrapping.

## Missing assertions and diagnostics

- SHA-256/SHA-384/SHA-512 paths and unsupported algorithm diagnostics have no
  published-vector or exception test.
- `setIterationCountProperty` and `setSaltProperty` call `initialize`, but no
  test establishes that previously buffered output is discarded and the next
  bytes derive from the new configuration.
- Boundary behavior near the `uint32_t` block counter cannot be practically
  reached by normal allocation; future implementation changes should preserve
  its explicit diagnostic and use a controllable counter seam for this branch.

## Final assessment

The reviewed SHA-1 path has good vector evidence.  The other advertised hash
algorithms, configuration transitions, and exceptional diagnostics remain
unverified.
