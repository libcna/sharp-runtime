<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `modules/core` `Environment` compatible slice — plan

Ticket #2239. Two frozen audit findings in
`modules/core/src/System/Environment.cpp`:

| Finding | Severity | Headline |
|---|---|---|
| SR-AUD-107 | medium | `GetCurrentDirectory` loses valid paths longer than its fixed 4 KiB buffer |
| SR-AUD-108 | medium | `CommandLine` simple-concatenates arguments and loses required quoting boundaries |

Audit numbering is frozen at `SR-AUD-001..364`; this review creates no new
`SR-AUD-*` identifier. This is a **slice of one file**, not a
`System::Environment` namespace review and not a `modules/core` review.

---

## 1. Exact scope, and what is deliberately left out

In scope: `Environment::GetCurrentDirectory()`, `Environment::getProcessPathProperty()`
(named by SR-AUD-107's own last sentence), `Environment::getCommandLineProperty()`,
and `modules/core/tests/System/EnvironmentTests.cpp`.

**Out of scope, by decision rather than by omission** — the other two findings in
the same audit report are *not* part of a compatible slice and must not be
absorbed:

- **SR-AUD-106** — `SetEnvironmentVariable` conflates an empty value with
  deletion. Fixing it needs a way to express "no value" that the current
  `const std::string&` parameter cannot carry, i.e. a **public signature
  change** (an `std::optional<std::string>` overload or equivalent). It also
  requires retiring `EnvironmentTests.Set_Empty_RemovesVar`, which pins the
  incompatible behaviour today. Approval-bound; not started here.
- **SR-AUD-105** — Unix special-folder resolution. Four separate contracts in one
  finding (absolute `XDG_CONFIG_HOME`/`XDG_DATA_HOME` honouring, the discarded
  `SpecialFolderOption`, filesystem verification and creation, and a range
  exception for an undefined `SpecialFolder`). That is an **XDG design**, needs a
  decision about creating directories from a getter, and would change resolved
  paths for existing users. Not started here.

Note one adjacency worth recording so a later reader does not mistake it for an
oversight: SR-AUD-105's "an undefined `SpecialFolder` falls through this switch to
an empty string rather than .NET's range exception" is the *same shape* as
SR-AUD-064, which this batch's Lazy family repaired. It is still excluded,
because it cannot be separated from the rest of SR-AUD-105's design.

Also untouched: the audit report's four "other missing assertions" bullets
(`environ` traversal under concurrent `setenv`, `uname` failure, the
`getTickCountProperty` wrap conversion, Windows/macOS branch coverage). None
carries an identifier.

## 2. Are these one family? — a slice, not a cause family

They share the file, the shape ("a public value is silently wrong and nothing
says so"), and the compatibility class (no signature, layout or `noexcept`
change). They do **not** share a root cause:

- **SR-AUD-107 is a buffer-sizing defect at an OS boundary.** The OS is asked
  for a value of unbounded length through a fixed stack array, and the failure
  is discarded. `System::IO::Directory::GetCurrentDirectory` in `modules/io` is
  the in-repository sibling that already does this correctly — it is
  `std::filesystem::current_path().string()`, with no ceiling — which is what
  makes the divergence measurable rather than arguable.
- **SR-AUD-108 is a missing text-encoding algorithm.** No buffer is involved and
  nothing fails; the emitted text is simply built by a join where .NET builds it
  with `PasteArguments.Paste`.

So: one bounded review (#2239), two independent implementation tickets (#2240,
#2241) and one deferred verification (#2242).

## 3. Before evidence, measured 2026-08-10

`build-probe/2239_probe1_before.cpp` against the shipped
`build/libsharp_runtime_core.a`: 24 cases, **6 OK / 18 BAD**
(`build-probe/2239_probe1_before.log`).

SR-AUD-107 reproduces the finding's own measurement almost exactly. The probe
builds a real current directory out of 200-byte components, `mkdir`/`chdir`
relative each time so no single pathname argument approaches `PATH_MAX`:

```
[107] built cwd of 4868 bytes at depth 24
[107] long cwd length     BAD  (got 0, want 4868)
[107] long cwd value      BAD  (got empty, want match)
```

4,868 bytes against the finding's 4,866 — the difference is component size, not
substance. The controls hold: a short cwd matches a dynamically sized `getcwd`
before and after, the probe restores the original cwd, and the process path is
absolute.

SR-AUD-108 fails 16 of its 18 rows, and half of those are round-trip rows: the
probe re-parses the emitted string with a `CommandLineToArgvW`-shaped reference
parser, so "cannot be parsed back to the original argv" is measured rather than
asserted. `prog` + `two words` yields `[prog|two|words]`; `prog` + `` (empty)
yields `[prog]`, i.e. an argument disappears entirely.

## 4. The two members, individually

### 4.1 SR-AUD-107 — remove the ceiling at both doors (#2240)

`GetCurrentDirectory` gets a growing buffer: call, and on `ERANGE` double and
retry; any other `errno` keeps the existing `""` result, so the **error contract
does not move**. A ceiling of 1 MiB remains as a runaway guard — a pathological
`errno` must not turn into unbounded allocation — and is documented as such;
4 KiB was the defect, an allocation bound two orders of magnitude above any real
path is not.

The Windows branch gets Win32's own documented two-call pattern:
`GetCurrentDirectoryA(0, nullptr)` returns the required size including the
terminator, then one sized call.

`getProcessPathProperty` is named by the finding's last sentence and carries a
**worse** version of the same defect, which the finding did not state and this
plan records as an addition rather than an inherited premise: `readlink` into a
fixed `char[4096]` **cannot report truncation** — it returns the number of bytes
written, so a longer path is silently returned *truncated* rather than empty.
The repair grows the buffer while `len == buf.size()`, which is the only
truncation signal `readlink` gives. The Windows branch likewise loops while
`GetLastError() == ERROR_INSUFFICIENT_BUFFER`, and — a second unstated defect —
stops ignoring a zero return, which previously produced whatever was in the
buffer.

> **Read §11.1 before quoting this paragraph.** The code claim above survived
> measurement; the *reachability* claim did not. Linux's procfs cannot represent
> an `exe` link target past `PATH_MAX` at all, so the truncation is unreachable
> through `/proc/self/exe` and this branch is defensive correctness, not a
> reproduced Linux defect.

Deliberately **not** switched to `std::filesystem::current_path()` despite the
`modules/io` sibling: on Windows `path::string()` converts through a different
narrow encoding than `GetCurrentDirectoryA`, so it would change bytes on a
platform this container cannot test. The minimal repair keeps every platform's
existing encoding and removes only the ceiling.

Emscripten is unaffected in kind: it takes the POSIX branch for the cwd and
already returns `""` for the process path.

### 4.2 SR-AUD-108 — implement `PasteArguments` (#2241)

.NET's `Environment.CommandLine` is
`PasteArguments.Paste(GetCommandLineArgs(), pasteFirstArgumentUsingArgV0Rules: true)`.
Two rule sets, both implemented:

**argv[0] rules** — backslash is an ordinary character, quotes exist only to
carry whitespace, and there is no escape: emit the argument verbatim, wrapped in
`"` if it is empty or contains whitespace.

**Every other argument** — the `CommandLineToArgvW` inverse:

- an argument that is non-empty and contains no whitespace and no `"` is emitted
  verbatim (so the overwhelmingly common case does not change at all);
- otherwise it is wrapped in `"`, a run of *n* backslashes immediately before a
  `"` becomes 2*n*+1 backslashes then `\"`, a run of *n* backslashes at the very
  end becomes 2*n* (because a `"` is about to follow), a bare `"` becomes `\"`,
  and every other character is copied.

`std::isspace` is **not** used: it is locale-dependent and takes an `int`, and
this is a byte-level operation over UTF-8 storage. Whitespace is the explicit
ASCII set `' '`, `'\t'`, `'\n'`, `'\v'`, `'\f'`, `'\r'`, which is what
`char.IsWhiteSpace` recognises among the Basic-Latin bytes that can appear
unescaped here.

**One element cannot be decided in this container and is deferred, not guessed
(#2242).** .NET's argv[0] rules cannot represent a literal `"` in argv[0]; what
`PasteArguments` does when asked to — emit it verbatim, or throw — is not
derivable from the finding, and `/rv` is absent. This port emits it verbatim,
because a public property that throws for a legal `argv[0]` would be a worse
failure than an unparseable one, and the choice is pinned by a test so it cannot
drift.

### 4.3 Behaviour change and migration (#2241 only)

`getCommandLineProperty()` emits different text than before for an argument that
is empty or contains whitespace or `"`. Everything else is byte-identical,
including the existing `CommandLine_JoinsArgs` expectation `"prog arg1"` and the
uninitialised `""`. The property is diagnostic — the source comment has
acknowledged the incompatibility since the type was ported, no in-repository
consumer reads it, and a `grep` for it outside `Environment.cpp`/`Environment.hpp`
and their tests returns nothing. No migration document is minted for a
diagnostic string that is moving *towards* its documented .NET contract; this
section is the record.

## 5. CCF relationships — none minted, none extended

Neither member joins a cause. SR-AUD-107 is not CCF-004 (defined arithmetic) and
not a memory-safety cause: the fixed buffer is never overrun, `getcwd` and
`readlink` are both given their true size, so the defect is data loss, not a
sanitizer-visible fault. SR-AUD-108 is not CCF-012 (composite-format grammar) —
it emits a shell/`argv` encoding, not a `{n}` format string. **CCF-021**
(`#2131`, protocol-field-terminator injection) is the nearest shape to
SR-AUD-108 and is deliberately **not** touched: it is an unminted candidate with
unresolved mint authority, and its members are protocol headers, not process
argument vectors. CCF-019, CCF-022 and CCF-011 are untouched.

## 6. Compatibility, ABI, layout and `noexcept`

| Property | #2240 | #2241 |
|---|---|---|
| Public signature change | none | none |
| `noexcept` change | none — neither door was `noexcept` | none |
| Layout / `sizeof` / vtable | none — both are free-standing static members | none |
| Error contract | unchanged: a non-`ERANGE` failure still yields `""` | none |
| Emitted-value change | only where the old code returned `""`/truncated text for a valid path | **yes**, §4.3 |
| New heap allocation | yes, only on the retry path (POSIX cwd) and unconditionally for the `readlink` buffer | no |

## 7. Ticket split

| Ticket | Status | Scope |
|---|---|---|
| #2239 | review | this plan, the before-probe, premise verification, the 105/106 exclusion |
| #2240 | implementation | SR-AUD-107 — dynamic `getcwd` and `readlink`/`GetModuleFileNameA` retrieval |
| #2241 | implementation | SR-AUD-108 — `PasteArguments` quoting for `getCommandLineProperty()` |
| #2242 | `todo` (deferred verification) | does .NET's `PasteArguments` argv[0] path reject a literal `"`, or emit it verbatim? |

## 8. Test matrix

Added to `modules/core/tests/System/EnvironmentTests.cpp`:

- #2240 — a POSIX-only test that builds a >4 KiB current directory out of legal
  200-byte components, asserts `GetCurrentDirectory()` returns exactly the
  dynamically retrieved path, and unwinds with `chdir("..")`/`rmdir` so no
  pathname argument is ever long; it `GTEST_SKIP`s if the filesystem refuses,
  and restores the original cwd on every exit path. Plus controls: the short-cwd
  value is unchanged, `SetCurrentDirectory`/`GetCurrentDirectory` still agree,
  the process path is absolute and matches `/proc/self/exe` on Linux.
- #2241 — a table of argument vectors covering the plain case, whitespace, tab,
  a bare quote, backslash runs before a quote, trailing backslashes, an empty
  argument and an argv[0] containing a space; each asserts the exact emitted
  text **and** re-parses it with a `CommandLineToArgvW`-shaped reference parser
  to prove the round trip. Plus the two existing expectations, unchanged.

## 9. Sanitizer matrix

Neither member is a memory-safety defect — the old code never overran its
buffer, it discarded data — so a sanitizer would not discriminate either, and
none is used for the verdict. The one new sanitizer-relevant surface is #2240's
growing heap buffers and #2241's string building; both are exercised by the new
tests, which run under the ordinary gate.

## 10. Family completion criteria

Both findings dispositioned in `audit/AUDIT_FINDINGS_INDEX.md` and in the
`Environment.cpp` per-file report; #2242 recorded as deferred; SR-AUD-105 and
SR-AUD-106 still `confirmed` and unclaimed; zero build warnings; no test
regression; the before-probe re-run with every BAD row turned OK.

---

## 11. Outcome, measured 2026-08-10

The slice probe moved from **6 OK / 18 BAD** to **24 OK / 0 BAD**
(`build-probe/2239_probe1_before.log`, `…_after.log`). `EnvironmentTests`
99 → 108 (+9). Build clean, zero warnings.

### 11.1 A premise correction the process-path probe produced

§4.1 said `getProcessPathProperty()` carries a worse version of SR-AUD-107's
defect because `readlink` cannot report truncation. The **code** claim is
correct. The **reachability** claim is not, on Linux, and the correction matters
more than the repair:

`build-probe/2240_probe2_driver.cpp` builds a 4,671-byte directory, copies a
helper binary into it and `execl`s it there, so the running process's own
executable path exceeds 4 KiB. The helper's own independently sized
`readlink("/proc/self/exe", buf, 1 MiB)` then fails with **`ENAMETOOLONG`**:

```
[107] helper directory is 4671 bytes at depth 23
[107] readlink(/proc/self/exe) failed: File name too long
[107] exe path: reference=0 port=0  OK   (match)
```

Linux's procfs builds the `exe` link target in a page-sized buffer, so a path
past `PATH_MAX` cannot be represented through that interface **at all**. The
port's fixed 4 KiB buffer was therefore never the binding constraint on Linux:
before and after the repair the answer is the same empty string, and it is the
right one. The `getProcessPathProperty()` change is kept as defensive
correctness — it detects truncation instead of silently accepting it, and stops
the Windows branch from ignoring a zero `GetModuleFileNameA` return — but this
plan does **not** claim a reproduced Linux defect there. Only the
`GetCurrentDirectory` half is a measured data-loss reproduction.

A second, smaller correction, for anyone repeating this: a POSIX shell cannot
drive that experiment. `dash`'s `cd` builtin composes an absolute pathname and
hits `PATH_MAX` at ~4,096 bytes, while `chdir()` on a *relative* name from C has
no such limit — which is why both the probe and the driver are C++ and why the
regression test unwinds with `chdir("..")` rather than a long path.

### 11.2 Mutation checks

Each was applied to production code, rebuilt and re-run:

| Mutation | Result |
|---|---|
| POSIX `getcwd` returns `""` on `ERANGE` instead of growing | 1 test fails (`GetCurrentDirectory_LongPath_IsNotTruncatedOrLost`) |
| `readlink` loop returns the buffer without testing for truncation | **no test fails — labelled EQUIVALENT on Linux**, for the `ENAMETOOLONG` reason in §11.1 |
| `2n + 1` backslash rule weakened to `2n` | 2 tests fail (the backslash-rule test and the round-trip table) |
| argv[0] never quote-wrapped | 1 test fails (`CommandLine_ArgV0_UsesTheArgV0Rules`) |

### 11.3 What did not change

`CommandLine_JoinsArgs` (`"prog arg1"`) and `CommandLine_EmptyWhenNotInitialized`
(`""`) are untouched and still pass, which is the compatibility statement of
§4.3 in test form: an argument that is non-empty and free of whitespace and
quotes is emitted byte-identically to the old join. The `GetCurrentDirectory`
error contract is unchanged — a non-`ERANGE` failure still yields `""` — and a
short cwd is asserted equal to a dynamically sized `getcwd`.
