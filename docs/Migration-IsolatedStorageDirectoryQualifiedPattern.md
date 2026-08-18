<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `GetFileNames`/`GetDirectoryNames` honour a directory-qualified pattern (ticket #2209)

*2026-08-18.* `IsolatedStorageFile::GetFileNames("sub/*")` now lists `sub`'s files. It used to
return nothing, because both doors iterated the store root and matched the **whole** pattern
against a bare filename.

This is a **widening** — every pattern that worked before returns the same answer — with one
narrowing that closes a hole rather than opening one (§3). Landed under
`docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `GetFileNames("sub/*")` | `[]` | `["nested.dat", "other.txt"]` |
| `GetFileNames("sub/*.dat")` | `[]` | `["nested.dat"]` |
| `GetDirectoryNames("sub/*")` | `[]` | `["deeper"]` |
| `GetFileNames("sub/")` | `[]` | everything in `sub` |
| `GetFileNames("nosuchdir/*")` | `[]` | `[]` (unchanged — absent is empty, not an error) |
| `GetFileNames("*")`, `"one*"`, … | — | **unchanged** |
| `GetFileNames("../*")` | `[]` | `ArgumentException` — §3 |

**Results are bare names, not sub-paths.** `GetFileNames("sub/*.dat")` returns `"nested.dat"`, not
`"sub/nested.dat"`. That is .NET's contract, and it is deliberate: `IsolatedStorageFile.cs:177`
maps each hit through `Path.GetFileName` precisely because the store exists to hide its own root.

## 2. The reference

.NET's own source states the contract in a comment above each method:

```csharp
// foo\abc*.txt will give all abc*.txt files in foo directory
public string[] GetFileNames(string searchPattern) { … }

// foo\data* will give all directory names in foo directory that starts with data
public string[] GetDirectoryNames(string searchPattern) { … }
```

Both delegate to `Directory.EnumerateFiles(RootDirectory, searchPattern)`, and
`FileSystemEnumerableFactory.NormalizeInputs` does the splitting:

```csharp
ReadOnlySpan<char> directoryName = Path.GetDirectoryName(expression.AsSpan());
if (directoryName.Length != 0)
{
    directory = Path.Join(directory.AsSpan(), directoryName);
    expression = expression.Substring(directoryName.Length + 1);
}
```
*(`FileSystemEnumerableFactory.cs:45-56`.)*

The split is at the **last** separator, and a trailing separator leaves an empty expression that
becomes `"*"` — `NormalizeInputs`' own comment says *"We also allowed for expression to be `foo\`
which would translate to `foo\*`"*.

## 3. One deliberate narrowing, on a security boundary

**.NET does not confine the search pattern.** `GetFileNames` and `GetDirectoryNames` are the only
two doors on `IsolatedStorageFile` that bypass `GetFullPath`, so in .NET
`GetFileNames("../*")` escapes the store and lists its parent directory.

This port resolves the directory half through the same `fullPath()` every other door uses, so it
raises `ArgumentException`. Reproducing .NET here would mean **opening a confinement hole to match
a reference that has one**. The port is more restrictive, which is the direction SA-8 does not
reach, and a test pins the asymmetry so it stays a decision rather than becoming an accident.

Containment is about where the path **lands**, not which characters it contains, so
`GetFileNames("a/../a/*")` is fine.

## 4. A second, older divergence this ticket records rather than introduces

.NET rejects a **rooted** pattern outright:

```csharp
if (Path.IsPathRooted(expression))
    throw new ArgumentException(SR.Arg_Path2IsRooted, nameof(expression));
```
*(`FileSystemEnumerableFactory.cs:29-30`.)*

This port's `fullPath()` strips leading separators at **every** door, so `"/x"` has always meant
*x relative to the store*. `GetFileNames("/*")` therefore lists the root and `GetFileNames("/etc/*")`
looks for `<store>/etc`. Rejecting a rooted pattern only here would make the type inconsistent
with itself, which is worse than a divergence that is at least uniform. Both rows are pinned.

## 5. To migrate

Nothing to change. If you previously worked around the defect by enumerating a subdirectory
yourself, the pattern form now works:

```cpp
// workaround, still correct
for (const auto& name : store.GetDirectoryNames("*")) { /* … */ }

// now available
const auto saves = store.GetFileNames("saves/*.sav");   // bare names
```

## 6. Evidence

| Mutation | Caught |
|---|---|
| Never split — glob the whole pattern against the root (the pre-#2209 code) | ✅ (2 tests) |
| Split at the **first** separator instead of the last | ✅ |
| Return the sub-path instead of the bare name | ✅ (2 tests) |
| Skip the confinement check on the directory half | ✅ |
| A trailing separator no longer means `"*"` | ✅ |

## 7. Downstream, measured

Per SA-2 condition 5: `mobile-eggbert` uses `IsolatedStorageFile` in one file
(`src/WindowsPhoneSpeedyBlupi/Worlds.cpp`) and calls only `GetUserStoreForApplication`; it never
calls either enumeration door. `cna` has its own
`Microsoft::Xna::Framework::Storage::StorageContainer::GetFileNames`/`GetDirectoryNames`, which
this change does not touch. **Zero affected sites in both.** Neither repository was modified.
