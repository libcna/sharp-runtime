# Audit: `modules/io-isolated-storage/include/System/IO/IsolatedStorage/IsolatedStorageFile.hpp`

## Metadata

- AUDITED: public root-backed store API, relative-path promise, file/directory
  operations, lifecycle, and space properties.
- Validation: direct C++ probe constructed a temporary-root store and passed an
  absolute temporary directory to CreateDirectory; dependent IO fixture passed
  527/527.

## SR-AUD-241 — high — an absolute caller path bypasses the isolated-storage root

The public contract says paths are relative to the storage root, but
`fullPath()` uses `rootDirectory_ / relativePath` without rejecting or
normalizing a rooted path.  On the audited POSIX platform, a rooted right-hand
`std::filesystem::path` discards the store root.  The safe `/tmp` probe prints
`escaped_exists=1 root_child_exists=0` after
`store.CreateDirectory(absoluteOutsidePath)`: the directory is created outside
the store.  Current .NET's `GetFullPath` removes leading directory separators
before combining with RootDirectory, so an absolute POSIX input stays beneath
the managed store.  The same C++ helper feeds file existence/open/create/delete,
copy/move, and directory operations.

## Assessment

The public surface is a useful practical subset, but its defining root
containment invariant is false for absolute POSIX caller paths.  Scope and
quota policy are separately documented adaptations; SR-AUD-241 is a direct
behavioral contradiction and gives arbitrary filesystem access wherever a
caller can select the relative-path argument.

## Other missing assertions and diagnostics

- Add absolute-path rejection/normalization tests for every file/directory
  entry point (SR-AUD-241), plus traversal and symlink-boundary tests with
  platform-specific rooted syntax.
- Add create/open/read/write/copy/move/delete/list lifecycle coverage,
  null/empty/path separator errors, non-empty directory deletion, disposal,
  Remove failures, quota/used/free-space behavior, and scope-root separation.

## Final assessment

SR-AUD-241 is directly reproduced. No source or test was changed during this audit.


---

## Correction appended 2026-08-10 (#2203 review, #2204 remediation)

*The original text above is unchanged. This section records what the review measured that the
audit pass did not.*

**SR-AUD-241 is remediated (#2204).** Its premise was correct and its scope was understated in
four ways, each measured by `build-probe/2203_probe1_confinement.cpp` against a
repository-local sandbox:

1. **Thirteen caller path arguments across ten members**, not one door. `CreateDirectory` was
   the demonstrated example; `FileExists`, `DirectoryExists`, `OpenFile`, `CreateFile`,
   `DeleteFile`, `CopyFile` (**both** arguments), `MoveFile`, `MoveDirectory` and
   `DeleteDirectory` escape identically.
2. **All four effect classes escape**, not just creation: an outside file was *read*, an outside
   file and an outside directory were *deleted*, files were *created and written* outside, store
   content was *moved out*, and outside content was *imported in* through `CopyFile`'s source and
   `MoveDirectory`'s source. A read-only escape is a defect in its own right.
3. **`..` traversal and symbolic links escape too**, so the .NET repair this report names --
   stripping leading separators -- is necessary and **not sufficient**. Measured:
   `../outside/x`, `a/../../outside/x`, and symbolic links at final, intermediate and chained
   components.
4. **`DeleteFile("")` deleted the store root**, because `fullPath("")` is the root and
   `std::filesystem::remove` removes an empty directory. Separately measured; it is what makes
   an empty-path rejection load-bearing.

Also measured and closed in the same batch, as ordinary tickets (**no `SR-AUD-` identifier was
created; numbering stays frozen at 364**):

- **#2205** -- `Remove()` and the three space properties never checked the disposed flag while
  the other ten members did, so a closed store still reported its size and still deleted itself.
- **#2206** -- the constructor and the three iterator-backed members let a native
  `std::filesystem_error` cross the public API.

Two residuals are recorded rather than folded into the finding, so its remediated status is not
overclaimed: the **check-then-use TOCTOU race** (#2207, blocked -- needs `openat(O_NOFOLLOW)`
per-component resolution, an fd-accepting `FileStream`, and a Windows/Emscripten story) and
**`IsolatedStorageFileStream`'s unconfined public constructor** (#2208, blocked on a public
signature change).

The report's "Other missing assertions" list is now satisfied by the module's first dedicated
test executable, `SharpRuntimeTests_IO_IsolatedStorage` (58 tests): absolute-path rejection at
every entry point, traversal and symlink-boundary tests, create/open/read/write/copy/move/delete/
list lifecycle coverage, empty and separator errors, non-empty directory deletion, disposal,
Remove failure, quota/used/free-space behaviour, and a discriminating control proving the raw
join still escapes.
