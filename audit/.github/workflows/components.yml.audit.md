# Audit: `.github/workflows/components.yml`

## Metadata

- Audit status: AUDITED (59 lines, full read).
- Subsystem: tracked Ubuntu component-boundary and documentation CI.
- Evidence: workflow matrix, `scripts/check_selective_components.sh`, and
  `NEXT.md`/`plan.md`/README claims about the selective matrix.

## Purpose

Runs per-component selective consumer builds, a full local compatibility gate,
and the pinned Doxygen-warning check on Ubuntu 24.04.

## Executive verdict

Needs attention: the CI workflow does not execute the complete ten-component
selective matrix that the repository documents and that the local script
defines.

## Findings

### SR-AUD-001 — medium — `Collections.Blocking` isolation is not covered by tracked CI

The workflow's `selective` matrix lists nine components: `Core.Base`,
`Text.Json`, `Net.Http.Headers`, `Net.WebSockets`, `IO.Compression`,
`IO.Compression.Zip`, `IO.IsolatedStorage`, `Security.Cryptography.Random`,
and `Xml.Linq`.  It omits `Collections.Blocking`.

In contrast, `scripts/check_selective_components.sh` defines ten entries and
explicitly includes `Collections.Blocking:blocking_collection.cpp`; the
planning documents claim a “ten-job selective matrix”, including a direct
`Collections.Blocking` consumer.  The full build verifies that the component
can compile as part of `All`, but cannot prove its independent consumer
closure or preserve the boundary that motivated the component split.

**Impact:** a regression in the direct `Collections.Blocking` target or its
consumer-visible dependency closure can merge while all tracked workflow jobs
remain green.  This is especially relevant because isolating `Threading` from
ordinary `Collections.Core` consumers is a documented architecture invariant.

**Follow-up evidence needed:** add the existing `Collections.Blocking` fixture
to the workflow matrix (or invoke the full local selective script in CI), then
verify both the direct fixture and the existing Text.Json negative closure
assertions.

## Positive findings

The full gate and Doxygen job are explicit rather than relying on a broad
unchecked build.  The Doxygen job pins Ubuntu 24.04, matching the version-
sensitive baseline policy.

## Final assessment

One medium CI coverage gap, indexed as SR-AUD-001.
