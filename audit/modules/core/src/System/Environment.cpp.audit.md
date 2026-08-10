# Audit: `modules/core/src/System/Environment.cpp`

## Metadata

- Audit status: AUDITED (486-line platform implementation, fully read with
  `Environment.hpp` and all 99 direct tests).
- Validation: `EnvironmentTests.*` passed 99/99 on 2026-07-26.  The isolated
  `/tmp/sharp-runtimervc-environment-audit-probe` prints
  `xdg_config=0`, `xdg_data=0`, `empty_value_key=0`,
  `invalid_option_throws=0`, `long_actual_length=4866`,
  `long_sharp_length=0`, and an ambiguous unquoted command line.
- Reference basis: local .NET Unix Environment and PasteArguments sources.

## SR-AUD-105 — medium — Unix special-folder resolution ignores XDG, verification, creation, and invalid-enum contracts

The POSIX `GetFolderPath` switch hard-codes `$HOME/.config` and
`$HOME/.local/share` (`Environment.cpp:255-278`), while current .NET honors
absolute `XDG_CONFIG_HOME` and `XDG_DATA_HOME`.  The probe sets both to absolute
`/tmp/audit-*` paths and prints `xdg_config=0` and `xdg_data=0`.  It also shows
`invalid_option_throws=0`: the overload discards every
`SpecialFolderOption`, including undefined values, whereas .NET rejects an
invalid option, returns an empty path for a nonexistent folder with `None`,
and creates it for `Create`.  An undefined `SpecialFolder` falls through this
switch to an empty string rather than .NET's range exception.

This redirects configuration/data paths away from an explicitly configured
user location and makes option/error calls silently succeed.  Existing tests
only require a nonempty self-derived path and explicitly assert the port's
ignored-option behavior.

## SR-AUD-106 — medium — SetEnvironmentVariable conflates a valid empty value with deletion

The C++ overload accepts only `std::string`, then calls `unsetenv` whenever
`value.empty()` (`Environment.cpp:207-218`).  Current .NET has a nullable
`string?` value: `null` deletes an entry, while `string.Empty` remains a valid
present environment variable with an empty value.  The direct probe sets an
empty value and its environment map prints `empty_value_key=0`, proving the key
was removed.  The C++ signature has no nullable/optional deletion state, so
callers cannot express both operations.

`EnvironmentTests.Set_Empty_RemovesVar` locks the incompatible behavior
instead of checking map membership for an empty value and a separately
representable delete operation.

## SR-AUD-107 — medium — GetCurrentDirectory loses valid paths longer than its fixed 4 KiB buffer

`GetCurrentDirectory` calls `getcwd` once with `char buf[4096]` and returns an
empty string on `ERANGE` (`Environment.cpp:100-109`).  The probe enters a real
4,866-byte `/tmp` directory path one legal component at a time; libc's dynamic
`getcwd` reports `long_actual_length=4866`, but the public port returns
`long_sharp_length=0`.  Current .NET's Unix `Interop.Sys.GetCwd` does not impose
this public 4 KiB ceiling.  The same fixed-buffer review is needed for the
separate process-path branch, which tests only on a short executable path.

## SR-AUD-108 — medium — CommandLine simple-concatenates arguments and loses required quoting boundaries

`getCommandLineProperty` joins raw entries with spaces (`Environment.cpp:469-483`).
For arguments `program`, `two words`, and `quote"value`, the probe prints
`command_line=program two words quote"value`; it cannot be parsed back to the
original argv.  Current .NET calls `PasteArguments.Paste`, which quotes
whitespace and escapes quotes/backslashes so the argument sequence round trips.
The source comment acknowledges this incompatibility, but the public member is
still presented as the .NET counterpart and its direct test covers only two
unquoted simple values.

## Other missing assertions and diagnostics

- No test covers concurrent traversal of the raw POSIX `environ` block while
  `setenv`/`unsetenv` may reallocate it, failure returns from `setenv`, or
  embedded-NUL values truncated by the OS C APIs.
- `getOSVersionProperty` ignores `uname` failure; ProcessPath, GetCurrentDirectory,
  and machine/user names convert OS failures to empty strings without a
  distinct diagnostic.
- `getTickCountProperty` converts a 64-bit time to signed native `intcs` at
  wrap; the required C# unchecked-wrap versus C++ out-of-range-conversion
  behavior is untested on an uptime past `INT32_MAX` milliseconds.
- No test covers XDG user-directory configuration, filesystem verification or
  creation, unsupported folder values, Windows/macOS branches, or stack-trace
  behavior.

## Final assessment

Core short-path/process-environment happy paths pass, but configured folders,
empty-value representation, long cwd handling, and diagnostic command lines
are observably incompatible.  No source or test was modified during this
audit.

---

## SR-AUD-107 — REMEDIATED (ticket #2240, slice #2239, 2026-08-10)

The original evidence above is retained unchanged. Its headline claim is
confirmed by an independent reproduction and its **second** claim needs a
correction.

Measured before the fix (`build-probe/2239_probe1_before.cpp`, log
`…_before.log`, 24 cases, 6 OK / 18 BAD). The probe builds a real current
directory out of 200-byte components, `mkdir`/`chdir` relative each time so no
single pathname argument approaches `PATH_MAX`:

```
[107] built cwd of 4868 bytes at depth 24
[107] long cwd length     BAD  (got 0, want 4868)
[107] long cwd value      BAD  (got empty, want match)
```

4,868 bytes against the report's 4,866 — a component-size difference, not a
substantive one.

**The repair.** The POSIX branch doubles its buffer on `ERANGE` and returns
`""` unchanged for every other `errno`, so the **error contract does not move**;
a 1 MiB runaway ceiling replaces the 4 KiB path ceiling and is documented as a
guard against unbounded allocation rather than as a path limit. The Windows
branch adopts Win32's own documented two-call pattern,
`GetCurrentDirectoryA(0, nullptr)` for the required size and then one sized
call. `std::filesystem::current_path()` was deliberately **not** adopted despite
`System::IO::Directory::GetCurrentDirectory` already using it, because on
Windows `path::string()` converts through a different narrow encoding than
`GetCurrentDirectoryA` and this container cannot test that platform.

**Premise correction — the process-path branch.** The report's last sentence
asks for the same review of `getProcessPathProperty()`, and the review's answer
is more interesting than a second repair. The **code** defect is real and worse
in kind: `readlink` returns the number of bytes it *wrote*, so a longer path
came back silently **truncated** rather than empty, and the Windows branch
discarded `GetModuleFileNameA`'s return entirely. But on Linux the truncation is
**unreachable**. `build-probe/2240_probe2_driver.cpp` builds a 4,671-byte
directory, copies a helper binary into it and `execl`s it there so the running
process's own executable path exceeds 4 KiB; the helper's independently sized
`readlink("/proc/self/exe", buf, 1 MiB)` then fails with **`ENAMETOOLONG`**
(`build-probe/2240_probe2.log`), because procfs builds the `exe` link target in a
page-sized buffer. Before and after the change that case returns the same empty
string, and it is the correct one. The loop is kept as defensive correctness on
platforms that do not cap the answer, together with the Windows zero-return
handling — which was a real unconditional defect — but **no reproduced Linux
defect is claimed for this branch**. The source comment says so too.

A methodological note for anyone repeating the experiment: a POSIX shell cannot
drive it. `dash`'s `cd` builtin composes an absolute pathname and hits
`PATH_MAX` at ~4,096 bytes, while `chdir()` on a *relative* name from C has no
such limit — which is why the probe, the driver and the regression test's unwind
are all C++ and all relative.

After the fix the probe reads 24 OK / 0 BAD. +3 permanent regressions in
`EnvironmentTests.cpp`: a POSIX-only >4 KiB current-directory test that asserts
the exact value, unwinds with `chdir("..")`/`rmdir` so no long pathname is ever
passed to the OS, `GTEST_SKIP`s if the filesystem refuses and restores the
original cwd on every exit path; a short-cwd control asserting equality with a
dynamically sized `getcwd`; and a process-path control asserting absoluteness and
agreement with an independently sized `readlink`. `EnvironmentTests` 108/108.

Mutation checks, each rebuilt and re-run: making the POSIX `getcwd` return `""`
on `ERANGE` instead of growing fails the long-path test; making the `readlink`
loop ignore truncation fails nothing and is labelled **equivalent on Linux**, for
the `ENAMETOOLONG` reason above.

No signature, `noexcept`, layout or vtable change — both doors are free-standing
static members. Plan: `docs/CoreEnvironmentCompatibleSlicePlan.md` §4.1, §11.1.

## SR-AUD-108 — REMEDIATED (ticket #2241, slice #2239, 2026-08-10)

The original evidence above is retained unchanged and is confirmed exactly as
written, with more of it than the report measured.

Before the fix, 16 of the finding's 18 probe rows were wrong, and half of those
are **round-trip** rows: the probe re-parses the emitted string with a
`CommandLineToArgvW`-shaped reference parser, so "it cannot be parsed back to
the original argv" is a measurement rather than an assertion. `prog` +
`two words` came back as `[prog|two|words]`; `prog` + `""` came back as
`[prog]`, i.e. an argument disappeared entirely.

`Environment.CommandLine` in .NET is
`PasteArguments.Paste(GetCommandLineArgs(), pasteFirstArgumentUsingArgV0Rules: true)`,
and both rule sets are now implemented:

- **argv[0]** — backslash is an ordinary character and quotes exist only to carry
  whitespace, so the argument is emitted verbatim, wrapped in `"` only when it is
  empty or contains whitespace;
- **every other argument** — emitted verbatim when non-empty and free of
  whitespace and quotes (so the common case is byte-identical to the old join);
  otherwise wrapped in `"`, with a run of *n* backslashes before a `"` becoming
  2*n*+1 backslashes then `\"`, a run of *n* at the very end becoming 2*n*, and a
  bare `"` becoming `\"`.

Whitespace is the explicit ASCII set `' '`, `'\t'`, `'\n'`, `'\v'`, `'\f'`,
`'\r'` — deliberately not `std::isspace`, which is locale-dependent and takes an
`int`, over what is a byte-level operation on UTF-8 storage.

**This is a behaviour change**, recorded rather than migrated: the emitted text
moves for an empty argument and for one containing whitespace or a quote.
Everything else is byte-identical, including both pre-existing expectations
(`"prog arg1"` and the uninitialised `""`), a `grep` finds no in-repository
consumer outside the header, the source comment has acknowledged the
incompatibility since the type was ported, and the change moves the property
*towards* its documented .NET contract.

**One element is deferred rather than guessed.** argv[0]'s rules cannot represent
a literal `"` at all; whether `PasteArguments` emits it verbatim or rejects the
argument is not derivable from the finding, and `/rv` is absent here. This port
emits it verbatim — a public diagnostic property that throws for a legal argv[0]
would be a worse failure than an unparseable one — and the choice is **pinned by
a test** so it cannot drift. Residual: ticket **#2242**, following the
#2060/#2070/#2130/#2234 convention.

+6 permanent regressions, including a table in which every non-argv[0] case is
re-parsed to exactly the original argument vector. Mutation-checked two ways:
weakening the 2*n*+1 rule to 2*n* fails the backslash test and the round-trip
table; never quote-wrapping argv[0] fails the argv[0] test.

No signature, `noexcept`, layout or vtable change. Plan:
`docs/CoreEnvironmentCompatibleSlicePlan.md` §4.2–4.3, §11.

SR-AUD-105 and SR-AUD-106 in this same report remain **confirmed and unclaimed**:
106 needs a public signature change to express "no value", and 105 is an XDG
design entangled with option semantics, filesystem verification and directory
creation from a getter. The report's four "other missing assertions" bullets are
likewise untouched.
