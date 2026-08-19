<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Kill(entireProcessTree)` walks the tree instead of signalling a process group (ticket #2031)

*2026-08-19.* `Kill(true)` was `::killpg(pid, SIGKILL)`, which reaches the child's process
**group** — and a descendant that called `setsid()` has left it. Measured by #2028: the setsid
grandchild was **alive** afterwards and the probe had to kill it itself.

Landed under `docs/StandingApprovals.md` **SA-5**. No signature, layout or vtable change;
`sizeof(Process)` is one `unique_ptr`, unchanged.

---

## 1. The gate was "/rv absent", and the reference corrects the proposed design three times

#2031 was blocked pending approval of *"the /proc descendant walk"* described in its plan §14.3 —
*"ppid from `/proc/*/stat`, breadth-first from the child pid, SIGKILL each"* — on the ground that
the reference could not be read. It can, and .NET's `Process.KillTree` (`Process.Unix.cs:97-137`)
differs in three ways that matter:

1. **`SIGSTOP` comes first**, before the children are enumerated. .NET's comment says why,
   verbatim: *"Stop the process, so it won't start additional children. This is best effort: kill
   can return before the process is stopped."* Option A enumerated and **then** killed, so a
   process that forked between those two steps left a survivor — the very defect this ticket
   exists to remove, reintroduced one level down.
2. **There is a self-guard.** `Kill(bool)` refuses outright when the tree contains the calling
   process (`Process.NonUap.cs:25-26`, `InvalidOperationException`), because attempting it kills
   the caller. Option A had no such check.
3. **`ESRCH` is ignored and other failures are collected**, then reported together, because a
   process may legitimately exit between any two steps of the walk.

The recursion is depth-first in .NET rather than breadth-first; for killing, the two are
equivalent, and the SIGSTOP ordering is not.

## 2. What changed

| Call | Was | Is |
|---|---|---|
| `Kill(true)`, setsid descendant | survived | killed |
| `Kill(true)`, descendant three levels deep | survived | killed |
| `Kill(true)` on the current process | silent no-op | `InvalidOperationException` |
| `Kill(true)` with a partial failure | silent | `InvalidOperationException` naming each pid |
| `Kill(false)` | `::kill(pid, SIGKILL)` | **unchanged** |
| `Kill(false)` on the current process | silent no-op | **unchanged** (§3) |

The self-guard runs **before** the current-process no-op and before the exit check, because that
is where .NET has it. Placing it after would make `GetCurrentProcess().Kill(true)` a silent no-op
where .NET throws, **and would leave the guard unreachable through any ordinary `Process`
object** — a first cut did exactly that and the test caught it.

## 3. One divergence deliberately not bundled

`Kill(false)` on the current process is a silent no-op here and terminates the process in .NET.
That is pre-existing, is on the `Kill()` overload rather than the tree one, and .NET reaches it by
delegating `Kill(false)` straight to `Kill()` (`Process.NonUap.cs:17-20`) with no tree logic at
all. Bundling it would be exactly the mixing this repository's records complain about elsewhere.

## 4. Linux specificity

The walk reads `/proc/<n>/stat` for the parent pid, so it is Linux-bound — and so is .NET's,
whose `GetChildProcesses` goes through `Process.GetProcesses()`, itself a `/proc` reader on Unix.
On a POSIX host without `/proc`, `opendir("/proc")` fails, `ChildrenOf` returns empty, and
`Kill(true)` degrades to killing the direct child only. `System::Diagnostics::Process` is already
documented POSIX-only and is exercised on Linux alone.

The `/proc/<n>/stat` parse scans to the **last** `)` rather than splitting on whitespace, because
field 2 is the executable name unescaped and may contain spaces and `)`.

## 5. Evidence

Six mutations, five caught:

| Mutation | Result |
|---|---|
| back to `killpg` | caught |
| the walk does not recurse (immediate children only) | caught — **after a case was added** |
| `Kill(false)` also walks the tree | caught |
| the self-guard never fires | caught |
| the self-guard moved after the current-process no-op | caught |
| **no `SIGSTOP` before enumerating** | **NOT caught — see below** |

**The recursion mutation went uncaught at first**, and the reason is worth keeping: `setsid()`
changes the *session*, not the parent, so the original pin's "grandchild" is still an **immediate
child** of the shell and a one-level walk kills it. `Fix2031_TheWalkIsTransitiveNotOneLevel` goes
three levels deep so the mutation survives it.

**The `SIGSTOP` mutation is not caught and cannot be, deterministically.** What it removes is a
*race window* — the microseconds between `ChildrenOf` and the `SIGKILL`, during which the target
could fork a child that is neither enumerated nor killed. A test could only observe it by forking
in a tight loop and hoping to land inside that window, which is a **flaky test**; this repository
has repaired two of those this session (#2352, #2105) on the ground that an intermittently green
gate is not evidence. The `SIGSTOP` is there because .NET has it and states its purpose, not
because a test distinguishes it. The note is at the site.

## 6. Downstream, measured

`cna` and `mobile-eggbert` reference `System::Diagnostics::Process` in **zero** code sites.
Neither was modified.
