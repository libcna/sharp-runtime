<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ArraySegment::CopyTo` rejects a short destination (ticket #2328)

*2026-08-18.* `ArraySegment<T>::CopyTo(std::vector<T>&, index)` **resized** the destination
whenever it was too short. .NET's body is `Array.Copy`, and .NET arrays cannot grow.

Landed under `docs/StandingApprovals.md` **SA-8**. A **narrowing**: a call that used to succeed by
enlarging the destination now throws. No signature, layout, vtable or `noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `seg.CopyTo(dest)` with `dest.size() < count` | **grew `dest`** | **`ArgumentException`**, `dest` untouched |
| `seg.CopyTo(dest, 3)` with an empty `dest` | grew `dest` to `3 + count` | **`ArgumentException`**, `dest` untouched |
| destination exactly large enough | worked | **unchanged** |
| a default segment | `InvalidOperationException` | **unchanged**, and still checked **first** |
| negative `destinationIndex` | `ArgumentOutOfRangeException` | **unchanged** |
| `CopyTo(ArraySegment<T>&)` | already rejected | **unchanged** |

The message is `Strings.resx:478-480`, transcribed: *"Destination array was not long enough. Check
the destination index, length, and the array's lower bounds."*

## 2. Why

Two independent reasons, and both matter.

**It diverged from .NET.** The body is `Array.Copy(_array, _offset, destination,
destinationIndex, _count)` (`ArraySegment.cs:106-110`), which raises
`ArgumentException(SR.Arg_LongerThanDestArray)` when the destination is short. A caller who passed
the wrong buffer got a silently enlarged one instead of a diagnostic, and a caller who sized a
buffer deliberately had that size overwritten.

**It was the one place in this repository that broke its own convention.** Of the twenty-six
`CopyTo(std::vector<T>&, …)` overloads in `modules/`, twenty already rejected a short destination —
thirteen with their own guard, five through
`System::Collections::detail::requireValidCopyDestination`, and `ImmutableList`'s two forwarding
forms through its four-argument body.

The check is written out in `ArraySegment.hpp` rather than shared, because that helper lives in
`Collections.Core` and `Core.Base` must not depend on it. The **message** is .NET's own, so the two
cannot drift apart in wording.

## 3. To migrate

Size the destination first:

```cpp
// before
std::vector<int> dest;
seg.CopyTo(dest);

// after
std::vector<int> dest(seg.getCountProperty());
seg.CopyTo(dest);

// with an index, the index counts against the room
std::vector<int> dest(3 + seg.getCountProperty());
seg.CopyTo(dest, 3);
```

Or use `ToArray()`, which allocates for you and is unchanged.

## 4. First party

Five tests asserted the resize, not the one the finding named. Four were predicted by the review
(`CopyTo_Vector_CopiesAllElements`, `CopyTo_Vector_PartialSegment`,
`CopyTo_VectorWithOffset_ExpandsDest`, and `NonOverlappingCopiesKeepTheirPreviousResults`, which
carried the comment `// SR-AUD-055's resize, unchanged`). **A fifth,
`NonDefaultSegmentsAreUnaffected`, was not** — it was found by the full gate after a filtered run
had already passed, which is why the gate is run on the whole repository rather than on the
suites a change looks like it touches.

`CopyTo_VectorWithOffset_ExpandsDest` was named for the behaviour and is **inverted** rather than
patched. #2214 had deliberately *preserved* the resize while repairing this file's default-state
and overlap defects; that preservation was correct then and is superseded now.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `ArraySegment` — **zero sites in both** (measured for
#2215 on the same date, and re-checked). Neither repository was modified.
