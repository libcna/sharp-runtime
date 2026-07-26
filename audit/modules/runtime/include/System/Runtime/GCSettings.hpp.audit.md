# Audit: `modules/runtime/include/System/Runtime/GCSettings.hpp`

## Metadata

- AUDITED: 60-line inline GC configuration header, fully read.
- Validation: `GCSettingsTests.*:GCLatencyModeTests.*` passed 9/9 on
  2026-07-27; the complete shared `RuntimeTests.cpp` filter passed 82/82.
- Reference/probe: local current-.NET `System/Runtime/GCSettings.cs`; a
  standalone C++ probe compiled with `g++ -std=c++17 -Imodules/runtime/include`
  stores latency `99` and LOH mode `0` without throwing.

## SR-AUD-156 — medium — GCSettings setters retain values outside their public enum domains

Both setter bodies are unconditional assignments.  The C++ probe therefore
prints `latency=99` after `static_cast<GCLatencyMode>(99)` and `loh=0` after
`static_cast<GCLargeObjectHeapCompactionMode>(0)`.  The authoritative current
.NET source validates `LatencyMode` from `Batch` through
`SustainedLowLatency` (so callers also cannot set the runtime-owned
`NoGCRegion` state) and validates LOH mode from `Default` through
`CompactOnce`; either invalid input throws `ArgumentOutOfRangeException`.

An invalid native enum cast consequently becomes a persistent, observable
global configuration state instead of being rejected at the public boundary.

## Assessment

The declared enum values and initial `Interactive`/`Default` values match the
current .NET definitions.  `isServerGC_` is intentionally hardwired false:
the separately audited `System::GC` adapter explicitly uses RAII/no tracing GC
and has no server-GC capability.  Searches find no other production consumer
of either mutable setting, so the header presently records settings rather
than configuring a collector; that documented adaptation boundary is not
classified separately.

## Other missing assertions and diagnostics

- The direct tests exercise only valid Batch, LowLatency, Default, and
  CompactOnce writes.  They omit arbitrary casts, both adjacent invalid LOH
  values, and the rejected `NoGCRegion` setter input that current .NET treats
  as runtime-owned state.
- Neither `SustainedLowLatency` nor the LOH enum's numeric values receive an
  assertion.
- Tests assert the fixed false server-GC state but cannot distinguish the
  documented no-tracing-GC adaptation from a future host configuration query.

## Final assessment

The small value surface is otherwise coherent, but its two public mutation
boundaries lack mandatory enum validation.  No source or test was modified.
