# Audit: `modules/core/include/System/Guid.hpp`

## Metadata

- Audit status: AUDITED (313-line public header, fully read).
- Related implementation: `modules/core/src/System/Guid.cpp` (569 lines, fully
  read).
- Related validation: `GuidTests.*` passed 80/80 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.

## Assessment

The header supplies a compact canonical-byte representation and clearly
documents several intentional adaptation boundaries (value-type interfaces,
rare legacy parser quirks, Unicode trimming, and a documented X-format
exception difference).  The byte-order and version/variant APIs have focused
normal-path coverage.  Its `NewGuid` and `CreateVersion7` declarations claim
current .NET counterparts, however, so their implementation must retain both
the concurrency and strong-entropy properties of those entry points.

Reference: [.NET `Guid.NewGuid` remarks](https://learn.microsoft.com/en-us/dotnet/api/system.guid.newguid?view=net-10.0) state that modern .NET uses an OS CSPRNG and returns a version-4 UUID with 122 bits of strong entropy.

## Finding references

- **SR-AUD-010 (extended):** `NewGuid()` is a public static API and
  `CreateVersion7()` is specified to seed its random fields.  Their source
  implementation shares an unsynchronised mutable `std::mt19937_64`, so normal
  concurrent callers have a sanitizer-confirmed C++ data race.
- **SR-AUD-043 (extended):** the public `Parse` and `TryParse` overloads accept
  `ReadOnlySpan<char>` and `ReadOnlySpan<bytecs>`.  Their implementation casts
  the span's signed length directly to `size_t`; a malformed negative span
  produced by the already-confirmed Span construction defect becomes an
  unbounded string length rather than a controlled format failure.
- **SR-AUD-050:** the `NewGuid` documentation identifies it as the .NET
  counterpart, but the implementation's Mersenne Twister is not the
  OS-backed cryptographically secure source that current .NET uses.  Version 7
  inherits the same random-field weakness.

## Other missing assertions and diagnostics

- No public contract note says whether a platform secure-random-source failure
  is reported as a `CryptographicException`, platform exception, or another
  local error type.
- The span-parsing declarations do not state their behavior for malformed
  metadata; remediation must be coordinated with the Span representation
  boundary rather than relying on a later `size_t` conversion.

## Final assessment

The API shape, byte order, and intentional parser deviations are well
documented.  Its random-generation contract is not met by the current source;
the owning implementation and tests carry the detailed evidence.  No source
or test was modified during this audit.
