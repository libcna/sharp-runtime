<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a `JsonElement` reports its document's disposal, and `sizeof` grows 48 → 56 (ticket #2117)

*2026-08-19.* A `JsonElement` captured before `JsonDocument::Dispose()` kept answering. It now
raises `System::ObjectDisposedException`, as .NET does.

Landed under `docs/StandingApprovals.md` **SA-3** (private data members; `sizeof` pinned by a
layout test, no vtable, base-class, signature or `noexcept` change). **Downstream consumers must
be recompiled**; no source change is needed.

---

## 1. The gate was SA-3 all along

#2117's recorded blocker: *"enforcing it needs shared disposal state reachable from every
element, i.e. an OBJECT-LAYOUT CHANGE to `JsonElement`. No design is guessed here; it is gated
exactly as `modules/io`'s #2098 is."*

#2098 landed under SA-3 (Approval IO-1) on 2026-08-18. SA-3 covers exactly this shape, so the
gate is discharged rather than waived.

**And the review's framing correction stands:** this was never a use-after-free. `JsonElement`
held an owning aliasing `shared_ptr`, so a captured element kept the tree alive and read **live**
storage. The defect was a *disposed document still serving data*.

## 2. The design is .NET's, not a workaround

.NET's `JsonElement` holds a `JsonDocument _parent` and an index, and delegates every accessor to
`_parent.GetXxx(_idx)`; each of those begins with `CheckNotDisposed()` — some twenty call sites in
`JsonDocument.cs`. **The flag lives with the document, and the element reaches it through the
reference it already holds.**

This port now does the same: the element points at a shared `detail::JsonDocumentState` and
carries the node as a raw pointer into it — the direct counterpart of `_parent` plus `_idx`.

| | Was | Is |
|---|---|---|
| `JsonElement` | `shared_ptr<const ordered_json>` aliasing the node | `shared_ptr<JsonDocumentState>` + `const ordered_json*` |
| `JsonDocument` | `shared_ptr<const ordered_json>` + a separate `bool disposed_` | one `shared_ptr<JsonDocumentState>` |
| `sizeof(JsonElement)` | **48** | **56** |

Two flags for one fact is what let the document and its elements disagree, so the separate
`disposed_` bool is **gone** rather than mirrored.

A side effect worth noting: deriving a child element no longer builds an aliasing `shared_ptr` at
all — `JsonElement(state_, &(*it))` — so five construction sites got simpler.

## 3. What changed observably

| Call after `doc->Dispose()` | Was | Is |
|---|---|---|
| `captured.GetInt32()` | `10` | `ObjectDisposedException` |
| `captured.getValueKindProperty()` | the kind | `ObjectDisposedException` |
| `captured.GetRawText()`, `ToString()` | the text | `ObjectDisposedException` |
| `array[1]`, `GetArrayLength()`, `GetProperty(…)` | answered | `ObjectDisposedException` |
| `captured.Clone()` | a copy | `ObjectDisposedException` |
| `doc->getRootElementProperty()` | already threw | **unchanged** |
| double `Dispose()` | already safe | **unchanged** |
| a **default** `JsonElement` | `Undefined` | **unchanged** (§4) |
| a `Clone()` taken **before** disposal | — | **still works** (§4) |

`ValueKind` throwing is easy to leave out and is not optional: .NET's reads
`_parent.GetJsonTokenType`, which begins with `CheckNotDisposed()`. A mutation skipping it there
is caught.

## 4. Two boundaries that are .NET's, not conveniences

**A default element is undefined, not disposed.** .NET keeps them apart:
`CheckValidInstance()` raises `InvalidOperationException` for a null parent, `CheckNotDisposed()`
raises `ObjectDisposedException`. A default element here has no document, so it must keep
answering "undefined" rather than claiming a disposal that never happened. Writing the guard as
`if (!node_) throw` would fail that, and the mutation is caught.

**A clone survives.** `Clone()` gives the copy its own state, so it outlives the original
document — .NET's `Clone()` delegates to `_parent.CloneElement(_idx)`, producing an element rooted
in a **new** document. A mutation that shares the original state instead is caught.

## 5. `Dispose()` still retains the tree, and now buys something for it

`Dispose()` does not free the parsed tree. .NET frees its buffer there, but .NET's elements hold a
reference to the *document* and are simply told they are disposed; this port's elements hold a
reference to the **state** and would read freed storage if it vanished under them.

Retention is what makes the diagnostic safe. It is the cost the class note has recorded since
#2110 — now paid for a check rather than for nothing.

## 6. Evidence

Five mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| the guard never fires | 2 cases |
| the guard also fires for a default element | `Fix2117_ADefaultElementIsUndefinedNotDisposed` |
| `Dispose()` does not set the flag | 2 cases |
| `Clone()` shares the original state | `Fix2117_DisposalReachesElementsHandedOutEarlier` |
| `ValueKind` skips the guard | 2 cases |

Two pins were inverted — `JsonReviewPinTests.DisposalGuardsThatALREADYWorkAndTheOneThatDoesNot`
and `JsonGatedBehaviourPins.PIN2117…` — each of which asserted the defect verbatim.

The layout pin uses shadow structs rather than bare numbers, and asserts that the two shadows
**differ by exactly one pointer**, so a change to any member's own size shows up as a mismatch
instead of a number to re-guess.

## 7. Downstream, measured

`cna` and `mobile-eggbert` reference `JsonElement` or `JsonDocument` in **zero** code sites.
Neither was modified. The full-rebuild requirement is recorded here for any future consumer.
