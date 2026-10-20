# Preserve native FileStream path-status failures

CNA-DEMOS-ISSUE-145 revealed that FileStream discarded filesystem status errors
and treated every non-regular path as absent. An overlong path therefore became
FileNotFoundException (or generic IOException), hiding the real failure from
TitleContainer's exception translation. Inspect status once and retain its error:
filename_too_long becomes PathTooLongException, denied access becomes
UnauthorizedAccessException, other non-missing errors become IOException. Genuine
absence and missing parents keep their previous categories and create behavior.

Reference: `/rv/tmp/runtime/src/libraries/Common/src/Interop/Unix/Interop.IOErrors.cs:114-152`
maps ENOENT/ENOTDIR separately from EACCES/EPERM and ENAMETOOLONG. This is .NET11
preview. No signatures, layout or vtable change. Callers that caught a misleading
FileNotFoundException for these errors must handle their actual category instead.

The previous54-check invalid-name/data-preservation oracle remains intact. Its
18 new long-path cases pass on .NET8.0.31 (72/72 total). Genuine Framework4 differs
in exactly Append/ReadWrite validation timing: it reaches full-path validation
before refusing read access for append. The initial universal expectation fails
71/72 and both raw reports remain; the explicit `--framework4` reference mode
records that difference. Sharp retains current .NET's argument-first validation,
derived from FileStreamHelpers.cs:102-115; no XNA OpenStream case uses Append.
Neither version's behavior is represented as timeless or cross-platform parity.

New native tests preserve the first passing invalid-path matrix and add a failing
18-case status matrix. The consumer Title path-length test also fails after
removing its masking existence precheck, while its other16 focused tests pass.
Those native failures are retained, not reclassified as expected suite successes.
The first expanded sanitizer harness did not link File helpers; its linker failure
is retained and is not a runtime sanitizer result.

The complete production build is warning-free. All18,144 tests across41
executables pass, zero failures/skips (18,143 previous plus the new status matrix).
Reports: `build-probe/cna-repair-file-status-{before,build,components-driver}.log`,
`build/gs-007b-components-live.log`. The version-specific genuine witness passes
72/72, as does .NET8.0.31 with its own recorded precedence. Original XNA XML
fixtures and the private unchanged Yacht SOAP fixture were used; the service was
stopped/removed afterward. Repaired CNA validation is pending. Precise after-open
errno mapping and FileShare remain separate work. No push or merge.
