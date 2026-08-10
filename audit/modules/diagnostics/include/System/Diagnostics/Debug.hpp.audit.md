# Audit: `modules/diagnostics/include/System/Diagnostics/Debug.hpp`

## Metadata

- AUDITED: provider replacement, diagnostic output, and thread-local indentation.
- Evidence: source review, target tests, and `/tmp/sharp-runtime-diagnostics-audit/debug_provider_race.cpp` under TSan.

## Assessment

### SR-AUD-275 — high — global Debug provider and diagnostics state are unsynchronised

`providerStorage()` and the global indent size are unsynchronised. A writer
calling `Debug::Write` while another thread calls `SetProvider` races on the
same `std::shared_ptr`; TSan reports the read at `Debug.hpp:180` racing the
assignment at `Debug.hpp:70`. This is undefined behavior, not merely
interleaved logging. Trace repeats the unsynchronised global-indent pattern.

## Other missing assertions and diagnostics

- Add TSan coverage for Get/SetProvider versus Write/Fail and concurrent
  indentation changes; preserve a provider lifetime across every dispatched call.

## Final assessment

SR-AUD-275 applies. No source or test changed.
