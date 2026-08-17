<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Process` destruction and captured-output safety (tickets #2029, #2030)

*2026-08-17.* Destroying a `System::Diagnostics::Process` whose child is still running no longer
blocks for the child's lifetime, and the two captured-output getters now return `std::string` by
value under a lock instead of a live reference into a buffer an internal thread is appending to.

Landed under `docs/StandingApprovals.md` SA-5 for the behaviour and SA-2 for the one declaration
change. `Process` is a PIMPL, so **no public object layout moved**.

---

## 1. #2029 — destruction was two opposite defects

The audit's premise correction still holds: which one fired depended on redirection.

| Case | Was | Is |
|---|---|---|
| **redirected**, child still running | destruction **blocked for the child's whole lifetime** — measured 2005 ms for a 2 s child, unbounded in general | returns promptly |
| **unredirected**, child still running | the child was left as a permanent zombie | unchanged — see §1.2 |

### 1.1 Why it blocked, and what changed

`~Impl` joins the pipe readers, and a reader calling a bare blocking `read()` cannot return until
the child closes stdout. Joining therefore meant waiting for the child.

The readers now wait on `poll()` in bounded slices and observe a stop flag the destructor sets, so
each returns within one slice. **The join itself is kept, deliberately.** The plan's option C said
to *detach* the readers instead; a detached reader keeps appending into the `Process`'s own string
after the object is gone, which is a use-after-free — that would trade this defect for a worse
one. Bounding the wait gets the same promptness without it.

This matches .NET's contract, which is now read from the source rather than inferred:
`Process.Close()` (`Process.cs:761-805`) stops watching for exit, releases the handle and
**cancels** the async read before disposing the stream. It never waits for the child.

### 1.2 What is still divergent, and why

A child that is **still running** when its `Process` is destroyed is left to the OS, so it becomes
a zombie until the parent exits. .NET does not have that problem because it reaps process-wide
from a SIGCHLD-driven wait state (`ProcessWaitState.Unix.cs`); this port cannot replicate that
without colliding with `PosixSignalRegistration` (#1975/#1979), which owns SIGCHLD handling here.

A child that has **already exited** is reaped, as before.

**To migrate:** call `WaitForExit()`, or `Kill()`, before dropping a `Process` whose child you do
not intend to outlive. That was already the right shape; it is now the only one that reaps.

## 2. #2030 — the captured-output getters

```cpp
const std::string& text = process.getStandardOutputTextProperty();   // still compiles
auto&              bad  = process.getStandardOutputTextProperty();   // now binds a copy
```

Both getters returned a reference into storage the internal reader thread was still appending to.
The audit measured the **same reference** reading 4 bytes mid-run and 8 bytes after exit. No
reference can be made safe there, so the getters return by value and take the copy under the same
lock the readers append under.

This is the only public declaration change in the namespace, and it is source-compatible for the
ordinary spelling: a returned value still binds to `const std::string&` and lifetime-extends.
`auto&` deduces differently and will no longer alias the process's own buffer — which is the
point.

### 2.1 `getHasExitedProperty()` is still `const` and still mutates

It reaps lazily through `reapIfNeeded`, so reading it transitions the object to the exited state.
**That is .NET's shape too** — `HasExited` reaps lazily — so the repair is not to stop mutating
but to make the mutation safe: it now happens under a lock. `const` on this class means "does not
change the observable process state", and it no longer silently implies "safe from two threads",
because now it is.

## 3. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched: neither `cna` nor `mobile-eggbert`
names `System::Diagnostics::Process` or either getter — **zero sites in both**. Neither repository
was modified.
