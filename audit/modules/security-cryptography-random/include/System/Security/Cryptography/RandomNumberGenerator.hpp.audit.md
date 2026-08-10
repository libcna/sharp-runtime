# Audit: `modules/security-cryptography-random/include/System/Security/Cryptography/RandomNumberGenerator.hpp`

## Metadata

- Audit status: AUDITED (120 lines, full read).
- Related evidence: the focused 13-test integration filter passed; its coverage
  limitations are recorded in
  `audit/tests/integration/System/Security/Cryptography/KeyDerivationTests.cpp.audit.md`.

## Assessment

The public contract exposes OS-backed byte generation, vector range helpers,
non-zero generation, and unbiased rejection sampling for `GetInt32`.  Argument
checks precede vector slicing, and the simple/small positive range path is
exercised by the integration test.

## SR-AUD-012 — medium — full-domain `GetInt32` uses implementation-defined conversion and reachable signed overflow

`SharpRuntime::intcs` is explicitly `int32_t`.  For the valid range
`[INT32_MIN, INT32_MAX)`, lines 83–100 can accept any `uint32_t` result except
`UINT32_MAX`.  When the accepted result is at least `0x80000000`, line 102 first
converts it to `int32_t` outside its representable range (implementation-defined
under the project’s supported C++ portability model) and then adds
`INT32_MIN`.  That signed addition can overflow before any intended
two's-complement wrap is observed, which is undefined behavior in C++.

The normal small-range integration test cannot reach this branch.  This is a
reachable public security-API portability/undefined-behavior defect, not merely
a theoretical oversized container issue.

### Reproduction

1. Confirm `intcs` is `int32_t` in
   `modules/core/include/SharpRuntime/SharpRuntimeHelper.hpp`.
2. Call `RandomNumberGenerator::GetInt32(INT32_MIN, INT32_MAX)` repeatedly
   under UBSan.  Approximately half of unmasked accepted draws have bit 31 set,
   reaching the conversion/addition sequence above.
3. Current normal focused tests pass because they invoke only `[10, 20)`.

### Required repair verification

Perform the final offset arithmetic in an unsigned representation (or another
defined equivalent), then add a bounded full-domain range test run under UBSan
and preserve the existing small-range checks.  The audit makes no source
change.

## Other missing coverage

The virtual range overload and `GetNonZeroBytes` have no deterministic fake
provider tests for offset/count boundaries, all-zero refill behavior, or empty
input.  Invalid argument and one-argument `GetInt32` paths are also absent.

## Final assessment

Clear API documentation and ordinary-range behavior, but the full signed
domain needs a defined arithmetic implementation and targeted diagnostics.
