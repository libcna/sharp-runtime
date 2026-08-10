# Audit: `modules/io-isolated-storage/src/System/IO/IsolatedStorage/IsolatedStorageFile.cpp`

## Metadata

- AUDITED: root construction, path resolution, all file/directory operations,
  disposal, listing/glob behavior, and space accounting.
- Validation: direct C++ `/tmp` containment probe and a complete 527/527
  dependent IO fixture run.

## SR-AUD-241 — high — an absolute caller path bypasses the isolated-storage root

`fullPath()` returns `rootDirectory_ / relativePath` with no rooted-path
handling.  POSIX `std::filesystem` ignores the left operand for an absolute
right operand; a direct `CreateDirectory(absoluteOutsidePath)` probe creates
the outside directory and reports `escaped_exists=1 root_child_exists=0`.
Current .NET's `GetFullPath` first strips leading directory separators before
Path.Combine, keeping the equivalent absolute POSIX input under RootDirectory.
Every file/directory operation here uses the unsafe helper.  See the public
IsolatedStorageFile report for the complete reproduction.

## Assessment

The disposed checks, non-recursive DeleteDirectory change, file/list behavior,
and unlimited quota policy are locally coherent.  However, the root join is a
security boundary failure for absolute inputs.  Relative `..` and symlink
behavior require separate cross-platform policy evidence and are not counted
as additional findings in this pass.

## Other missing assertions and diagnostics

- Add exhaustive rooted-path and containment tests for SR-AUD-241, including
  all file/directory operations and platform path forms.
- Cover error-code translation, remove failures, simultaneous access,
  cancellation of partially-created directories, glob edge cases, recursive
  iterator errors, file-size overflow, and disposed space-property behavior.

## Final assessment

SR-AUD-241 is directly reproduced. No source or test was changed during this audit.


---

## Correction appended 2026-08-10 (#2203 review, #2204 remediation)

*The original text above is unchanged.*

**SR-AUD-241 is remediated (#2204).** `fullPath()` now takes the public parameter's name and
validates before any filesystem effect: reject an embedded NUL, strip leading directory
separators (.NET `GetFullPath` parity, so a rooted path is reinterpreted store-relative rather
than honoured), reject what strips to nothing, reject a surviving drive/UNC root, require lexical
containment via `lexically_normal` + `lexically_relative`, then require containment again after
`weakly_canonical` resolves every symbolic link in the existing prefix. The operation runs on the
lexically normalized path, so deleting a link still deletes the link.

**This report's own deferral is now answered.** It said *"Relative `..` and symlink behavior
require separate cross-platform policy evidence and are not counted as additional findings in
this pass."* Both were measured escaping and both are closed by the same resolver; the *policy*
question that genuinely needs cross-platform evidence is narrower than the report implied -- it
is the **TOCTOU** half only, recorded as #2207.

**A real bug was found while testing the repair**: `CopyFile`, `MoveFile` and `MoveDirectory`
validated inside the call expression, and C++ leaves call-argument evaluation order unspecified.
GCC evaluated right to left, so a doubly-invalid call named `destinationFileName` instead of
`sourceFileName`. Both arguments are now resolved into locals in declared parameter order, which
makes the validation order part of the contract rather than a compiler artefact.

The remaining items from this report's "Other missing assertions" list that were **not** closed
here are recorded as tickets rather than dropped: directory-qualified search patterns (#2209,
deferred -- the `/rv` reference tree is absent, and neither enumeration door can leave the root,
so this is a parity gap and not a confinement gap).
