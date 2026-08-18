<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `EnableRaisingEvents = false` stops the rest of the batch too (ticket #2105)

*2026-08-18.* A `FileSystemWatcher` handler that stops its own watcher used to keep receiving the
rest of the inotify batch. **Measured: 13 further invocations out of a 24-file batch, after the
setter had returned.**

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. The question, and the measurement the review could not make

#2097 recorded that whether a handler can still be invoked after `EnableRaisingEvents = false`
returns *"was NOT measured … because doing it properly needs TSan plus a deterministic harness"*.

It needs no TSan, and the harness is deterministic — **the batch is what makes it so**. inotify
delivers many events in one `read()`, and this port's watch loop dispatched the whole batch before
it next reached `poll()`. So creating 24 files quickly and stopping the watcher from the *first*
handler answers the question with no timing race at all: either the remaining handlers run or they
do not. They ran. Thirteen of them.

## 2. Two paths, and only one was wrong

| Path | Mechanism | Before | After |
|---|---|---|---|
| **external** — another thread sets it false | **joins** the watch thread | correct: the setter cannot return while a handler is running | unchanged |
| **self-stop** — a handler sets it false | cannot join itself (#2347), so it flags and returns | **13 more handlers ran** | none run |

The external path was already correct, and a test now pins it: the setter blocks while a sleeping
handler finishes. That is a *stronger* guarantee than .NET's, which does not join.

## 3. What .NET does

```csharp
public void Stop()
{
    lock (this)
    {
        if (IsWatcherStopped) return;
        IsWatcherStopped = true;
        _emitEvents = false;
        …
    }
    …
}

internal void QueueEvent(WatcherEvent ev)
{
    if (!_emitEvents) return;      // dropped, per event
    …
}
```
*(`FileSystemWatcher.Linux.cs:1071-1093`, `:1211-1217`.)*

The gate is **per event**, at the point of queueing, and the event is **dropped** rather than
deferred. This port now checks `enabled_` in the same place — before each dispatch — and drops.

## 4. The gate has two sites, and the second is easy to miss

An `IN_MOVED_FROM` whose paired `IN_MOVED_TO` never arrives is reported as a `Deleted` from a loop
that runs **after** the batch. A gate on the per-event loop alone lets that one through.

Found by mutation: removing the second gate passed every other test in the repository.

**And the test for it is easy to write vacuously.** The move-out must come **first** in the batch,
so its `IN_MOVED_FROM` is recorded while the watcher is still enabled and the create's handler
stops it afterwards. With the create first, the per-event gate drops the move-out before it is
ever recorded and the second gate is unreachable — which is exactly what the first cut of that
test measured, and it passed against the mutation. The ordering is now stated in the test.

## 5. To migrate

Nothing to change. If you relied on receiving the remainder of a batch after stopping — you could
not have, deliberately: there was no way to know how many events were still queued.

## 6. Evidence

| Mutation | Caught |
|---|---|
| Remove the per-event gate (the pre-#2105 loop) | ✅ |
| Remove the unpaired-rename gate | ✅ — **only after** the batch was reordered; see §4 |
| `break` instead of `continue` at the per-event gate | **equivalent, and the code says so** |

The third is recorded rather than counted: because the unpaired-rename loop is gated on the same
flag, the two spellings are observably identical. `continue` is chosen for fidelity to .NET's
drop-and-keep-walking, not because a test distinguishes them, and the comment at the site says
exactly that rather than claiming a rationale it does not have.

## 7. Downstream

Measured per SA-2 condition 5: neither `cna` nor `mobile-eggbert` *uses* `FileSystemWatcher` —
**zero code sites in both**. The two textual matches in `cna` are planning notes, one of which
records a past Emscripten build fix in this file. Neither repository was modified.
