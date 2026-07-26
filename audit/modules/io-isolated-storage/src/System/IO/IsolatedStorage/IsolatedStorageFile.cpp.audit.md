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
