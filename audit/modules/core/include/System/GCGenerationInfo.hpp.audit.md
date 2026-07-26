# Audit: `modules/core/include/System/GCGenerationInfo.hpp`

## Metadata

- Audit status: AUDITED (29-line inline stub value type, fully read).
- Validation: `GCTests.*` passed 61/61 on 2026-07-26; GCMemoryInfo's empty
  generation-information route was exercised.
- Reference basis: local .NET `GCMemoryInfo.cs` generation-info value contract.

## Assessment

The type consistently returns zero for all four generation-size/fragmentation
properties, matching the parent GC header's explicitly unavailable
telemetry.  There is no mutable or partially initialized state that could
misrepresent a real collection.

## Other missing assertions and diagnostics

- No direct test calls any of the four property methods; the suite tests only
  that the parent returns an empty vector.
- A future telemetry implementation needs tests for values before/after a
  collection, signed-byte ranges, and vector ownership/lifetime rather than
  preserving the present all-zero behavior.

## Final assessment

This is a coherent zero-information stub.  No source or test was modified
during this audit.
