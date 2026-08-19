<!-- SPDX-License-Identifier: MIT -->
# Migration — POSIX `GetFolderPath` completes the .NET table (#2364)

Ticket **#2364**, landed 2026-08-19 on an explicit decision. **Decided against the recommendation
on the record.**

## The premise #2320 recorded was false, and that is why this ticket exists

When #2320 offered full XDG and it was declined, the reason given was that *"part of it has no .NET
mapping, so it would cross from parity into invention"*. Measured afterwards, that is **wrong**:
every remaining row maps onto `ReadXdgDirectory` (`GetFolderPathCore.Unix.cs:220-249`) or onto a
static path. #2320 stopped where it did for **scope**, not for invention — so the eight rows below
are **alignments**, not widenings.

## What changed — eight rows, and some change a value callers get today

| Folder | before | after | reference |
|---|---|---|---|
| `Personal` / `MyDocuments` | `$HOME` | `ReadXdgDirectory(XDG_DOCUMENTS_DIR, "Documents")` | `:133-134` |
| `Desktop` / `DesktopDirectory` | `$HOME/Desktop` | `ReadXdgDirectory(XDG_DESKTOP_DIR, "Desktop")` | `:119-121` |
| `MyMusic` | `$HOME/Music` | `ReadXdgDirectory(XDG_MUSIC_DIR, "Music")` | `:135-136` |
| `MyPictures` | `$HOME/Pictures` | `ReadXdgDirectory(XDG_PICTURES_DIR, "Pictures")` | `:139-140` |
| `MyVideos` | `$HOME/Videos` | `ReadXdgDirectory(XDG_VIDEOS_DIR, "Videos")` | `:137-138` |
| `Templates` | `$HOME/Templates` | `ReadXdgDirectory(XDG_TEMPLATES_DIR, "Templates")` | `:88-89` |
| `CommonApplicationData` | `/etc` | **`/usr/share`** | `:54` |
| `CommonTemplates` | *unmapped* | **`/usr/share/templates`** | `:55` |
| `Fonts` | `/usr/share/fonts` | **`$HOME/.fonts`** | `:141-142` |
| `ProgramFiles` | `/usr` | **`""`** | not mapped on Linux |
| `System` | `/usr/lib` | **`""`** | not mapped on Linux |

**The sharpest row is `Personal`.** This port returned the **home directory itself**, so an
application writing "the user's documents" wrote into `$HOME`. It now returns a `Documents`
subdirectory. `UserProfile` still returns home — the two used to be the same answer, and a repair
that moved both would look correct without the row that separates them.

**`CommonApplicationData` is the second.** `/etc` is not writable by a non-root user and is not
where shared application data belongs.

**`ProgramFiles` and `System` are the reverse shape**, and worth stating separately: .NET maps them
only under `TARGET_OSX` (`:57-60`), so on Linux no arm matches, `GetSpecialFolder` returns `null`,
and `GetFolderPathCore` turns that into `""`. This port had **invented** a mapping where .NET has
none, so here the alignment **removes** one.

## `ReadXdgDirectory`, transcribed

Three sources in .NET's order:

1. the environment variable, **if set and absolute** — a relative value is **ignored, not joined**,
   the same rule `xdgBase` already applied to the two base directories;
2. the `XDG_DESKTOP_DIR="$HOME/Desktop"`-style lines of `user-dirs.dirs`, read out of the **XDG
   config directory**, so it honours `XDG_CONFIG_HOME` rather than a hard-coded `~/.config`;
3. `home/fallback`.

The line grammar is transcribed rather than approximated, because each rejection is one .NET makes:
whitespace is skipped before the key, around `=` and before the opening quote; a value must be
either `$HOME/`-prefixed or absolute and **anything else is skipped rather than accepted**; the
value ends at the next `"`, and an empty one is skipped. .NET eats every error reading the file, so
an unreadable or malformed file falls through to the fallback.

## Tests — and the rule they follow

The eight rows **were never covered**, which is exactly why the divergence survived: the existing
cases assert enum values and non-emptiness under `DoNotVerify`, and the full suite passed unchanged
both before and after.

Every new case uses `DoNotVerify` and sets `HOME` and the XDG variables **explicitly**, so what is
asserted is the *mapping* rather than what this machine happens to have. That is the #2320 / SA-6
lesson as a rule: five of its rows passed only because this container has `~/Desktop`, which SA-6
calls a defect in the **test** rather than evidence about the code.

`Decl2364_AnUnknownHomeStillFallsBackToRoot` pins the unchanged `"/"` fallback, because every new
row builds on `home` and .NET states that fallback as a **safeguard** — `"/"` is not writable by a
non-root user, so a row that silently produced a relative `Documents` would be somewhere an
application could write private data.

## Mutation testing

Six mutations, **all caught**:

| # | Mutation | Caught by |
|---|---|---|
| M1 | `Personal` reverts to home | the six-directory case |
| M2 | a relative XDG variable is accepted | the absolute/relative case |
| M3 | `user-dirs.dirs` read from a hard-coded `~/.config` | the file-grammar case |
| M4 | a non-absolute `user-dirs.dirs` value is accepted | the file-grammar case |
| M5 | the `ProgramFiles` mapping is restored | the static/unmapped case |
| M6 | `CommonApplicationData` reverts to `/etc` | the static/unmapped case |
