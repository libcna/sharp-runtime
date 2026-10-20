# Reject invalid FileStream names before opening or truncating

CNA-DEMOS-ISSUE-145 path validation exposed a lower-layer data-loss defect:
an embedded NUL in a std::string path was passed through to native file operations,
which used only the prefix. Create/Truncate could therefore erase an unrelated
prefix-named file; Open could read it instead of rejecting the invalid argument.
FileStream now rejects empty names and embedded NUL with ArgumentException before
filesystem effects. Callers must pass the intended full, nonempty name.

Reference: `/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/IO/Strategies/FileStreamHelpers.cs:64`
rejects empty input before mode/access validation;
`System/IO/Path.Unix.cs:28-33` rejects NUL before native path handling. This is a
.NET11 preview snapshot. Genuine .NET Framework4 independently passes all54 checks
in `cna-demos/docs/repair-evidence/storage/FilePathValidationOracle.cs`, covering
all18 mode/access pairs, both invalid inputs and preservation of existing bytes.
The corresponding native matrix fails before repair, including real truncation
of its own uniquely created fixture; no user file is used.

No signature, layout, vtable or ownership change. This does not change FileShare
policy or claim comprehensive validation of every other filesystem API. CNA's
TitleContainer must let the canonical stream validate invalid names even when
an earlier existence check would otherwise mask the error as a missing file.

Full existing build is warning-free. The complete component gate passes18,143/
18,143 across41 executables, zero failures/skips (18,142 previous plus this new
matrix test). Original XNA XML fixtures and the private unchanged Yacht SOAP
fixture were used; the fixture was stopped/removed afterward. Reports:
`build-probe/cna-repair-file-path-{before,build,components-driver}.log`,
`build/gs-007b-components-live.log`. CNA's actual rebuilt-library and external
validation are still pending. No push or merge.
