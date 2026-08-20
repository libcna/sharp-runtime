<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `CurrentCulture` is per-thread, with a process-wide fallback (ticket #2409)

*2026-08-20.* `CultureInfo::CurrentCulture` and `CurrentUICulture` were **process-wide statics**. A
set on one thread changed what every other thread read, and a concurrent get/set was an
**unsynchronised read/write of a non-atomic object** — a data race, not merely a surprising value.
**The type's own doc-comment already said *"the current thread's culture"***, so documentation and
behaviour disagreed.

Landed under **SA-14 decision 2**, which granted this as a **separate ticket ahead of #1940**.

**Downstream, measured:** **zero** `CultureInfo` sites of any kind in `cna` and in
`mobile-eggbert`. Downstream record: **#2411**.

---

## 1. Why the obvious repair was the wrong one

Making the two members `thread_local` would have fixed the race — and **silently removed the
process-wide setting this port accidentally had**, with no replacement and no diagnostic. Code that
sets the culture once at startup and expects every thread to see it would simply stop working.

.NET's own answer is a **second property**. `CultureInfo.cs:358-366`:

```csharp
public static CultureInfo CurrentCulture =>
    s_currentThreadCulture ?? s_DefaultThreadCurrentCulture ?? s_userDefaultCulture ?? ...;
```

`s_currentThreadCulture` is `[ThreadStatic]` (`:111-112`) and `DefaultThreadCurrentCulture` is a
real public static property (`:407-413`). This port now has the same three-step chain:

| step | this port | meaning |
|---|---|---|
| 1 | `currentCulture_` — `thread_local`, **absent** by default | this thread's own choice |
| 2 | `DefaultThreadCurrentCulture` — process-wide, `std::nullopt` by default | what a thread that chose nothing gets |
| 3 | `getInvariantCultureProperty()` | last resort |

**Absent rather than invariant-valued at step 1 is load-bearing.** *"This thread has not chosen"*
and *"this thread chose the invariant culture"* are different facts, and only the first may fall
through to step 2.

## 2. New public surface

```cpp
static std::optional<CultureInfo> getDefaultThreadCurrentCultureProperty();
static void setDefaultThreadCurrentCultureProperty(const std::optional<CultureInfo>&);
static std::optional<CultureInfo> getDefaultThreadCurrentUICultureProperty();
static void setDefaultThreadCurrentUICultureProperty(const std::optional<CultureInfo>&);
```

`std::nullopt` is .NET's `null`: *no process-wide default*, so the chain falls through to the
invariant culture.

## 3. What a caller has to change

**If you set the culture once at startup and expect the whole process to see it, that call must
move**:

| Was | Now |
|---|---|
| `CultureInfo::setCurrentCultureProperty(c)` — visible everywhere | `CultureInfo::setDefaultThreadCurrentCultureProperty(c)` |
| `CultureInfo::setCurrentCultureProperty(c)` — meant for this thread | unchanged; it now genuinely means this thread |

Reading is unchanged: `getCurrentCultureProperty()` still returns `const CultureInfo&` and still
resolves to the invariant culture when nothing has been set.

### 3.1 The save/restore idiom no longer restores what it used to

This is subtle and it bit this repository's own tests, so it is worth stating plainly. The idiom

```cpp
CultureInfo previous = CultureInfo::getCurrentCultureProperty();
CultureInfo::setCurrentCultureProperty(CultureInfo("de-DE"));
// ...
CultureInfo::setCurrentCultureProperty(previous);   // "restore"
```

does **not** restore the original state. It turns *"this thread has chosen nothing"* into *"this
thread has explicitly chosen the invariant culture"* — and only the first falls through to the
process-wide default. **.NET has exactly the same asymmetry**: its `CurrentCulture` setter takes a
non-null value, so there is no way to un-choose there either. A test or a scope guard that wants the
fall-through behaviour back must run its work on a thread that then exits, which is what this
repository's own new cases do.

## 4. Why the reference stays valid, and what it costs

`getCurrentCultureProperty()` returns `const CultureInfo&`. A step-2 fallback that read the shared
object and returned a reference **into** it would have moved the race one level down: the next store
drops the last owner and frees the object the caller is still holding.

The process-wide default is therefore an `std::atomic<std::shared_ptr<const CultureInfo>>`, and the
reader **parks the loaded pointer in a `thread_local` holder** before dereferencing it. The held
reference then stays valid until that thread next reads — it may be **stale**, but it is never
**invalid**, and a case asserts exactly that.

`shared_ptr` rather than a plain static also keeps the hot path cheap: `CultureInfo` holds
`NumberFormatInfo` and `DateTimeFormatInfo` **by value**, so a per-read deep copy would be paid on
every formatting call. A pointer swap costs a refcount bump.

## 5. Testing, and one result worth carrying forward

Six mutations, all caught — two at compile time (restoring `static` storage), and the rest
behaviourally.

**The ownership case had to be made deterministic before it caught anything.** It first ran a churn
thread swapping the default in a tight loop while a reader took and read a reference: over 20,000
iterations, **neither gtest nor ASan ever caught** the mutation that drops the holder. The window is
nanoseconds wide and simply never landed. Replacing the default **from the same thread** removes the
timing entirely — the store drops the last reference at a known point — and the mutation is then
caught on **every** run. A test that catches a defect only sometimes is not evidence (#2352).

**Two test defects of my own were found and are recorded rather than quietly fixed.** The first cut
set the culture on the *test* thread and never restored it, which leaked into the rest of the binary
and made three unrelated pre-existing cases fail under mutation — a defect in the test, not evidence
about the code. Every thread-culture set now happens on a spawned thread that then exits. And a
first mutation run reported three false *NOT CAUGHT* results because the harness's gtest filter
still named the fixture by its old name; that is a harness error, not a result.
