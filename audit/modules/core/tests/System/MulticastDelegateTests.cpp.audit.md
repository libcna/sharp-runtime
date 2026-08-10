# Audit: `modules/core/tests/System/MulticastDelegateTests.cpp`

## Metadata

- Audit status: AUDITED (252-line direct fixture, fully read).
- Validation: `MulticastDelegateTests.*` passed 25/25 in the 70-test direct
  delegate filter on 2026-07-26.
- Reference basis: `MulticastDelegate.hpp`, `Delegate.cpp`, and local CoreCLR
  multicast composition/equality/removal code.

## Findings

The fixture accurately verifies a direct clone's dynamic type and one
plain-function equality case.  It deliberately treats Combine as returning a
base Delegate and compares list entries built from the identical pointers,
leaving SR-AUD-118 and SR-AUD-119 invisible.  Its one Remove case has a
single-entry value and cannot exercise the subsequence rule in SR-AUD-120.

## Other missing assertions and diagnostics

- Missing dynamic type after Combine/Remove, mismatch rejection, equal
  separate invocation lists, RemoveAll, last-sequence selection, and hash
  equality for equal multi-entry lists.
- Empty instance, target tracking, reflection methods, and DynamicInvoke are
  only adapted/stubbed paths; no test distinguishes their diagnostics.

## Final assessment

Green results cover the happy path but codify the base-result adaptation that
breaks concrete multicast delegate behavior.  No source or test was modified
during this audit.
