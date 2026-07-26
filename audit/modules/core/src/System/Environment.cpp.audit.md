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
