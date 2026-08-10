<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::IO::IsolatedStorage` root confinement (ticket #2204, SR-AUD-241)

*2026-08-10.* This change is **source-, ABI-, layout-, vtable- and `noexcept`-compatible**, and
**behaviour-incompatible on purpose** for paths that leave the storage root. Code that passes
ordinary relative paths — which is every in-repository caller — is unaffected, byte for byte;
that property is pinned by the whole `LegitimatePaths_*` group and by the unchanged 635/635 IO
and 893/893 integration suites.

## What was wrong

`IsolatedStorageFile`'s documented contract is *"All paths passed to methods are interpreted
relative to the storage root."* Its entire implementation of that contract was:

```cpp
std::filesystem::path IsolatedStorageFile::fullPath(const std::string& relativePath) const
{
    return rootDirectory_ / relativePath;
}
```

`std::filesystem::path::operator/` **discards the left operand** when the right one is absolute,
and performs no normalization at all. So the promise was false at **thirteen caller path
arguments across ten members**, and all four effect classes went with it:

| Input | What happened |
|---|---|
| `/absolute/outside/x` | the operation ran on `/absolute/outside/x` |
| `../outside/x` | the operation ran outside the root |
| `a/../../outside/x` | the operation ran outside the root |
| a symbolic link inside the store pointing out | followed, at final, intermediate and chained components |
| `""` | resolved to the root — and `DeleteFile("")` **deleted the store** |
| `"name\0tail"` | silently truncated to `name` |

Measured effects, not hypotheses: an outside file **read**, an outside file and an outside
directory **deleted**, files **created and written** outside, store content **moved out**, and
outside content **imported in** through `CopyFile`'s source and `MoveDirectory`'s source. On a
process running as root, a leading `/` reaches the filesystem root itself.

## What changed

Every caller path argument now goes through a validating resolver, **before any filesystem
access or mutation**:

1. an embedded NUL is **rejected** (it would truncate the name);
2. leading directory separators are **stripped**, matching .NET's
   `IsolatedStorageFile.GetFullPath` — a rooted path is *reinterpreted* as store-relative, not
   honoured and not rejected;
3. a path that strips to nothing is **rejected**;
4. a path still carrying a root after the strip (a Windows drive or UNC form) is **rejected**;
5. the path must be **lexically** contained once `.` and `..` are collapsed;
6. the path must **still** be contained after every symbolic link in the existing prefix is
   resolved.

Alongside it, `Remove()` and the three space properties gained the disposed guard the other ten
members already had (#2205), and four doors stopped letting a native `std::filesystem_error`
escape (#2206).

## What callers must change

| Previously | Now | Fix |
|---|---|---|
| `store.CreateFile("/save.dat")` created `/save.dat` | creates `save.dat` **inside** the store | usually nothing — this is the intended reading |
| `store.OpenFile(someAbsolutePath, …)` reached that path | reaches the mirrored path inside the store | pass a store-relative path |
| `store.DeleteFile("../other/x")` deleted outside | `ArgumentException` | pass a store-relative path |
| `store.FileExists(anythingOutside)` returned `true` | `false`, or `ArgumentException` for a traversal | pass a store-relative path |
| `store.DeleteFile("")` deleted the store root | `ArgumentException("relativePath")` | use `Remove()` to delete a store |
| a symlink inside the store pointing out was followed | `ArgumentException` | keep link targets inside the store |
| `store.getUsedSizeProperty()` on a closed store returned a number | `ObjectDisposedException` | query before closing |
| an unusable root threw `std::filesystem_error` | throws `IsolatedStorageException` | catch the module's own type |

The exception is always `System::ArgumentException`, carrying the offending parameter's declared
name (`relativePath`, `sourceFileName`, `destinationFileName`, `sourceDirectoryName`,
`destinationDirectoryName`) and one of three stable messages:

- `Path must not contain an embedded NUL character.`
- `Path must not be empty.`
- `Path must be relative to the isolated storage root.`

For a two-argument member the **source** is validated before the destination — the arguments are
resolved into locals in declared order rather than inside the call expression, because C++ leaves
call-argument evaluation order unspecified and GCC evaluated it right-to-left.

## What is deliberately still open

This is a **check-then-use** design. It rejects every path a caller can name, but it cannot
defeat a process that replaces a path component with a symbolic link between the check and the
operation. Closing that needs `openat(O_NOFOLLOW)`-style per-component resolution and an
fd-accepting `FileStream`, across three platforms — **ticket #2207**.

`IsolatedStorageFileStream`'s own public constructor takes a full path and is **not** confined;
confining it would change its public signature — **ticket #2208**. Its doc-comment now says so,
and a test pins the current behaviour so the day #2208 ships the pin inverts rather than passes
silently.

## Where this is stricter than .NET

.NET Core's isolated storage strips leading separators and stops there; it does not reject `..`,
and its own documentation declines to treat isolated storage as a security boundary. This port
keeps .NET's strip verbatim and adds lexical and link-resolved containment on top, because the
type's documented contract in *this* repository is confinement. Rule 2 of that difference: the
port is a strict **superset of rejections** — no input .NET rejects is accepted here.
