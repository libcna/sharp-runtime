<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Random-byte boundary oracle repair

The full post-change gate for cna-engine UP-001's installed-package prerequisite
failed in the pre-existing `Rng2398ChunkingTests.TheChunkBoundaryIsHandledOnBothSides`.
No production source or existing test binary had changed (incremental build said
`ninja: no work to do`). Its 257-byte case checks the one-byte tail differs from
0xA5. A correct uniform random generator returns 0xA5 with probability 1/256, so
the test rejects legitimate output. The prior unmodified full baseline had passed
18,193/18,193 tests. A diagnostic run of the unchanged binary with
`--gtest_repeat=1000 --gtest_break_on_failure` stopped on iteration 293, explicitly
reporting `n=257 was filled only up to the first chunk` at line84. That failure is
retained, not reclassified as a product failure or suppressed.

The corrected oracle collects a fixed 32 independent draws for each boundary
length and checks that **every byte position** differs from the sentinel at least
once. It does not retry on failure or stop early on success. For a uniform source,
a particular position coincides in all draws with probability 256^-32 = 2^-256;
the union bound over all 1,793 checked positions is below 2^-245. An omitted byte
fails deterministically on every draw. This also strengthens the previous
multi-byte-tail check, which accepted a partially unwritten tail if any other
byte in that range changed.

Two deterministic tests validate the oracle itself: an intentionally omitted
position is rejected at the beginning/middle/end of all five boundary lengths,
and a legitimate sentinel-valued first draw is accepted when later draws differ.
Both assert all 32 draws ran. Other entropy, fork, range and window tests remain
unchanged. There is no production RNG, cryptographic algorithm, API or ABI change.
This is a separate test-correctness commit, not part of the package implementation.

Validation: the full unchanged producer build plus the corrected integration
tests passed 18,195/18,195 checks across 41 executables, zero failures/skips and
zero compiler warnings. The module-boundary validator also passed. This used
`build`, shared ccache, one compiler worker and the original XNA sample XML and
Yacht SOAP fixtures (private loopback endpoint, no external service mutation).
Evidence: cna-demos `repair-runtime-package-after-v2-{build,components,boundaries}.log`.
The earlier failed gate remains in `repair-runtime-package-after-v1-*`.
