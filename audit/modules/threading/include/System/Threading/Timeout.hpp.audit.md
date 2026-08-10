# Audit: `modules/threading/include/System/Threading/Timeout.hpp`

## Metadata

- AUDITED: 20-line static timeout-constant declaration, fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27; direct wait, lock, spin, and join tests exercise
  `Infinite` behavior, while its direct assertion checks `Infinite == -1`.
- Related implementation evidence: audited wait primitives validate their own
  millisecond boundaries; remaining consumers are pending audit.

## Assessment

`Infinite` is -1 and `InfiniteTimeSpan` is -10,000 ticks, exactly one negative
millisecond in the project TimeSpan representation and matching current .NET.
The deleted constructor makes its static-only intent explicit.  No new source
defect is demonstrated.

## Other missing assertions and diagnostics

- No direct test asserts `InfiniteTimeSpan` exactly or converts it through
  every TimeSpan-taking wait API; the suite consequently cannot localize a
  ticks-versus-milliseconds unit regression.
- Tests do not distinguish the one allowed negative sentinel from `-2` at all
  native timeout entry points, overflowed TimeSpan values, or inconsistent
  exception precedence after disposal.  Existing wait reports record several
  such boundaries where they have been demonstrated.

## Final assessment

Both constants are correct and internally documented.  No source or test was
changed.
