# Empty relative URI references

CNA-DEMOS-ISSUE-145 exposed a prerequisite in TitleContainer's relative-URI
validation: empty and whitespace-only references were rejected before UriKind
could permit them. Relative and RelativeOrAbsolute now accept these references
and retain their original text. Absolute and the one-argument constructor still
reject them; invalid UriKind still fails before parsing. TryCreate follows the
same constructor path. No public signature, layout or vtable changes.

Reference: `/rv/tmp/runtime/src/libraries/System.Private.Uri/src/System/Uri.cs:1920-1921`
returns EmptyUriString; `System/UriExt.cs:61-68,156-170` accepts the result as
relative when allowed by UriKind. The source is .NET11 preview; the genuine
.NET Framework4 diagnostic `cna-demos/docs/repair-evidence/storage/RelativeUriOracle.cs`
independently confirms these three inputs under all three kinds, including
OriginalString preservation. Its remaining long-input measurements are diagnostic
only; this change makes no claim about .NET-version-specific length limits.

The new native matrix fails against the exact committed pre-fix Uri.cpp from
551ced42 (compiled directly from git into a separate object and linked with the
unchanged dependencies). All constructor/TryCreate checks then pass with the repair.
Initial test-only
builds failed on a most-vexing-parse spelling in EXPECT_THROW and missing braces
around a gtest assertion; those logs are retained and the harness is corrected.
Neither compiler diagnostic is claimed as a runtime reproduction.

The full production build is warning-free; the complete gate passes18,142/18,142
tests across41 executables, zero failures/skips (the previous18,141 plus this new
matrix test). Reports: `build-probe/cna-repair-uri-empty-before.log`,
`build-probe/cna-repair-uri-empty-build.log`,
`build-probe/cna-repair-uri-empty-components-driver.log`,
`build/gs-007b-components-live.log`. Original XNA XML fixtures and the private
unchanged Yacht SOAP fixture were used and the latter stopped/removed afterward.
TitleContainer and external demo validation are separate gates; no CNA issue is
closed here. No push or merge.
