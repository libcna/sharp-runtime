<!-- SPDX-License-Identifier: MIT -->
# Migration — `XObject` Changed/Changing notification is implemented (#2199)

Ticket **#2199** (SR-AUD-336), landed 2026-08-19. Both of its gates were opened the same day
(`docs/StandingApprovals.md` SA-13).

## What changed

Until now the four accessors **accepted a handler and discarded it**, and no mutation anywhere in
the hierarchy raised anything. `XLinqChangeNotificationTests.cpp` pinned that inert surface at
every door, deliberately, so a partial implementation could not land silently and so the eventual
repair would have failing tests to turn green. This is that repair.

```cpp
// before                                   // after
void add_Changed(const Handler&);           [[nodiscard]] XObjectChangeRegistration add_Changed(const Handler&);
void remove_Changed(const Handler&);        bool remove_Changed(const XObjectChangeRegistration&) noexcept;
void add_Changing(const Handler&);          [[nodiscard]] XObjectChangeRegistration add_Changing(const Handler&);
void remove_Changing(const Handler&);       bool remove_Changing(const XObjectChangeRegistration&) noexcept;
```

## Gate 1 — the layout growth

`sizeof(XObject)` **16 → 24**, and every derived type with it. One pointer: a `unique_ptr` to the
registration block, allocated **only on first registration**, so an unobserved tree pays a null
pointer and no allocation — the closest analogue of .NET's annotation slot, which is likewise
absent until something is annotated.

| type | before | after |
|---|---|---|
| `XObject`, `XNode` | 16 | 24 |
| `XContainer` | 40 | 48 |
| `XElement` | 128 | 136 |
| `XAttribute` | 120 | 128 |
| `XText`, `XCData`, `XComment` | 48 | 56 |
| `XProcessingInstruction` | 80 | 88 |
| `XDocument` | 56 | 64 |

Silent binary break; **every consumer must rebuild**. Measured: **zero** `Xml::Linq` sites in `cna`
and **zero** in `mobile-eggbert`. **Three** shipped layout pins had to be updated — two `sizeof`
pins and one `static_assert` block from #1890 — and all three failed the build, which is the
evidence they were load-bearing.

## Gate 2 — the removal design, and the one deliberate divergence

.NET removes a registration by passing the **delegate** back, because a C# delegate is
equality-comparable. `XObjectChangeEventHandler` is a `std::function`, and `std::function` has **no
`operator==` against another `std::function`** — proved at compile time, not assumed. A
handler-taking `remove_*` therefore cannot identify which registration a caller means. **This was
never a cost question**; it is not implementable as declared, at any layout cost.

The decision was a **registration token**, chosen so the divergence is visible *in the type* rather
than hidden in the behaviour. Two alternatives were offered and declined:

* keep .NET's signature and remove **all** registrations — silently drops a third party's subscription;
* keep it and **throw** from `remove_*` — a subscriber could then never unsubscribe.

```cpp
auto token = element.add_Changed(handler);   // was: element.add_Changed(handler);
element.remove_Changed(token);               // was: element.remove_Changed(handler);
```

`add_*` is `[[nodiscard]]` deliberately: a discarded token is a registration that can never be
removed. Ids come from a **process-wide** atomic counter, so a token issued by one `XObject` can
never match a registration on another; a foreign or default-constructed token removes nothing and
returns `false`.

## Semantics, all transcribed from the reference

* **Notifications bubble.** Every object from the changed one up to the root is notified,
  **innermost first** (`XObject.cs:418-460`).
* **The sender is the object *changed*, not the object *observed*.** For `Add` and `Remove` .NET
  walks the **parent's** chain with the **child** as sender (`XLinq.cs:156,177`) — an asymmetry
  reproduced rather than tidied.
* **`Changing` runs before the mutation, `Changed` after**, and `Changed` is guarded on what
  `Changing` returned, as every .NET call site does.
* **`notify` means "any object on the chain carries registrations at all", not "a changing handler
  ran".** .NET tests `Annotation<XObjectChangeAnnotation>() != null` and only then invokes the
  possibly-null delegate. Reading it the other way would **silently disable every `Changed`-only
  subscription** — the case `Fix2199_AChangedOnlySubscriptionStillReceivesChanged` exists for that.
* **Handlers are invoked from a snapshot**, so a handler may register or unregister during a
  notification; the change takes effect on the next one.

### Two places where raising nothing is the correct answer

* **`XElement::setValueProperty` raises no `Value`.** .NET's setter is `RemoveNodes(); Add(value);`,
  so a subscriber sees a `Remove` pair per existing child then an `Add` pair. Raising `Value` would
  invent an event .NET does not raise.
* **`Add(std::string)` raises `Add` or `Value` depending on merging**, because it merges into a
  trailing `XText` rather than creating a sibling; an empty string is a genuine no-op and raises
  nothing.

## Mutation testing

Nine mutations, **all caught — but four only after a test was repaired, and the repairs are the
interesting part.**

| # | Mutation | Caught by |
|---|---|---|
| M1 | `notify` means "a changing handler ran" | two ancestor-walk cases |
| M2 | the walk stops at the first registration | two bubbling cases |
| M3 | iterate the live vector instead of a snapshot | the reentrancy case, **after repair** |
| M4 | ids `thread_local` instead of process-wide | the threaded token case, **added for it** |
| M4b | ids per-object | the foreign-token case, **after repair** |
| M5 | `Add`'s *Changing* sender is the parent | the add case, **after repair** |
| M6 | `RemoveNodes` raises once, not per child | the per-child case |
| M7 | `Changed` raised before the mutation | the before/after state case |
| M8 | attribute `Add` raises nothing | the attribute add case |
| M9 | `RemoveAttribute`'s *Changing* kind is `Value` | the attribute remove case, **after repair** |

**Three of the four repairs share one root cause and it is worth stating plainly.** The recorder
captured only the **`Changed`** half's kinds and senders, so any mutation that corrupted the
**`Changing`** half alone (M5, M9) passed. Half a pair is still a wrong notification, so the
recorder now records both and every case asserts they agree.

The fourth (M4b) was a **vacuous assertion**: the foreign-token case removed A's registration
*before* trying B's token, leaving A's list empty — so it returned `false` for the wrong reason and
a per-object counter went undetected. The foreign attempt now comes first, while A's registration
is still present.

M3's repair is the same shape: the reentrancy case removed the running handler as well as adding
one, which ended the live-vector iteration early and hid the difference. It now only adds, and adds
enough to force a reallocation.

## SA-2 conditions

1. This note. ✔
2. `test/consumer/xml_linq_change_registration_token_negative.cpp` — **5 sites**, the third being
   the spelling most likely to survive a careless migration: keeping the old `add_Changed(h);` line
   and discarding the token. Fixture set grows to **45 fixtures / 231 sites**. ✔
3. Downstream ticket: **#2394**. ✔
4. Full gate. ✔
5. Measured consumer impact: **zero sites** in both. ✔
