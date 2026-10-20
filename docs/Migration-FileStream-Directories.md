# FileStream directory-open exception categories

CNA-DEMOS-ISSUE-145 needs the underlying file-open failure to remain distinct
from a missing file before TitleContainer translates it. Existing directories
previously produced FileNotFoundException for Open/Truncate and a generic
IOException for other valid modes. Valid directory opens now produce
UnauthorizedAccessException, except CreateNew, which retains the existing-path
IOException. Invalid mode/access combinations still throw ArgumentException first.
No directory or existing child file is modified.

Reference: `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/Microsoft/Win32/SafeHandles/SafeFileHandle.Unix.cs:380-390`
rejects directory handles with EACCES; write opens fail with EISDIR. This is a
.NET 11 preview snapshot. Independently, genuine .NET Framework 4 agrees on all
18 FileMode/FileAccess combinations in
`cna-demos/docs/repair-evidence/storage/FileShareEdgesOracle.cs` (36 total checks,
zero failures). The new native matrix fails before the repair.

Callers distinguishing missing files from denied directory access should catch
UnauthorizedAccessException for the latter. There is no signature, public spelling,
layout, vtable or ownership change. This patch does not implement FileShare or
claim an atomic filesystem-open implementation. Those are separate tracked work.

The existing full build is warning-free. The complete gate passes18,141/18,141
tests across41 executables, zero failures/skips (18,140 previous checkpoint plus
the new18-case matrix test). Reports: `build-probe/cna-repair-directory-build.log`,
`build-probe/cna-repair-directory-components-driver.log`, and
`build/gs-007b-components-live.log`. Original XNA XML samples and the unchanged
private Yacht SOAP fixture were used; the fixture was stopped/removed afterward.
CNA TitleContainer and external demos remain pending. No push or merge.
