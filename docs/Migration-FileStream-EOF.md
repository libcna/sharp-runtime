# FileStream position and writes after EOF

CNA-DEMOS-ISSUE-056 corrects the native stream state left behind by a short read.
The read still returns the actual number of bytes and never writes beyond that
count. Repeated reads at EOF still return zero; ReadByte still returns -1.

Previously, a short read left std::fstream's EOF/failure bits set. Position then
reported -1, a relative seek used the wrong origin, and a later write could be
silently lost. EOF is not an I/O error: the repair clears only the benign EOF/fail
state after saving the actual read count. It preserves badbit and failures not
associated with EOF.

Callers should use the read count to detect EOF, not a negative Position sentinel.
Position now remains at the true end. Relative seeks use that position, and writes
through a writable stream advance and persist normally after an EOF read. There
is no public signature, object-layout, ownership or exception-hierarchy change.

Reference: `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/IO/Strategies/OSFileStreamStrategy.cs:220-223`
updates the position by the returned read count. That checkout is .NET11 preview;
the same behavior was independently verified with13/13 checks against genuine
.NET Framework4 (`cna-demos/docs/repair-evidence/storage/FileStreamEofOracle.cs`).
The unchanged native implementation failed seven of those checks and all three
new FileStreamEofTest cases. The isolated candidate passes all723 IO tests (the
720-test baseline plus those three), with no failures or skips.

The production build is warning-free. The complete post-fix gate passes18,140
tests across41 executables, zero failures and zero skips (pre-fix18,137 plus the
three new regressions). Original local XNA XML fixtures and the unchanged Yacht
SOAP service were used; its private service process and temporary files were
removed afterward. Reports: `build-probe/cna-repair-eof-build.log`,
`build-probe/cna-repair-eof-components-driver.log`, `build/gs-007b-components-live.log`.
The earlier baseline log is preserved as
`build-probe/cna-repair-storage-baseline-components-passed.log`.

Downstream native CNA now passes all13 original EOF checks through its actual
rebuilt library, all18 storage tests, and1,408 of the exact1,412 prior consumer
tests (four unchanged fixture/capability skips). The complete runtime-source sweep
retains three deliberately introduced, still-open Title145 path regressions:
239 total,233 passed,three skipped,three failed; those failures were already
reproduced before this patch and are not relabeled as passes. The actual modified
FileStream.cpp and its three regression tests additionally pass ASan/UBSan with
leak detection. External CNA C ABI/demos validation remains pending behind the
separately frozen lifecycle checkpoint; no external closure is claimed here.

Existing `build/` and `build-probe/` are reused, with at most two compiler/test jobs
and CMAKE_BUILD_PARALLEL_LEVEL=2 for the fixture helper. Native CNA uses its existing
`cmake-build-debug/`; its affected CnaTests target builds warning-free. The broader
example build was deliberately interrupted, not reported as complete; its17 warnings
exactly match the original baseline. No new build tree, push or merge.
