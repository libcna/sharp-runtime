<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration: `IsolatedStorageFileStream` is confined to its store (#2208)

**Landed:** 2026-08-19, branch `next`. **Ticket:** #2208.

## What changed

`System::IO::IsolatedStorage::IsolatedStorageFileStream`'s public constructor was

```cpp
IsolatedStorageFileStream(const std::filesystem::path& fullPath, System::IO::FileMode mode);
```

It opened whatever path it was handed, **anywhere on the filesystem**, and created that path's
missing parent directories on the way. It is now

```cpp
IsolatedStorageFileStream(const std::string& path, System::IO::FileMode mode);
IsolatedStorageFileStream(const std::string& path, System::IO::FileMode mode,
                          const IsolatedStorageFile& store);
```

Both forms resolve `path` against a store's root through that store's own `fullPath()` — the same
resolver `OpenFile`, `CreateFile`, `DeleteFile` and `MoveFile` already used — so a path that
escapes the store is refused **before any filesystem access happens**.

## Why this is more than a tidy-up

The type exists to confine file access, and this was the one door on it that confined nothing.
It was a **wider hole than the TOCTOU #2207 declared and accepted**: that race needs an attacker
who can already write inside the store root, and a window between check and use. This needed
neither a race nor a privilege — only the call.

## The reference corrected this ticket's proposed shape

#2208 was written as *"remove the path constructor and take the owning store instead"*, and that
is **not** what .NET does. .NET publishes **eight** constructors and every one begins
`(string path, FileMode mode, ...)` with the store as an **optional trailing** parameter,
`IsolatedStorageFile? isf` (`IsolatedStorageFileStream.cs:21-56`).

Its storeless form is **not unconfined**. When `isf` is null it takes
`IsolatedStorageFile.GetUserStoreForDomain()` and then resolves through `isf.GetFullPath(path)`
exactly as the store-taking form does (`:82-118`). So the confinement this ticket exists to add
is obtained **without removing an overload .NET publishes** and without inventing a leading-store
parameter order.

That correction removed the source break the ticket was priced around. An earlier cut of this
work used `(store, path, mode)`; it is now pinned as **not compiling**, because the wrong order
would compile perfectly well as an *addition* and would leave this port with two spellings where
.NET has one.

## Is this a source break?

**On POSIX, essentially no.** `std::filesystem::path` converts to `std::string` implicitly there
(`path::operator string_type()`, and `value_type` is `char`), so an existing
`IsolatedStorageFileStream(somePath, FileMode::Open)` still compiles.

**Its meaning changes**, and that is the repair rather than a side effect: the argument used to be
a filesystem path and is now a path relative to the store. An absolute path is **contained, not
refused** — `fullPath()` strips leading separators at every door, so `"/etc/passwd"` now names
`<store-root>/etc/passwd`. That rule is older than this ticket (#2209 recorded it) and refusing it
at this one door alone would make the type inconsistent with itself.

**On Windows it is a source break**, because `std::filesystem::path::value_type` is `wchar_t`
there and no implicit conversion to `std::string` exists. Callers pass `p.string()`.

## Migration

Measured on 2026-08-19:

| Consumer | Constructions | Note |
|---|---|---|
| `cna` | 0 | no occurrence of the type at all |
| `mobile-eggbert` | 0 | one `#include` in `WindowsPhoneSpeedyBlupi/Worlds.cpp:60`, no construction |
| first-party | 1 | `IsolatedStorageFile::OpenFile`, which already held the store it now passes |

So the migration is **empty**. Where a call does exist:

```cpp
// before
IsolatedStorageFileStream s(store.getRootDirectoryProperty() / "save.dat", FileMode::Create);
// after -- the store resolves the path
IsolatedStorageFileStream s("save.dat", FileMode::Create, store);
```

## Two behaviours that came with it

- **The mode is validated.** A `FileMode` outside the six defined values is rejected with .NET's
  own text, `Invalid mode, see System.IO.FileMode.` (`SR.IsolatedStorage_FileOpenMode`), before
  the file is created. Every named `FileMode` is legal, so only a value cast in from outside the
  enumeration can reach it.
- **Each door reports its own parameter name.** The public constructors name `path`; `OpenFile`
  and `CreateFile` still name `relativePath`, which is what their own parameters are called.
  Routing every door through one resolver made it possible for one door's diagnostic to name
  another's parameter, which is the shape #2323 rules out.

## What .NET's check this port does not reproduce

.NET additionally rejects a path equal to `"\\"` (`SR.IsolatedStorage_Path`). On POSIX a backslash
is an ordinary file-name character — this module's own `isDirectorySeparator()` says so in a
comment — so reproducing that literal test would reject a legitimate name. The outcome is
nonetheless the same on both platforms: `fullPath()` strips leading separators and rejects what is
left when it is empty, which covers `"/"` on POSIX and both `"/"` and `"\\"` on Windows.

## New public member

`IsolatedStorageFile::GetUserStoreForDomain()` was added, purely additively, because it is the
store .NET's storeless constructor defaults to. It uses .NET's exact scope combination
(`Assembly | Domain | User`, `IsolatedStorageFile.cs:466-469`). Like this port's other two
factories it resolves to the same storage root, so the scope is **recorded rather than reflected
in the directory** — which is why a mutation swapping it for `GetUserStoreForApplication()` is an
unobservable equivalence here. It is written as `ForDomain` for fidelity to the reference, not
because anything in this port can tell the difference.

## Evidence

Eight mutations, seven caught, one a proven equivalence (the store scope, above). Three were
invalid as first written — `-Werror=unused-parameter`, `-Werror=unused-function`, and a
`[[nodiscard]]` on `fullPath()` — and were reformulated rather than counted. The mutation that
only the new cases catch is the **half-repair**: `OpenFile` pre-resolving while the constructor
ignores its store, which leaves the direct door open while every pre-existing confinement test
still passes.

The other seven are caught by **pre-existing** confinement tests, which is the point: `OpenFile`
now routes through the constructor, so the whole shipped confinement suite covers it.

Negative consumer fixture: `test/consumer/io_isolated_storage_stream_confinement_negative.cpp`,
four sites — the store's private `fullPath()`, the private resolving constructor, the private
`Resolve()`, and the store-first parameter order that .NET does not have.
