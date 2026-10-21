<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# CNA-DEMOS-ISSUE-101 — URI local paths and embedded-NUL file existence

2026-10-21. This is a necessary CNA consumer repair under the campaign request
and StandingApprovals SA-5, not a complete System.Uri port. No existing member
signature, data member, base class, vtable or noexcept specification changes.
Rebuild the static libraries and consumers to use the two additive non-virtual
getters. No public source spelling is removed.

## Behavior and migration

- `Uri::getIsFileProperty()` checks the file scheme case-insensitively.
- `Uri::getLocalPathProperty()` returns an unescaped UTF-8 local path, excluding
  query/fragment. Both new getters reject relative URIs with
  `System::InvalidOperationException`.
- LocalPath decodes valid percent-encoded UTF-8 once, retains malformed percent
  escapes/invalid UTF-8, compresses path dot segments and handles DOS/UNC paths.
  It preserves `%00` as an actual NUL byte; it is **not** a filesystem validator.
- Absolute DOS paths are classified as file URIs before generic one-letter
  schemes. Conversely, `file:/tmp/clip` and `file:relative` are relative under
  RelativeOrAbsolute; constructors requiring Absolute reject them. Use
  `file:///tmp/clip` for an explicit absolute Unix file URI.
- `File::Exists(path)` returns false when the string contains NUL. Previously
  native filesystem calls could truncate it and return true for an existing
  prefix. Pass the real complete filename; do not strip NUL as a workaround.

Existing component and string getters still retain the partial parser's spelling;
they have not become general canonicalizing/decoding APIs. Do not substitute
LocalPath for a serialized URI or use it to open remote HTTP resources. Implicit
DOS paths retain literal percent characters. Ordinary Unix file paths preserve
backslashes on Unix; DOS/UNC paths use backslashes on all systems. Windows file
separator handling is source-reviewed, **not runtime-validated on Windows**.

## Reference and version boundaries

Source reference is the local **.NET 11 preview**, not a claim that all .NET
versions match: `System.Private.Uri/src/System/Uri.cs` LocalPath771–780,
IsFile853–864, GetLocalPath987–1083, DOS classification3530–3556 and Unix file
syntax2077/4226; `UriHelper.cs`317–360 delegates UTF-8 percent decoding to
PercentEncodingHelper. `System.Private.CoreLib/src/System/IO/File.cs`91–119
calls Path.GetFullPath and catches ArgumentException for invalid paths.

An independent34-input oracle was compiled with genuine Framework4 csc and run
on Framework4 under Wine and .NET8.0.31/Linux. Both agree on ordinary escaped file
paths, relative guards, DOS/UNC classification and preserving NUL. Differences
are recorded, not hidden: Framework4 differs in malformed UTF-8 decoding, file
query text and backslash conversion. The candidate follows measured .NET8/Linux
and current source in those cases. It does **not** claim complete Framework4
LocalPath parity or native Windows execution. CNA's FromUriEXT is an FNA extension.

Historical SystemUriNamespaceReviewPlan §15 excluded LocalPath/IsFile and percent
decoding from that old audit. This consumer-driven addition narrows only that
boundary; it does not reopen unrelated URI canonicalization or IDN work.

## Validation and downstream impact

The completed runtime baseline has18,158/18,158 PASS across41 executables. The
candidate has18,193/18,193 PASS, zero failures/skips; URI311→345, plus one IO test.
The34 parameter rows pin actual .NET8 oracle outputs, including byte lengths for
NUL and non-ASCII values. The IO test checks an existing file's payload is unchanged.
The full build has zero warnings/errors. Module boundaries remain valid:
44 physical modules,109 edges. No vendor or module dependency changes.

The first baseline attempt had five fixture-path skips, not test assertion
failures. The accepted rerun uses XNA_SAMPLES_ROOT pointing at a symlink view of
the unchanged original ShipGame and Spacewar samples; no skip allowance is set.
The existing SOAP fixture harness supplies private loopback service as before.

Measured search on2026-10-21: `rg -n 'System::Uri|File::Exists'` over CNA `modules`
and `/rv/data/development/github.com/openeggbert/mobile-eggbert`, limited to cpp/hpp
and excluding vendor/build trees. Mobile Eggbert has zero matches. CNA's matches
include phone notification URI declarations/tests, TitleContainer validation,
Song URI overload/tests and the new Video factory/play guards/tests. UriFormatException
comment/spelling matches are not counted as constructor consumers. No Mobile
Eggbert files were modified. This is a spelling inventory, not a claim of runtime
coverage for all applications.

CNA production integration is authorized separately by the repair campaign.
Its original focused URI fixture reproduces5 failures/4 passing controls before
the change. Additional file-boundary fixture reproduces5 failures against the
old native library. CNA candidate native/full validation is currently running;
external renderer and sanitizer validation remain pending. This runtime green
gate does **not** close CNA-DEMOS-ISSUE-101.

Evidence is retained in cna-demos:
`docs/repair-evidence/media/UriLocalPathOracle.cs`, VideoUriContractTests.cpp,
VideoFilePathBoundaryTests.cpp and the repair checkpoint. Generated logs under
`build/linux-full/reports/repair-uri-local-path-oracle-*`,
`repair-uri-runtime-before-v2-*`, `repair-uri-runtime-after-v2-*` pin revisions,
source hashes, exact outcomes and memory events. Latest runtime run has
830,410,752-byte peak, no memory-limit/OOM events, unchanged frozen source hashes.

The user explicitly forbids campaign pushes without authorization; this overrides
the repository's immediate-push default. No remote publication is part of this fix.
