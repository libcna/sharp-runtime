<!-- SPDX-License-Identifier: MIT -->
# Filter-crossing rename test synchronization

During CNA-DEMOS-ISSUE-147 validation, the full component gate failed the unchanged
`WatcherReconfigurationFixture.ARenameAcrossTheFilterIsADeletedOrACreated` test.
The IO executable was not rebuilt by the XML repair. A fixed twenty-iteration
diagnostic of that test produced18PASS/two FAIL, both at the immediate lookup for
`Deleted("leaving.txt")`. The original full gate and diagnostic failures remain in
`build-probe/cna-repair-xml-nul-components-first-failed.log` and
`build-probe/cna-repair-xml-nul-watcher-diagnostic.log`.

Root cause: the test awaited a Created sentinel and assumed all earlier filesystem
operations had already dispatched callbacks. FileSystemWatcher.cpp records unmatched
IN_MOVED_FROM events and dispatches their Deleted callbacks after the read-batch loop
(lines459–568); the sentinel Created callback can occur inside that loop. This is
not a barrier for deferred rename events. No production watcher behavior changed.

The test now uses its existing condition-variable `awaitEvent` for both required
events, retaining the same types/names, the sentinel, the forbidden Renamed events
and handler-failure checks. The existing bounded timeout is a failure deadline,
not a sleep. There are no extra retries, larger timeouts or suppressed assertions.
Validation uses a fixed twenty complete34-test watcher-suite iterations, followed
by the entire normal component gate; reports use `cna-repair-watcher-barrier-*`.

The corrected test passed all20 iterations. The complete stress run nevertheless
ended nonzero:678PASS/two FAIL across680 executions. Different tests,
`TheDefaultFilterIsCompletelyUNAFFECTED` (iteration16) and
`Decision5b_AppliesToDeletedAndToBothHalvesOfARename` (iteration18), missed Renamed
callbacks. These remain uncorrected, separately recorded watcher findings; this
barrier change does not claim to repair production rename pairing across reads.
The next normal component gate passed all726 IO tests but failed two Xml.Linq
tests whose old NUL exception expectations needed reference-backed correction.
Both failed gates and the stress failures are retained, not retried away.

After the separate reference-backed XML/LINQ repair, the full normal gate passes
18146/18146 across41 executables, zero failures/skips. The original XNA fixtures
are provided through supported XNA_SAMPLES_ROOT and the original Yacht service
runs on an isolated private port. Final summary is retained as
build-probe/cna-repair-xml-linq-components-final.log. This normal-suite pass does
not erase or resolve the two independent stress-only rename findings above.
