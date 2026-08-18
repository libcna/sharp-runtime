<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — XDG base directories, and `SpecialFolderOption` on POSIX (ticket #2320)

*2026-08-18.* `Environment::GetFolderPath` ignored `XDG_CONFIG_HOME` and `XDG_DATA_HOME`, and
accepted `SpecialFolderOption` on POSIX without honouring it. Both are fixed.

Landed under `docs/StandingApprovals.md` SA-5, on the user's decision of the same date (SA-11).
**The default option's behaviour changes** — read §2. No signature, layout, vtable or `noexcept`
change.

---

## 1. XDG — decision 1, answer (b)

| `XDG_CONFIG_HOME` | `ApplicationData` was | is |
|---|---|---|
| unset | `$HOME/.config` | `$HOME/.config` |
| `/srv/cfg` (absolute) | `$HOME/.config` | **`/srv/cfg`** |
| `relative/cfg` | `$HOME/.config` | `$HOME/.config` |
| `""` (empty) | `$HOME/.config` | `$HOME/.config` |

`XDG_DATA_HOME` behaves identically against `LocalApplicationData` / `$HOME/.local/share`.

The test is `Environment.GetFolderPathCore.Unix.cs:157-161` transcribed —
`config is null || !config.StartsWith('/')` — so it is **"is it absolute"**, not "is it set". An
empty value falls back exactly as an unset one does, because an empty string does not start with
`/` either. The XDG specification agrees: a relative value *"must be ignored"*. A naive
`getenv() != nullptr` implementation fails that row, and a test pins it.

## 2. `SpecialFolderOption` is now honoured on POSIX — decision 2

This half was **not** a user question in the end: .NET answers it itself
(`Environment.GetFolderPathCore.Unix.cs:26-47`), so it is a derivation under SA-5.

| Option | Behaviour |
|---|---|
| `DoNotVerify` | return the path unchecked |
| `None` — **the default** | `access(path, R_OK)`; **return `""` if that fails** |
| `Create` | verify; on failure create every missing component, then return the path |
| undefined value | `ArgumentOutOfRangeException`, paramName `option` (`Environment.cs:149-163`) |

**Before this ticket all three returned the path**, so a caller could not tell a real directory
from a name. The behaviour change that matters:

```cpp
// A machine with no ~/Desktop:
Environment::GetFolderPath(SpecialFolder::Desktop);   // was "/home/u/Desktop", is now ""
```

If you want the old unconditional answer, ask for it: pass
`SpecialFolderOption::DoNotVerify`. If you want the directory to exist, pass
`SpecialFolderOption::Create`.

`Directory::CreateDirectory` is deliberately **not** used — `Core.Base` does not depend on
`modules/io`, and taking that edge to create one directory would be a far larger change than the
behaviour it buys. `::mkdir` per component is the whole implementation.

Note the option was already meaningful on Windows, where its values *are* the CSIDL flags and are
OR-ed into the folder id. This change makes POSIX agree with a contract Windows always had.

## 3. What this ticket deliberately did NOT change — and a premise it corrected

Option (c), *full XDG*, was declined **on a premise that turned out to be wrong**, and this is
recorded rather than quietly acted on. The decision was offered with the reasoning *"part of it
has no .NET mapping, so it would cross from parity into invention"*. Measured against the
reference, **every remaining XDG behaviour does have a .NET mapping**: `ReadXdgDirectory`
(`Environment.GetFolderPathCore.Unix.cs:165+`) reads `user-dirs.dirs` out of the XDG config
directory and backs `Desktop`, `MyDocuments`, `MyMusic`, `MyPictures`, `MyVideos` and `Templates`.

So the reason to stop here is *scope*, not *invention*. Eight further rows diverge from the
reference and are filed as **ticket #2364** for a decision made on accurate information:

| `SpecialFolder` | this port | .NET on Linux |
|---|---|---|
| `Personal` / `MyDocuments` | `$HOME` | `ReadXdgDirectory(XDG_DOCUMENTS_DIR, "Documents")` |
| `Desktop` / `DesktopDirectory` | `$HOME/Desktop` | `ReadXdgDirectory(XDG_DESKTOP_DIR, …)` |
| `MyMusic` / `MyPictures` / `MyVideos` | `$HOME/Music` … | `ReadXdgDirectory(…)` |
| `Templates` | `$HOME/Templates` | `ReadXdgDirectory(XDG_TEMPLATES_DIR, …)` |
| `CommonApplicationData` | `/etc` | **`/usr/share`** |
| `Fonts` | `/usr/share/fonts` | **`$HOME/.fonts`** |
| `ProgramFiles`, `System` | `/usr`, `/usr/lib` | **not mapped** (empty) |
| `CommonTemplates` | not mapped | `/usr/share/templates` |

One row **was** adopted here because it is a safety property rather than a path preference: when
`HOME` is unset, the reference falls back to `/` and says why — `/` is not writable by a non-root
user, so an application cannot silently write private data into a path built from an empty
string.

## 4. A test defect this ticket had to fix on the way

Five existing rows asserted `GetFolderPath(...)` was non-empty, and passed only because the
development machine happens to have `~/Desktop` and `~/.config`. Once the default option
verifies, they encode the environment rather than the contract — exactly what
`docs/StandingApprovals.md` SA-6 calls a defect in the test. They now pass `DoNotVerify`, which is
what they were always about.

One row asserted the **opposite** of the new contract (`DoNotVerify` equals the default) and is
rewritten to state the real one: the two agree exactly when the directory is readable, and differ
otherwise — which is the entire point of the option.
