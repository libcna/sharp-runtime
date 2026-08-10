<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::IO::IsolatedStorage` namespace review plan (ticket #2203)

*Measured 2026-08-10 on branch `claude/remediation-batch-1804-namespace-b1yjh5`, GCC 13.3.0,
C++23, Linux. Every claim below is a measurement from `build-probe/2203_probe1_confinement.cpp`
(pre-repair log `build-probe/2203_probe1_before.log`) or a direct reading of the shipped
sources — nothing is inherited unverified.*

---

## 1. Scope and file inventory

`modules/io-isolated-storage` is the whole unit: **5 headers, 3 sources, 606 lines**, plus an
empty `tests/.gitkeep`.

| File | Lines | Role |
|---|---:|---|
| `include/System/IO/IsolatedStorage/IsolatedStorage.hpp` | 46 | abstract base: scope, space/quota defaults, `Remove`/`Close`/`IncreaseQuotaTo` |
| `include/System/IO/IsolatedStorage/IsolatedStorageScope.hpp` | 41 | scope flag enum + `operator|`/`operator&` |
| `include/System/IO/IsolatedStorage/IsolatedStorageException.hpp` | 30 | exception type |
| `include/System/IO/IsolatedStorage/IsolatedStorageFile.hpp` | 120 | **the store**; every path-taking door |
| `include/System/IO/IsolatedStorage/IsolatedStorageFileStream.hpp` | 33 | `FileStream`-derived wrapper |
| `src/.../IsolatedStorageException.cpp` | 29 | three constructors, `COR_E_ISOSTORE` |
| `src/.../IsolatedStorageFile.cpp` | 264 | `fullPath()`, glob, all operations, space accounting |
| `src/.../IsolatedStorageFileStream.cpp` | 43 | parent-directory preparation, Emscripten IDBFS sync |

Component metadata (`modules/io-isolated-storage/CMakeLists.txt`): `IO.IsolatedStorage`,
`PUBLIC_DEPENDENCIES Core.Base IO`, `PRIVATE_DEPENDENCIES Storage`. The store root comes from
`SharpRuntime::Storage::StoragePaths::GetIsolatedStorageRoot()` =
`std::filesystem::current_path() / ".cna_isolated_storage"` on desktop POSIX,
`/save/.cna_isolated_storage` on Emscripten, `SDL_GetPrefPath(...)` on Android.

### 1.1 Premise correction — **the module is not untested**

The inherited handoff (`NEXT.md` §7 caution 1) says the module "has ZERO tests today", citing
`modules/io-isolated-storage/tests/.gitkeep`. **That is true only of the module's own test
tree.** Measured: isolated storage already has **34 tests in three other executables**, wired
through `modules/io`'s `TEST_DEPENDENCIES IO.IsolatedStorage`:

| Location | Tests | Covers |
|---|---:|---|
| `modules/io/tests/System/IO/IOStreamTests.cpp` | 25 `IsolatedStorageFileTests` | open/create/copy/move/delete/list round trips, disposal, quota, scope |
| `modules/io/tests/System/IO/IOTests.cpp` | 9 `IsolatedStorageScopeTests` + `IsolatedStorageExceptionTests` | enum values, HResult, message |
| `tests/integration/Task39RemainingTests.cpp`, `Task42Tests.cpp` | referenced | integration usage |
| `test/consumer/io_isolated_storage.cpp` | — | selective-component isolation fixture |

**None of those 34 tests exercises containment.** So the correct statement is: the module has
no *dedicated test executable* and **no security coverage whatsoever**. The executable-count
consequence the handoff predicted still holds — adding
`modules/io-isolated-storage/tests/*.cpp` makes CMake's `CONFIGURE_DEPENDS` glob mint
`SharpRuntimeTests_IO_IsolatedStorage` (component key `MAKE_C_IDENTIFIER("IO.IsolatedStorage")`),
taking the gate from **37 to 38 executables**. That transition is recorded in §16.

---

## 2. Public-surface inventory

### 2.1 `IsolatedStorageFile` — every public member, classified by whether it takes a path

| Member | Path parameters | Effect class |
|---|---|---|
| `IsolatedStorageFile(const path&, IsolatedStorageScope)` | root (trusted, not caller-relative) | creates the root |
| `GetUserStoreForApplication()` / `GetUserStoreForAssembly()` | — | factory |
| `FileExists(relativePath)` | 1 | **read** (existence) |
| `OpenFile(relativePath, FileMode)` | 1 | **read + write + create** |
| `CreateFile(relativePath)` | 1 | **write + create + truncate** |
| `DeleteFile(relativePath)` | 1 | **delete** |
| `CopyFile(src, dst)` / `CopyFile(src, dst, overwrite)` | 2 | **read (src) + write (dst)** |
| `MoveFile(src, dst)` | 2 | **rename** |
| `GetFileNames(searchPattern)` | 0 (glob, not a path — see §2.3) | enumerate |
| `DirectoryExists(relativePath)` | 1 | **read** (existence) |
| `CreateDirectory(relativePath)` | 1 | **create** |
| `DeleteDirectory(relativePath)` | 1 | **delete** |
| `MoveDirectory(src, dst)` | 2 | **rename** |
| `GetDirectoryNames(searchPattern)` | 0 | enumerate |
| `Remove()` / `Close()` / `Dispose()` | 0 | lifecycle |
| `getAvailableFreeSpaceProperty()` / `getUsedSizeProperty()` / `getQuotaProperty()` | 0 | accounting |
| `getRootDirectoryProperty()` | 0 | discloses the root |

**Thirteen caller-supplied path arguments across ten members.** Every one of them reaches the
same private helper:

```cpp
std::filesystem::path IsolatedStorageFile::fullPath(const std::string& relativePath) const
{
    return rootDirectory_ / relativePath;   // <-- the entire confinement implementation
}
```

### 2.2 `IsolatedStorageFileStream` — a second, independent public door

`IsolatedStorageFileStream(const std::filesystem::path& fullPath, FileMode)` is **public** and
documented as taking an "Absolute filesystem path". It creates missing parent directories and
then behaves as a `FileStream`. It performs **no** containment check and does not know about
any store. See §11.2 and ticket #2208.

### 2.3 The enumeration doors are not path doors

Measured: `GetFileNames`/`GetDirectoryNames` iterate `rootDirectory_` only and match
`entry.path().filename()` against the pattern with a local `globMatch`. `GetFileNames("sub/*")`
returns **0** entries even though `sub/nested.dat` exists (`enum.GetFileNames.subdir_pattern.count=0`).
They therefore cannot escape the root — but they also cannot express .NET's directory-qualified
search patterns. That is a **parity gap, not a confinement gap**; recorded as #2209
(deferred verification), not as a security finding.

---

## 3. Audit findings owned by this module

`audit/AUDIT_FINDINGS_INDEX.md` re-parsed row-by-row at session start (robustly, by finding
identifier and status field — **not** with a fixed six-column regex, which drops SR-AUD-029's
seventh column and yields a false 363/169):

> **172 remediated / 137 confirmed (plain) / 55 confirmed (design-complete) / 364 total.**

Filtering that index for this module returns **exactly one row**:

| ID | Severity | Status | Source files |
|---|---|---|---|
| **SR-AUD-241** | **high** | confirmed | `IsolatedStorageFile.hpp`, `IsolatedStorageFile.cpp`, `IsolatedStorageFileStream.cpp` |

> *An absolute POSIX caller path makes `root / path` discard root, so file and directory
> operations escape their isolated store; current .NET strips leading separators first.*

The five other per-file audit reports (`IsolatedStorage.hpp`, `IsolatedStorageScope.hpp`,
`IsolatedStorageException.hpp`/`.cpp`, `IsolatedStorageFileStream.hpp`) each end with *"No
declaration-level defect was demonstrated"* and contribute only "Other missing assertions"
entries. **No finding is hidden by file/module mapping**: every one of the eight per-file
reports was read in full, and only the two `IsolatedStorageFile` reports carry an `SR-AUD-`
heading. Both carry the same one.

### 3.1 Selection justification

`modules/io-isolated-storage` was taken over the alternatives on six measured axes:

| Axis | Measurement |
|---|---|
| Severity | **high**, and the only *high* in the corpus that is neither blocked nor approval-gated |
| Filesystem security impact | **arbitrary read, write, create, delete and rename outside the store** (§6) — measured, including a directory created at the real filesystem root `/` on a uid-0 container |
| Caller-controlled input | **13 path arguments across 10 members**, all funnelled through one 3-line helper |
| Decidability | **complete** — needs no `/rv` reference tree, no ICU, no network, no downstream inspection |
| Bounded scope | 606 lines; the repair is one private helper plus its call sites |
| Namespace closure potential | **1 finding total** — closing it closes the compatible namespace |

Runners-up and why they lose: `diagnostics` (2 highs, both the blocked `Process` family
#2029/#2030), `net-sockets` (#2134 CCF-019-blocked, #2138 `needs_user`), `threading-tasks`
(#1970 blocked), `collections-object-model` and `text-regular-expressions` (low severity,
likely stateful-raw-`this`, i.e. CCF-019-shaped and therefore blocked). **No candidate
outranks it**, so the inherited selection is confirmed rather than merely adopted.

---

## 4. Premise corrections (all measured)

1. **The module is not untested — it is unsecured.** 34 tests exist in three other executables
   (§1.1); none touches containment.
2. **SR-AUD-241 is not one door, it is thirteen path arguments across ten members.** The audit
   demonstrated `CreateDirectory`; the escape is measured on `FileExists`, `DirectoryExists`,
   `OpenFile`, `CreateFile`, `DeleteFile`, `CopyFile` (**both** arguments), `MoveFile`,
   `MoveDirectory` and `DeleteDirectory` as well (§6).
3. **The escape is not read-only and not create-only.** It **deletes** files and directories
   outside the store (`abs.DeleteFile.victim_still_exists=0`,
   `abs.DeleteDirectory.victim_still_exists=0`) and **imports** outside content into the store
   (`abs.CopyFile.src.leaked_in_exists=1`, `abs.MoveDirectory.src.pulled_in_exists=1`). All four
   effect classes — read, write/create, delete, rename — escape.
4. **Lexical `..` traversal escapes too**, and the audit's own file report explicitly declined
   to count it (*"Relative `..` and symlink behavior require separate cross-platform policy
   evidence and are not counted as additional findings in this pass"*). Measured:
   `dotdot.CreateDirectory.escaped_exists=1`, `dotdot.CreateFile.escaped_exists=1`,
   `dotdot.repeated.escaped_exists=1` (`a/../../outside/mixed_dir`),
   `dotdot.FileExists.result=1`. **Stripping leading separators — .NET's exact repair — closes
   the audit's named probe and leaves every one of these open.** This confirms the inherited
   caution in writing.
5. **Statically-planted symlinks escape**, at both final and intermediate components and
   through a chain: `symlink.final.FileExists.result=1`,
   `symlink.intermediate.escaped_exists=1`, `symlink.intermediate.dir_escaped_exists=1`,
   `symlink.chain.escaped_exists=1`. A dangling symlink correctly reports non-existence
   (`symlink.dangling.FileExists.result=0`).
6. **`DeleteFile("")` deletes the store root.** `fullPath("")` is the root itself and
   `std::filesystem::remove` removes an empty directory: `root_delete.root_still_exists=0`.
   This is a **new, separately measured defect**, not part of SR-AUD-241's absolute-path
   premise, and it is what makes an empty-path rejection load-bearing rather than cosmetic.
7. **The escape reaches `/` on a privileged process.** The first probe run passed
   `"/leading_sep_dir"` and **created a directory at the real filesystem root** (this container
   runs as uid 0). It was removed by hand and the probe was re-scoped to a sandbox-relative
   rooted path so re-running can never write outside `build-tmp/`. The measurement is recorded
   here rather than repeated.
8. **Four public doors leak a native `std::filesystem_error`.** The constructor
   (`create_directories` with no `error_code`), `GetFileNames`, `GetDirectoryNames` and
   `getUsedSizeProperty` (unguarded iterators). Measured
   `native.ctor.root_is_a_file=std::filesystem_error` and the same for the other three. This
   is a post-audit defect (#2206) — the module's own contract throws `IsolatedStorageException`.
9. **Four members are missing their disposed guard.** `Remove()`,
   `getAvailableFreeSpaceProperty()`, `getUsedSizeProperty()` and `getQuotaProperty()` all
   return `no-throw` on a closed store, while ten other members correctly throw
   `ObjectDisposedException`. .NET's `IsolatedStorageFile` calls `EnsureStoreIsValid()` in all
   of them. Post-audit defect (#2205).
10. **An embedded NUL silently truncates the name.** `CreateDirectory("nul_probe\0tail")`
    creates `nul_probe` (`degenerate.nul.truncated_name_exists=1`) — the same shape as #2085
    and #2201 in the XML modules, here on a *filesystem* name.
11. **The enumeration doors are not confinement doors** (§2.3) — a parity gap, not a security
    gap. Recorded so the finding's door inventory is provably complete rather than silently
    narrowed.

---

## 5. The root-confinement invariant

> **RC.** For every public member of `IsolatedStorageFile` that accepts a caller-supplied path,
> the operation must either act on a filesystem object whose location — after resolving every
> symbolic link in every path component — is the store root or a proper descendant of it, **or**
> throw before performing any filesystem access, mutation or side effect.

Three things RC deliberately is **not**:

- RC is **not** "remove `/` and `\`". That is .NET's rule and it leaves §4.4 open.
- RC is **not** "reject `..`". That is a lexical rule and it leaves §4.5 open.
- RC is **not** a guarantee against an adversary who can write inside the store root
  concurrently. RC as specified is a **check-then-use** property; see §10.

RC's read half matters independently: `FileExists`/`DirectoryExists` returning `true` for a path
outside the store is an information disclosure even though nothing is modified. A read-only
escape is still a defect.

---

## 6. Complete path-door inventory — measured pre-repair

Every line below is from `build-probe/2203_probe1_before.log`. "escaped" means the effect
landed outside the store root.

| Door | Argument | Input class | Disposition | Escaped? |
|---|---|---|---|---|
| `CreateDirectory` | `relativePath` | absolute | `no-throw` | **yes** (the audit's probe: `escaped_exists=1 root_child_exists=0`) |
| `FileExists` | `relativePath` | absolute | `no-throw`, returns **true** | **yes (read)** |
| `DirectoryExists` | `relativePath` | absolute | `no-throw`, returns **true** | **yes (read)** |
| `OpenFile` | `relativePath` | absolute | `no-throw` | **yes (create+write)** |
| `CreateFile` | `relativePath` | absolute | `no-throw` | **yes (create+write)** |
| `DeleteFile` | `relativePath` | absolute | `no-throw` | **yes (deleted an outside file)** |
| `CopyFile` | `destinationFileName` | absolute | `no-throw` | **yes (wrote outside)** |
| `CopyFile` | `sourceFileName` | absolute | `no-throw` | **yes (imported outside content in)** |
| `MoveFile` | `destinationFileName` | absolute | `no-throw` | **yes (moved store content out)** |
| `MoveDirectory` | `sourceDirectoryName` | absolute | `no-throw` | **yes (pulled an outside tree in)** |
| `DeleteDirectory` | `relativePath` | absolute | `no-throw` | **yes (deleted an outside directory)** |
| `CreateDirectory` | `relativePath` | `../outside/x` | `no-throw` | **yes** |
| `CreateFile` | `relativePath` | `../outside/x` | `no-throw` | **yes** |
| `CreateDirectory` | `relativePath` | `a/../../outside/x` | `no-throw` | **yes** |
| `FileExists` | `relativePath` | `../outside/secret.txt` | returns **true** | **yes (read)** |
| `FileExists` | `relativePath` | symlink, final component | returns **true** | **yes (read)** |
| `CreateFile` | `relativePath` | symlink, intermediate component | `no-throw` | **yes** |
| `CreateDirectory` | `relativePath` | symlink, intermediate component | `no-throw` | **yes** |
| `CreateFile` | `relativePath` | symlink **chain** | `no-throw` | **yes** |
| `DeleteFile` | `relativePath` | `""` | `no-throw` | **the store root was deleted** |
| `MoveDirectory` | `sourceDirectoryName` | `".."` | `IsolatedStorageException` | no (rename refused by the kernel, not by a check) |
| `CreateDirectory` | `relativePath` | `"nul_probe\0tail"` | `no-throw` | no — but **silently truncated** to `nul_probe` |
| `IsolatedStorageFileStream` ctor | `fullPath` | any absolute | `no-throw` | **yes, by design** (§11.2) |

Inputs that are legitimate and **must keep working** (all measured `no-throw` + inside):
`"a//b"`, `"trailing/"`, `"./dot_seg"`, `"..hidden"` (a name that merely *begins* with `..`),
ordinary nested relative paths.

---

## 7. Lexical traversal analysis

`std::filesystem::path::operator/` performs **no** normalization: `root / "../outside/x"` is
literally `<root>/../outside/x`, and every POSIX syscall resolves `..` against the *real*
parent. `lexically_normal()` collapses `..` textually and is therefore the right tool for the
lexical half — but only the lexical half.

The check must be *relative*, not a string prefix. A `starts_with(root.string())` test is wrong
in two directions: it accepts `<root>xyz/evil` (sibling with a shared prefix) and it rejects a
correctly-contained path when the root string lacks a trailing separator. The design uses
`candidate.lexically_relative(normalizedRoot)` and rejects when the first component is `..` or
when the result is empty.

Deliberately **not** rejected: a `..` that is cancelled out (`a/../b` → `b`), and any name that
merely starts with `..` (`..hidden`). Rejecting those is the over-rejection mutation §15 tests
for.

---

## 8. Absolute-path analysis

.NET's `IsolatedStorageFile.GetFullPath` strips leading `Path.DirectorySeparatorChar` /
`AltDirectorySeparatorChar` and then `Path.Combine`s. That is **reinterpretation, not
rejection**: `"/etc/passwd"` becomes `<root>/etc/passwd`. This review adopts .NET's rule
verbatim for leading separators, because it is the behaviour the audit named and because it is
strictly more permissive than rejecting (no legitimate caller loses).

After the strip, a path may still be rooted on Windows (`C:\x`, `\\server\share\x`) because a
drive or UNC root is a `root_name`, not a leading separator. Those are **rejected**, not
reinterpreted — this port has no Windows drive semantics to reinterpret them into, and
inventing one is exactly what §"do not invent Windows behavior" forbids. On POSIX, `fs::path`
gives `"C:\\x"` no root at all, so it stays an ordinary (ugly) file name; that is POSIX-correct
and unchanged.

The strip is what makes an **empty** result reachable (`"/"`, `"///"`), which is why §9's empty
rejection runs *after* it.

---

## 9. Validation order and the exact exception contract

For each caller path argument, in this order, **before any filesystem call**:

| # | Check | Throws |
|---|---|---|
| 1 | embedded NUL | `ArgumentException("Path must not contain an embedded NUL character.", paramName)` |
| 2 | strip leading `/` (and `\` on Windows) | — (reinterpretation, .NET parity) |
| 3 | empty after strip | `ArgumentException("Path must not be empty.", paramName)` |
| 4 | still rooted (Windows drive/UNC) | `ArgumentException("Path must be relative to the isolated storage root.", paramName)` |
| 5 | lexical escape (`..` past the root) | `ArgumentException("Path must be relative to the isolated storage root.", paramName)` |
| 6 | symlink-resolved escape | `ArgumentException("Path must be relative to the isolated storage root.", paramName)` |

`paramName` is the header's own declared name: `relativePath`, `sourceFileName`,
`destinationFileName`, `sourceDirectoryName`, `destinationDirectoryName`. The disposed check
(`ObjectDisposedException`) stays **first** on every member that already has it — a closed store
must not report an argument problem it never looked at.

`ArgumentException` rather than `IsolatedStorageException` because the module already throws
`ArgumentException` for empty `CopyFile`/`MoveFile`/`MoveDirectory` arguments (three shipped
tests pin it), and an out-of-contract argument is an argument defect.
`IsolatedStorageException` stays what it is today: the wrapper for a *filesystem* failure on an
in-contract path.

**No native `std::` exception may cross any public door** (#2206 closes the four that do).

---

## 10. Symlink and TOCTOU analysis

### 10.1 What the repair achieves

Containment is verified with `std::filesystem::weakly_canonical` (`error_code` overload — the
throwing overload would itself violate §9) against a `weakly_canonical` store root.
`weakly_canonical` resolves every symlink in the existing prefix and appends the non-existent
remainder literally, which is exactly right for create/move destinations that do not yet exist.

This closes **statically planted** symlink escape: a link that exists at the moment of the call,
at a final component, at an intermediate component, or through a chain. It also correctly
**allows** a symlink inside the root whose target is also inside the root (matrix case 1) —
containment is judged on the resolved location, not on link-ness.

The operation is then performed on the **lexically normalized** path, not the canonicalized one,
so a final-component symlink keeps its current meaning (`DeleteFile` on a link removes the link,
not its target). Verification and use name the same object in the absence of a race.

### 10.2 What the repair does not achieve — the residual, stated plainly

The design is **check-then-use through a path name**. Between `weakly_canonical` and the
`std::filesystem` call, a process that can write **inside the store root** can replace a path
component with a symlink pointing outside, and the operation follows it. This is a genuine
TOCTOU race and it is **not** closed by this ticket.

Classification: *local, same-privilege, requires write access to the store's own directory tree.*
Under the store's threat model — the store root is application-private data and the untrusted
input is the **path string**, not the directory contents — the caller-controlled-path attack
(closed here) and the concurrent-directory-writer attack (not closed) are different adversaries.

Closing it requires per-component resolution with `openat(..., O_NOFOLLOW)` and fd-relative
`*at` operations, plus an fd-accepting `FileStream` primitive this port does not have, plus a
Windows equivalent (`NtCreateFile` / `FILE_FLAG_OPEN_REPARSE_POINT`) and an Emscripten story.
That is an architectural and platform-policy decision, not a bounded repair — **ticket #2207,
blocked**. Per the brief, the compatible lexical-plus-canonical repair lands now and the
stronger policy is a separate, explicitly-stated residual.

**SR-AUD-241's acceptance criterion is the absolute-path escape**, which is fully closed. The
TOCTOU residual is recorded as its own ticket rather than folded in, so the finding's remediated
status is not overclaimed.

---

## 11. Compatible versus architectural work

### 11.1 Compatible (this batch)

- **#2204** — RC across all thirteen path arguments; first dedicated test executable.
- **#2205** — the four missing disposed guards.
- **#2206** — the four native-`std::filesystem_error` leaks.

### 11.2 Architectural / gated (not this batch)

- **#2207** — TOCTOU-proof fd-relative confinement (§10.2). **Blocked**: cross-platform
  architecture + a new `FileStream` primitive.
- **#2208** — `IsolatedStorageFileStream`'s public constructor is an **unconfined primitive**.
  Measured: `stream_ctor.escaped_exists=1`. Confining it requires the constructor to take the
  owning store, i.e. a **public signature change**, and would break any consumer constructing
  one directly. **Blocked on approval.** Until then the header must say plainly that this
  constructor is not a confinement boundary — the doc-comment edit ships with #2204.
- **#2209** — the enumeration doors ignore directory-qualified search patterns (§2.3).
  **Deferred verification**: .NET's exact `GetFileNames("dir/*")` contract cannot be established
  without the `/rv` reference tree, which is absent.

### 11.3 Explicitly out of scope

Quota enforcement (`IncreaseQuotaTo` returning `false`, `Quota` = `long::MaxValue`) is a
documented adaptation, not a defect. Scope-to-root mapping (application and assembly stores
sharing one root) is a documented adaptation. Neither is touched.

---

## 12. Source / ABI / layout / vtable / noexcept consequences

| Aspect | Consequence |
|---|---|
| Public signatures | **unchanged** — every repair is inside existing bodies or a new **private** member |
| Object layout | **unchanged** — `IsolatedStorageFile` keeps exactly `rootDirectory_` + `disposed_`; the canonical root is computed per call, never cached in a member |
| Vtable | **unchanged** — no virtual added, removed or reordered; `Remove`/`Close` overrides keep their slots |
| `noexcept` | **unchanged** — nothing here was `noexcept` |
| Exception specification | **widened by design**: doors that previously escaped now throw `ArgumentException`. No door throws a *narrower* set. |
| Header ABI | one new private member function (`resolveContained`) — a new mangled symbol, no layout or vtable effect |

**Binary compatibility is therefore preserved**; behaviour compatibility is deliberately
narrowed, and §13 records exactly where.

---

## 13. Behaviour and platform consequences

Deliberate, documented strictness increases (see `docs/Migration-IsolatedStorageConfinement.md`):

1. A path that escapes the root lexically or through a symlink now throws `ArgumentException`
   instead of silently operating outside the store.
2. An absolute path is now **reinterpreted** as store-relative (.NET parity), not honoured.
3. An empty path — including `"/"` — now throws `ArgumentException` at every door, closing
   `DeleteFile("")`'s destruction of the store root.
4. An embedded NUL now throws instead of silently truncating the name.

Platform notes: the strip handles `\` only where it is a separator (`_WIN32`); POSIX keeps
`\` as an ordinary name character, unchanged. `weakly_canonical` is available on all three
supported toolchains; on Emscripten's virtual FS it resolves within the MEMFS/IDBFS tree, and
no platform-specific header enters a public `.hpp`. No `PlatformNotSupportedException` path is
added or removed.

---

## 14. Test matrix (permanent, `modules/io-isolated-storage/tests/`)

The new dedicated executable owns all of it. Every test builds its store root under a
repository-local sandbox derived from the process working directory (the gate runs executables
from `build/`), never `/tmp`, `/var/tmp`, `/dev/shm` or `$HOME`, and tears its own tree down.

| Group | Cases |
|---|---|
| **Audit probe** | the exact SR-AUD-241 `CreateDirectory(absolute)` reproduction, asserting rejection **and** that nothing was created outside |
| **Absolute path, per door** | all 11 escaping arguments of §6 |
| **Lexical traversal** | `..`, `../x`, `a/../../x`, repeated `..`, `..` as the whole path |
| **Legitimate paths still accepted** | `a/b`, `a//b`, `trailing/`, `./x`, `..hidden`, deep nesting |
| **Degenerate** | `""`, `"/"`, `"///"`, `"."`, separator-only, trailing separator, embedded NUL, long component, long path, Unicode, whitespace, hidden (`.dotfile`) |
| **Symlink** | in→in (**allowed**), in→outside file, in→outside dir, chain, intermediate component, final component, dangling |
| **Root protection** | `DeleteFile("")` and `DeleteDirectory("")` leave the root intact |
| **Atomicity** | before/after directory snapshots prove a rejected call creates, opens, deletes and renames **nothing**, inside or outside |
| **Exception contract** | exact type, exact `paramName`, disposed-before-argument ordering |
| **Disposed** (#2205) | `Remove`, `AvailableFreeSpace`, `UsedSize`, `Quota` |
| **Native leak** (#2206) | constructor and the three iterator doors surface `IsolatedStorageException`, never `std::filesystem_error` |

---

## 15. Evidence strategy

**Direct filesystem observation is the primary evidence.** Sanitizers do not prove confinement;
they are run for the memory/lifetime half only, and any non-discriminating result is reported as
such.

Discriminating controls, all required to move together:

- a **vulnerable control**: the shipped `rootDirectory_ / relativePath` join, kept in the probe
  so "escaped" and "rejected" are measured by the same harness;
- an **outside-root target** that exists and is observable before and after;
- a **symlink control** whose target is outside the sandbox's store root but still inside the
  sandbox;
- a **legitimate-path control** that must keep succeeding, so an over-rejecting repair fails
  loudly instead of looking like a pass.

Mutation testing (§"Mutation testing" of the brief) must distinguish: no traversal rejection;
absolute-path bypass; one door left unguarded; validation after a side effect; over-rejection of
valid nested relative paths; symlink policy disabled.

---

## 16. Tickets

| # | Title | P | Status | Notes |
|---|---|---|---|---|
| **#2203** | review `System::IO::IsolatedStorage` (this document) | P1 | **done** | — |
| **#2204** | SR-AUD-241 — root confinement across all 13 path arguments + first dedicated test executable | P1 | **todo → done** | gate 37 → **38** executables |
| **#2205** | four members missing the disposed guard (post-audit) | P2 | **todo → done** | no `SR-AUD-*` |
| **#2206** | `std::filesystem_error` crosses four public doors (post-audit) | P2 | **todo → done** | no `SR-AUD-*` |
| **#2207** | DESIGN — TOCTOU-proof fd-relative confinement | P2 | **blocked** | architecture + platform policy |
| **#2208** | DESIGN/APPROVAL — confine `IsolatedStorageFileStream`'s public constructor | P2 | **blocked** | public signature change |
| **#2209** | DEFERRED VERIFICATION — directory-qualified search patterns | P3 | **todo** | needs the absent `/rv` tree |

**Audit numbering stays frozen at 364.** No `SR-AUD-` identifier is created; #2205, #2206,
#2207, #2208 and #2209 are ordinary tickets.

**Implementation order:** #2203 (this plan) → #2204 (the security repair, which also mints the
test executable every later ticket needs) → #2205 → #2206 → reconciliation.

---

## 17. Residual risks

1. **TOCTOU (§10.2)** — a concurrent writer inside the store root can still win a race against
   the check. Open as #2207. This is the honest limit of a check-then-use design.
2. **`IsolatedStorageFileStream`'s public constructor (§11.2)** — an unconfined door that stays
   open until the #2208 approval. It is now documented as such rather than implied to be safe.
3. **Hard links** are not distinguishable from ordinary files by any portable API; a hard link
   planted inside the store to a file outside it resolves *inside* the root and is therefore
   allowed. Recorded, not claimed closed.
4. **`getRootDirectoryProperty()`** discloses the absolute root to any caller, who may then use
   plain `std::filesystem` outside the store entirely. That is inherent to a store built on
   public paths and is not a defect of the confinement.
5. **The `/rv` reference tree is absent**, so .NET's exact behaviour for directory-qualified
   search patterns (#2209) and for `..` (which .NET Core does **not** reject) is stated from the
   audit's own recorded reading, not re-derived. Where this port is deliberately stricter than
   .NET, §13 says so.

---

## 18. Completion criteria

The namespace is **closed for compatible work** when all of:

1. SR-AUD-241 flips `confirmed → remediated` with its original text preserved and a correction
   appended;
2. every one of the 13 caller path arguments has a permanent test proving rejection **and** no
   side effect;
3. every legitimate-path control still passes;
4. the dedicated executable exists, is registered, and the executable-count transition is
   recorded;
5. #2205 and #2206 are done;
6. #2207, #2208 and #2209 are recorded with exact reasons;
7. the full repository gate shows no new failure.

It is **not** called *fully* closed while #2207 and #2208 remain open — the audit's named probe
passing is not the same as the invariant holding against every adversary.
