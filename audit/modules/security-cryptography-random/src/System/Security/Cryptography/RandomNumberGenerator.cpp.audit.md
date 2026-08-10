# Audit: `modules/security-cryptography-random/src/System/Security/Cryptography/RandomNumberGenerator.cpp`

## Metadata

- Audit status: AUDITED (79 lines, full read).
- Platforms reviewed: Windows `BCryptGenRandom`, Linux `getrandom`, Emscripten
  explicit unsupported exception, and Darwin/BSD `getentropy` chunking.

## Assessment

The implementation avoids silently falling back to weak entropy.  Linux handles
`EINTR` and partial reads; the portable branch limits `getentropy` calls to its
documented 256-byte maximum.  `Fill` holds a function-local static shared
provider; static initialization itself is safe, and provider calls do not
mutate visible wrapper state.

## Missing assertions and diagnostics

- No test substitutes a provider that returns an OS failure, so the translated
  `CryptographicException` diagnostics and no-partial-success behavior are not
  asserted.
- Linux does not explicitly handle a zero-byte `getrandom` result.  The kernel
  API normally returns a positive byte count for a non-empty request, but a
  defensive zero-progress diagnostic would prevent an infinite loop if a
  platform abstraction violates that expectation.
- Platform branches require supported-platform CI or controlled syscall seams;
  the current Linux-focused integration tests cannot validate Windows,
  Emscripten, and BSD/Darwin behavior.

## Required post-audit verification

Use a syscall/provider seam to test EINTR, partial progress, hard failure, and
zero progress without weakening OS entropy use.  Run the platform-specific
tests on their corresponding CI runners.

## Final assessment

The implementation chooses strong failure behavior and has sensible
platform-specific code; error-path diagnostics and platform coverage remain
incomplete.
