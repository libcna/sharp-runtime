# Audit: `modules/threading/include/System/Threading/ApartmentState.hpp`

## Metadata

- AUDITED: 22-line public apartment-state enum, fully read.
- Validation: the complete `SharpRuntimeTests_Threading` executable passed
  359/359 on 2026-07-27; its direct tests assert STA=0, MTA=1, Unknown=2, and
  pairwise distinction.
- Related implementation evidence: Thread source and its apartment APIs remain
  pending dedicated audit.

## Assessment

The three public values and their numeric representations match current .NET
`ApartmentState`.  A scoped C++ enum is an explicit, type-safe native
adaptation and does not itself claim to create a COM apartment.  No new source
defect is demonstrated.

## Other missing assertions and diagnostics

- Tests do not pass invalid underlying casts through `Thread` apartment APIs,
  verify exception/result diagnostics, or exercise a native apartment-aware
  platform.
- The declaration does not state the platform-specific meaning of the values
  when no COM apartment mechanism exists; the pending Thread consumer audit
  must establish whether its documented behavior makes that adaptation clear.

## Final assessment

The enum values are coherent.  No source or test was changed.
