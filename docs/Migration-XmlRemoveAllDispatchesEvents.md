<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `RemoveAll` dispatches the node-change pair, and its children survive (ticket #2086)

*2026-08-19.* `XmlNode::RemoveAllChildren` (reached by `RemoveAll`, `setInnerTextProperty` and
`setInnerXmlProperty`) raised **no** node-change event and **destroyed** its children. It now
removes each child through `RemoveChild`, exactly as .NET does.

Landed under `docs/StandingApprovals.md` **SA-5**. No signature, layout or vtable change.

---

## 1. The reference selects one of the two candidates, so it is derived rather than chosen

#2086 recorded two approaches with **neither selected** and both needing evidence:

> *(a) raise `NodeRemoving` for every child before the bulk delete and `NodeRemoved` never —
> rejected as shipped because an asymmetric pair misleads a consumer counting them; (b) detach
> each child through `XmlDocument::DetachNode` instead of `DeleteChildren` … but changes
> lifetime/memory semantics … and is therefore **NOT compatible without approval**.*

`XmlNode.RemoveAll` is literally a loop over `RemoveChild`:

```csharp
public virtual void RemoveAll()
{
    XmlNode? child = FirstChild;
    XmlNode? sibling;
    while (child != null)
    {
        sibling = child.NextSibling;
        RemoveChild(child);
        child = sibling;
    }
}
```

That is candidate (b), and it needs no approval because it is the reference. This port's
`RemoveChild` already detaches rather than destroying — `XmlDocument::DetachNode` moves the node
under a scratch parent — so **the borrowed-pointer hazard #2079 refused to introduce does not
arise on this route at all**: the wrapper a handler receives names a live object.

## 2. What changed

| | Was | Is |
|---|---|---|
| `RemoveAll()` on 2 children | no events | `Removing`, `Removed`, `Removing`, `Removed` |
| the `XmlNode*` a handler receives | — | a **live** child, usable for the whole call |
| a removed child after the call | destroyed | alive, `ParentNode == nullptr` |
| `setInnerTextProperty` over existing content | inserts only | removals **then** inserts |
| a node with no children | no events | no events |

The sibling is captured **before** the removal, as .NET does, because detaching re-parents the
child and its `NextSibling()` then walks the holder's list instead of this node's. A mutation
reading it afterwards is caught.

## 3. The cost, stated

Detached children live until the document does. That is **not a new policy** — every
`RemoveChild` in this port has behaved that way since the detached holder was introduced — but
this door is reached by the `InnerText`/`InnerXml` setters, so repeatedly reassigning them now
accumulates orphans instead of freeing them. It is the same trade .NET makes, minus the garbage
collector.

A caller who sets `InnerXml` in a tight loop on a long-lived `XmlDocument` will see memory grow
where it previously did not. The remedy is the one .NET users have: scope the document.

## 4. Evidence

Four mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| back to the destroying implementation | 2 cases |
| the sibling read *after* the removal | 2 cases |
| only the first child removed | 2 cases |
| children purged but not removed | 6 cases, four of them pre-existing |

A fifth attempt was a **no-op rather than a mutation** and is recorded as such: guarding the loop
with `child == native_->FirstChild()` is always true, because removing the head makes the next
child the head. It was reformulated rather than counted.

**ASan discharges the acceptance criterion.** The AC asks that *"every `XmlNode*` a handler
receives names a LIVE object for the whole handler call, proven under ASan"*:

* the repair is **clean** under AddressSanitizer across all 29 cases in the two affected suites;
* the shape #2079 refused to introduce — raise `NodeRemoved` after destroying the child —
  reports `heap-use-after-free` at the handler's own dereference.

So the tool discriminates, which is what makes the clean run mean something.

## 5. Downstream, measured

`cna` and `mobile-eggbert` call `RemoveAll` in **zero** code sites. Neither was modified.
