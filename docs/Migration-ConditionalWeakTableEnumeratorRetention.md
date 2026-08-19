<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the weak-table enumerator retains only `Current`, and `Reset()` does nothing (ticket #1981)

*2026-08-19.* `System::Runtime::CompilerServices::ConditionalWeakTable<TKey,TValue>`'s enumerator
no longer keeps every snapshotted value alive, and `Reset()` is now an empty method, matching
.NET.

**One behaviour is removed deliberately: `Reset()` no longer restarts an enumeration.** Read §3
before upgrading. Landed under **SA-5** (the `Reset()` semantics) and **SA-3** (the enumerator's
storage), with both sets of conditions discharged below.

---

## 1. What was wrong

The enumerator's snapshot was a `std::vector<Entry>`, and `Entry::value` is a **strong**
`shared_ptr`. So an enumerator kept every value it had snapshotted alive for its own lifetime —
including values the table had since released.

Measured before the repair (`build-probe/1981_probe1_layout.cpp`):

```
after table.Remove, value still alive (enumerator retains it) = 1
after deleting the enumerator, value alive = 0
```

and after it:

```
after table.Remove, value still alive (enumerator retains it) = 0
```

For a type whose entire purpose is *not* to keep values alive, an enumerator that does is a
defect of the primary contract, not a detail.

## 2. What changed

| | Was | Is |
|---|---|---|
| snapshot row | `weak_ptr<TKey>` + **strong** `shared_ptr<TValue>` | `weak_ptr<TKey>` + **weak** `weak_ptr<TValue>` |
| what the enumerator retains | every snapshotted value | only `Current` |
| an entry released after the snapshot | yielded, with the stale value | **skipped** |
| `Reset()` | rewound the index and cleared `Current` | **does nothing** |
| `sizeof(ConditionalWeakTable<int,int>)` | **72** | **72** |
| `GetEnumerator()`'s signature | `IEnumerator<Pair>*` | unchanged |

`Reset()` is transcribed from `ConditionalWeakTable.cs:492`, which is literally
`public void Reset() { }`.

## 3. The removed capability

A caller that used `Reset()` to enumerate the same snapshot twice can no longer do so. That is
.NET's behaviour, and there is no replacement on the enumerator: call `GetEnumerator()` again to
get a fresh snapshot.

```cpp
// before — worked here, never worked in .NET
while (e->MoveNext()) { /* ... */ }
e->Reset();
while (e->MoveNext()) { /* ... again ... */ }

// after
{ std::unique_ptr<IEnumerator<Pair>> e(table.GetEnumerator()); while (e->MoveNext()) { /* ... */ } }
{ std::unique_ptr<IEnumerator<Pair>> e(table.GetEnumerator()); while (e->MoveNext()) { /* ... */ } }
```

Because .NET's `Reset()` body is empty it also does **not** clear `Current`, so `Current()` after
`Reset()` still returns the last element. A "no-op" that still reset the current-element flag
would be wrong in the other direction, and that is pinned separately.

**No existing test in this repository relied on the old behaviour** — all 174
`SharpRuntimeTests_Runtime` cases passed unchanged before the new ones were added.

## 4. .NET's design is deliberately not reproduced, and the reason is recorded

.NET's enumerator holds **no snapshot at all**. It keeps a reference to the table plus an index
range and reads the live container under the table's lock at every `MoveNext`
(`ConditionalWeakTable.cs:441-478`), which is why it retains only `_current`.

That design requires the enumerator to hold a **borrowed pointer to the table**. This port's
`GetEnumerator()` hands the caller a raw `IEnumerator<Pair>*` whose lifetime the table does not
control, so reproducing it would introduce exactly the CCF-019 defect class this programme has
spent the session removing — an object outliving the thing it points into.

A snapshot of **weak** references reaches the same observable contract (retain only `Current`;
skip anything released in the meantime) without that hazard. The deviation is in the header, at
the site, with this reasoning.

## 5. SA-3's conditions, discharged

* **No vtable change** — `Enumerator` already derived from `IEnumerator<Pair>`; its virtuals are
  unchanged.
* **No mangled-symbol, signature or `noexcept` change** on anything a consumer can name.
  `GetEnumerator()` still returns `IEnumerator<Pair>*`.
* **`sizeof` pinned** — `Decl1981_TheTablesOwnLayoutIsUnchanged` asserts **72** before and after.
  The point worth stating: `Enumerator` is a **private nested class** that `GetEnumerator()`
  heap-allocates, so no consumer can name it, size it or hold one by value. Its storage change
  is invisible through every public spelling, which makes this materially cheaper than the design
  record's "object-layout change in a header-only template" implied.
* **Full-consumer-rebuild requirement**: it is a header-only template, so any consumer that
  instantiates `ConditionalWeakTable` must be recompiled. Nothing needs a source edit.
* **Full gate**: **17,451 run, 17,451 passed, 0 failed, 0 skipped** across 38 executables — `+7`
  on 17,444, exactly the seven new cases (`SharpRuntimeTests_Runtime` 174 → 181). No other
  executable moved. Module graph unchanged at 41/93.

## 6. Evidence

Three mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the snapshot holds strong values again | `Fix1981_TheEnumeratorDoesNotRetainAReleasedValue`, `Fix1981_OnlyCurrentIsRetained` |
| M2 — `Reset()` rewinds again | `Fix1981_ResetDoesNothingAndCannotReEnumerate`, `Fix1981_ResetLeavesCurrentUntouched` |
| M3 — `MoveNext` tests only the key, not the value | `Fix1981_AnEntryReleasedAfterTheSnapshotIsSkipped` — **only after that case was added** |

**M3 is the one worth recording.** It went uncaught at first because no case enumerated *after* a
`Remove`, and that is the only situation where the key is alive and the value is not: the caller
still holds the key, so `key.lock()` succeeds, while the table has dropped the value. Testing the
key alone would have yielded a pair with a **null value** and counted it as a live entry.

M1 was invalid as first written — it referenced a member that does not exist — and was
reformulated as the real revert rather than counted.

## 7. Downstream, measured

`ConditionalWeakTable` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`.
Neither repository was modified.
