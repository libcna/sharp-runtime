# Audit: `modules/threading/include/System/Threading/Mutex.hpp`

## Metadata

- AUDITED: 85-line recursive native mutex adapter, fully read.
- Validation: focused `MutexTests.*` passed 6/6 on 2026-07-27; direct C++ and
  .NET 10 probes called `Close()` before `WaitOne(0)`.
- Reference basis: current .NET 10 Mutex close/disposal behavior and the
  local explicitly documented process-local/named-mutex adaptation.

## SR-AUD-208 — medium — `Mutex::Close` is a no-op and leaves a supposedly closed handle fully usable

The concrete `Close()` body is empty and the inherited `Dispose()` is also a
no-op. The C++ probe calls `Close()` then prints `wait=1` after a successful
`WaitOne(0)`. The matching .NET 10 probe prints
`exception=System.ObjectDisposedException`. The header says that `Close`
closes the mutex handle, so silent continued acquisition is neither the
documented native adaptation nor the managed lifecycle contract.

## Assessment

Normal recursive ownership, non-owner diagnostics, finite/infinite timeout,
and initially-owned construction are covered by the focused green tests. The
documented absence of named cross-process synchronization and abandoned-mutex
detection is an explicit adaptation, not this finding.

## Other missing assertions and diagnostics

- Tests omit SR-AUD-208 Close/Dispose-before-wait and Close/Dispose-before-
  release cases.
- They omit named-object sharing/created-new behavior, abandoned owner
  behavior, recursive depth limits, disposal while another thread waits, and
  construction/destruction under contention.
- No platform diagnostic distinguishes the deliberate process-local adapter
  from a native named mutex request.

## Final assessment

SR-AUD-208 is confirmed by direct current-.NET comparison. No production or
test source was changed.
