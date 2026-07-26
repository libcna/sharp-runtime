# Audit: `tests/integration/System/Security/Cryptography/KeyDerivationTests.cpp`

## Metadata

- Audit status: AUDITED (113 lines, 13 `TEST` cases, full read).
- Runtime evidence: `./build/SharpRuntimeIntegrationTests --gtest_filter='RandomNumberGeneratorTests.*:RNGCryptoServiceProviderTests.*:Rfc2898DeriveBytesTests.*' --gtest_color=no` passed all 13 cases on 2026-07-25.
- Related implementation evidence: `RandomNumberGenerator.hpp/.cpp`,
  `RNGCryptoServiceProvider.hpp`, and `Rfc2898DeriveBytes.hpp/.cpp` were
  reviewed with this integration slice.

## Coverage observed

The test file checks output sizes, two successive entropy results, a small
half-open `GetInt32` range, provider construction, three RFC 6070 SHA-1 PBKDF2
vectors, buffered continuation, reset, and read accessors.  The deterministic
RFC vectors give meaningful evidence for the SHA-1 PBKDF2 path; the entropy
tests intentionally cannot assert exact random bytes.

## Missing assertions and diagnostics

- `RandomNumberGenerator::GetInt32` is checked only for `[10, 20)` and one
  single-value range.  It lacks the valid full `int32` domain
  `[INT32_MIN, INT32_MAX)`, one-argument boundary checks, and invalid/equal
  range exception tests.  The full-domain path is important because its current
  reconstruction uses signed arithmetic after a `uint32_t` draw; see
  **SR-AUD-012**.
- There is no test for `GetBytes(-1)`, empty buffers, range overload offset and
  count validation, `GetNonZeroBytes`, or the non-zero postcondition.
- `GetBytes_ProducesDifferentValuesEachCall` has a negligible but non-zero
  false-failure probability; it should not be the only entropy-quality signal.
  A deterministic fake generator should exercise the range/non-zero helpers,
  while OS-backed tests should assert size and error propagation.
- PBKDF2 covers only SHA-1.  It lacks published SHA-256/SHA-384/SHA-512
  vectors, zero/negative `cb` and iteration checks, rejected hash identifiers,
  setter reset semantics, and proof that returned salt is a defensive copy.

## Required post-audit verification

Add deterministic fake-provider tests for helper validation and full-domain
range handling; add standard SHA-2 PBKDF2 vectors and exception tests.  Run the
focused integration filter above plus an UBSan build for the full-domain
`GetInt32` regression.  Do not make randomness assertions dependent on two
independent OS entropy samples differing.

## Final assessment

The SHA-1 vectors and continuation/reset checks are useful, but the suite does
not cover the arithmetic and exception paths where the native adaptation is
most exposed.
