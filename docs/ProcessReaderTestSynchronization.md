<!-- SPDX-License-Identifier: MIT -->

# Concurrent process-reader test scheduling (2026-10-20)

During the CNA-DEMOS-ISSUE-148 dependency regression, the full component gate
failed in `Fix2030_ConcurrentReadersSeeAConsistentBuffer`. A fixed 100-iteration
diagnostic on the unchanged binary reproduced one failure: `reads == 0`.
The short child could finish before any of the four worker threads was scheduled;
the main thread then set `stop`, allowing all workers to exit without a snapshot.
This was a test setup race, not evidence of an incorrect Process buffer.

The test now waits on a four-party latch, counted down after each worker's first
actual snapshot, before allowing the main thread to stop the workers. The minimum
read assertion is strengthened from greater than zero to at least four. Exact
final output and each snapshot's existing checks are unchanged. There is no
production change, sleep, retry, ignored assertion, or new public API.

Validation uses the existing `build/` tree and shared ccache
(`CCACHE_DIR=/rv/cnaccache`, `CCACHE_BASEDIR=/rv`), maximum two compilation jobs:

- Incremental whole-library build: zero errors/warnings.
- Fixed 100-iteration post-repair stress: 100 passes; not retry-until-green.
- Whole Diagnostics suite: 233 passes, zero failures/skips.
- Full 41-executable gate with the concurrent XML candidate: 18,151 passes,
  zero failures/skips. The original Yacht SOAP fixture ran on a private loopback
  port through `run_component_tests_with_soap_fixture.py`, with
  `SHARP_RUNTIME_BUILD_JOBS=2` and `CMAKE_BUILD_PARALLEL_LEVEL=2`.

The original failed gate and both fixed-length diagnostic logs are retained in
`../cna-demos/build/linux-full/reports/repair-xml-pi-*`. The unrelated XML parser
repair has its own commit and provenance document. Only Linux was runtime-tested.
