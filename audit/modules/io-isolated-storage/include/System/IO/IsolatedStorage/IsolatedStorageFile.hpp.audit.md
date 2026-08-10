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
