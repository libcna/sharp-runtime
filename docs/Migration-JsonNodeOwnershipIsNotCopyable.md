<!-- SPDX-License-Identifier: MIT -->
# Migration — `JsonNode` is not copyable, and `DetachParent` is not public (#1888)

Ticket **#1888** (SR-AUD-327, CCF-019), landed 2026-08-19 on an explicit approval after being
declined since July.

## What changed

```cpp
JsonNode(const JsonNode&) = delete;   JsonNode& operator=(const JsonNode&) = delete;
JsonNode(JsonNode&&)      = delete;   JsonNode& operator=(JsonNode&&)      = delete;
protected: void DetachParent();       // was public; JsonArray and JsonObject are friends
```

## The three measured defects it closes

| probe | what it did |
|---|---|
| **J08** | `JsonArray copy = *orig` gave a second array sharing the **same children**, each still reporting the **original** as its parent |
| **J09** | `nodeRefA = nodeRefB` **sliced**, rewriting `parent_` on a node still stored in a container |
| **J13** | public `DetachParent()` let a caller sever the link a container believes it owns and put the same node into a **second** container |

**A .NET `JsonNode` is a reference type**, so there was never an object copy to translate —
assigning one C# variable to another copies a reference. All four members were a C++ artefact, and
`System::Xml::Linq::XObject` already deleted all four, so this **ends an asymmetry inside the port**
rather than inventing a restriction.

## The header's own note about `DetachParent` was wrong, and the reference corrects it

It said the member "mirrors `JsonNode.cs`'s internal `DetachParent`". **There is no `DetachParent`
on `JsonNode.cs` at all.** .NET puts it on the *containers*, as a **private** helper on each
(`JsonObject.cs:316`, `JsonArray.IList.cs:231`), whose whole body is `item?.Parent = null` — and
`Parent`'s setter is `internal`. So in .NET a consumer can neither call it nor reach what it does.
Protected-plus-friends is that reachability expressed in C++.

## Migration

* a copy → **`DeepClone()`**, which does what the implicit copy did not: the clone's children are
  its own and name the **clone** as their parent;
* moving a node between containers → **`Remove`/`Add`**, which keeps the links and the containers in
  step.

**Measured impact: zero.** No first-party copy/assign site existed, and `cna` and `mobile-eggbert`
have zero `JsonNode`/`JsonArray`/`JsonObject` sites. (`cna`'s single `JsonObject` match is its own
`ExtractJsonObjectFieldEXT` helper, an unrelated name.)

## Four shipped pins were inverted, and one was not where the measurement said to look

Three were the probe-case pins, each carrying a `NOLINT - deliberate: pins today's implicit copy`
marker. The fourth was **not found by the initial measurement**: `JsonNodeTeardownTests.cpp` built
its second container with `std::make_shared<JsonArray>(realOwner)` — a copy-construction commented
*"shares children"*. A grep for `X = *y` patterns missed it; the **compiler** found it. Its real
subject is #1886's `== this` guard, so it is rewritten to reach that guard through two containers
that genuinely hold different children, and a second case covers the moved-child form.

## SA-2 conditions

1. This note. ✔
2. **`test/consumer/text_json_node_lifetime_negative.cpp` — 5 sites.** This is the fixture #1894's
   acceptance criteria named and could not write for seven weeks: its notes recorded, correctly,
   that it *"cannot be started, not merely should not be"*, because no CCF-019 repair had outlawed
   any spelling. #1888 is the one that finally did. Fixture set **45 / 231 → 46 / 236**. ✔
3. Downstream ticket **#2396**. ✔
4. Full gate. ✔
5. Measured consumer impact: **zero sites** in both. ✔

## Mutation testing

Three mutations. **M1** (restore the copy members) and **M3** (make `DetachParent` public again) are
caught at compile time. **M2 (restore the move members) is NOT caught, and that is reported rather
than dressed up** — it is a **proven equivalence**, for two independent reasons measured with a
probe:

* `JsonNode` is **abstract** (three pure virtuals), so `is_move_constructible_v<JsonNode>` is false
  whatever those declarations say;
* `JsonArray` and `JsonObject` each have a **user-declared destructor** (#1895's iterative
  teardown), which suppresses their implicit move constructors — so their move-constructibility
  falls back to the copy constructor, already deleted.

The deletion is kept because it states the intent and becomes load-bearing the day a container drops
its destructor. Both the header and the fixture's site 3 say exactly this, so a later reader is not
misled into thinking the deletion is what rejects the spelling today.
