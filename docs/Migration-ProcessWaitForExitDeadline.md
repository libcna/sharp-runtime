<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `WaitForExit(milliseconds)` honours its deadline (ticket #2032)

*2026-08-19.* Reaping the child **joined the pipe-reader threads**, and a reader cannot return
until the pipe reaches EOF — which needs every holder of the write end to close it, including a
grandchild that inherited it. Measured by #2032: `WaitForExit(5000)` returned `true` after
**29,951 ms**, roughly 6× its own declared bound and unbounded in general.

Landed under `docs/StandingApprovals.md` **SA-5**. No signature, layout or vtable change.

---

## 1. The blocker was #2029, which has landed — and the choice it gated was a false trichotomy

#2032's recorded blocker: *"every repair has to decide what happens to a reader thread that cannot
finish — join it (today, unbounded), detach it, or abandon the fd — and DETACHING IS EXACTLY
#2029's gated destructor policy."*

#2029 landed on 2026-08-17 and chose **none of the three**: it kept the join and bounded it with a
stop flag and a 50 ms poll slice, because a detached reader appends into the destroyed object's
own string.

And the reference shows the trichotomy was the wrong question anyway. **Reaping the child and
waiting for its output are two different things**, and .NET does the second in exactly one place:

```csharp
if (exited && milliseconds == Timeout.Infinite) // if we have a hard timeout, we cannot wait for the streams
{
    _output?.EOF.GetAwaiter().GetResult();
    _error?.EOF.GetAwaiter().GetResult();
}
                                  // Process.Unix.cs, WaitForExitCore
```

The comment is the answer, verbatim. So the readers are neither detached nor abandoned — they are
simply **not waited for** by doors that have a deadline, or no stream contract at all.

## 2. What changed

`reapIfNeeded` no longer joins the readers. The join now lives only in the unbounded
`WaitForExit()`, which is `WaitForExit(Timeout.Infinite)` in .NET too.

Measured against a grandchild holding the redirected pipe:

| Door | Was | Is |
|---|---|---|
| `WaitForExit(200)` | blocked past its bound | returns within it |
| `getHasExitedProperty()` | blocked | prompt |
| `Kill()` | blocked | prompt |
| `getExitCodeProperty()` | blocked (via `HasExited`) | prompt |
| `WaitForExit()` (no timeout) | waited for the output | **unchanged** |
| the restart `Start()` | blocked | **unchanged** — §3 |
| `~Process` | bounded by #2029 | **unchanged** |

#2033 measured that the join was reached from five public doors, six counting
`getExitCodeProperty()`. Four of them are repaired by removing one statement, because they all
reached it through `reapIfNeeded`.

**The consequence a caller should know:** after a `WaitForExit(ms)` that returns `true`, the
captured output may be incomplete. That is .NET's contract too, and it is why .NET's own guidance
is to call the parameterless overload when you need the whole of it.

## 3. The one door still pinned as blocking, and why it is not a policy choice

The restart `Start()` keeps its join. #2025's restart preamble resolves the previous child's
readers before assigning over the `std::thread`, and in C++ **assigning to a joinable
`std::thread` calls `std::terminate`** — so this join is required by the language, not by a policy
decision. .NET has no counterpart, because its `Start` does not reuse the reader machinery this
port must. `Pin2033_RestartBlocksBehindAGrandchild` stays, with its comment rewritten to say so.

## 4. Evidence

Four mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| the join restored in `reapIfNeeded` | the three inverted pins |
| the unbounded overload stops joining | **seven** other tests (§5) |
| the bounded overload joins after its deadline expires | a case added for it (§5) |
| `~Impl` stops setting the stop flag | `Fix2029_RedirectedDestructionIsPrompt` |

Two of these needed work before they were caught, and both are recorded rather than smoothed over.

**The third went uncaught at first**, because `WaitForExit(ms)` returns from an early
`if (hasExited) return true` when the child has already exited — so the fall-through after the
deadline is a *different statement* that no case reached.
`Fix2032_AnExpiredTimeoutReturnsWithoutJoiningEither` was added for it: the child is still running,
so the call must time out and return `false` without waiting.

**The second is caught, but not by the case written for it.**
`Fix2032_TheUnboundedOverloadStillWaitsForTheOutput` states the contract and does **not**
discriminate the mutation — measured, the reader drains 16 KiB well before `waitpid()` reports the
exit, so the output is complete either way. What catches it is seven pre-existing tests, five of
them `ZZZ_NoZombieChildrenRemain` checks plus
`ProcessForkSafetyTests.StartsWithEnvironmentWhileReaderThreadsAreLive`. The case is kept because
it is the only place the contract is written down; the note is at the site.

## 5. Downstream, measured

`cna` and `mobile-eggbert` reference `System::Diagnostics::Process` in **zero** code sites.
Neither was modified.
