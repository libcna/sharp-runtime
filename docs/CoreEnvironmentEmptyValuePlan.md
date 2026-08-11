<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `Environment` empty value vs. deletion (SR-AUD-106) — review record

Ticket #2311. The review of the one frozen `Environment.cpp` finding that
ticket #2239 deliberately excluded from its compatible slice, and that no
ticket has owned since. Reviewed 2026-08-11.

---

## 1. Ownership before this review

`plan.sqlite3` holds exactly one ticket mentioning SR-AUD-106: **#2239**, and
it mentions it only to say the finding is **not** in scope — in its
`description` ("state explicitly why SR-AUD-105 (XDG design) and SR-AUD-106
(public signature change) are NOT in this slice") and again in its
`acceptance_criteria`. `docs/CoreEnvironmentCompatibleSlicePlan.md` §10 records
the intended end state as "SR-AUD-105 and SR-AUD-106 still `confirmed` and
unclaimed", and the `Environment.cpp` audit report repeats it. So the finding
was unowned by construction, not by oversight.

**Search-method note.** A plain `LIKE '%SR-AUD-106%'` scan is not sufficient for
this repository. Audit checkpoints compress runs of identifiers
(`"confirm SR-AUD-173/174"`, `"SR-AUD-175/176"`), so a substring scan silently
misses the second member of every such pair. The scan behind this record
expands those runs. It does not change SR-AUD-106's answer; it changes
SR-AUD-174's (§ `CoreUnicodeCategoryTablePlan.md` §1).

---

## 2. The frozen finding, and what is still true

> The C++ overload accepts only `std::string`, then calls `unsetenv` whenever
> `value.empty()` (`Environment.cpp:207-218`). Current .NET has a nullable
> `string?` value: `null` deletes an entry, while `string.Empty` remains a
> valid present environment variable with an empty value. […] The C++ signature
> has no nullable/optional deletion state, so callers cannot express both
> operations. `EnvironmentTests.Set_Empty_RemovesVar` locks the incompatible
> behavior instead of checking map membership […]

**The port-side facts are still exactly as described**, now at
`Environment.cpp:214-227` (`::unsetenv` on POSIX when `value.empty()`;
`_putenv_s(name, "")` on Windows, which also removes). Re-measured live with
`build-probe/2311_probe4_live.cpp`, linked against the real
`Environment.cpp`:

```
os_setenv_empty_in_block=1 getenv_nonnull=1
map_contains_present_empty=1 map_value_len=0
after_set_value_in_block=1
after_set_empty_in_block=0 map_contains=0
get_present_empty='' get_absent='' indistinguishable=1
```

Row 3→4 is the finding reproduced: a variable that existed is gone after
`SetEnvironmentVariable(name, "")`.

---

## 3. Three premise corrections the measurement produced

### 3.1 The "signature change" is understated — the named remedy is a source break, not an addition

Both #2239 and the audit report price the repair as "a **public signature
change** (an `std::optional<std::string>` overload or equivalent)", which reads
as *additive*. Measured, it is not. `build-probe/2311_probe2_optional_overload.cpp`
declares exactly the current member plus that proposed overload and calls the
single most common spelling:

```
error: call of overloaded 'SetEnvironmentVariable(const char [2], const char [2])' is ambiguous
  candidate: static void Env::SetEnvironmentVariable(const std::string&, const std::string&)
  candidate: static void Env::SetEnvironmentVariable(const std::string&, std::optional<std::string>)
```

`const char*` → `const std::string&` and `const char*` → `std::optional<std::string>`
are both user-defined conversion sequences of equal rank, so **every existing
call site that passes a string literal stops compiling** — not only the ones
that wanted deletion. The overload named as the remedy is the one option that
breaks the most source.

### 3.2 The defect is not confined to the setter — the getter conflates the same two states

The finding frames SR-AUD-106 as a setter problem. The probe's last row shows
`GetEnvironmentVariable` returning `""` for a present-empty variable and for an
absent one, `indistinguishable=1`, because its return type is `std::string` and
has no absent state. The header states this as a deliberate port-wide
convention (`Environment.hpp:243-252`: "Real .NET's `GetEnvironmentVariable("")`
returns null […] which this mirrors with the runtime's empty-string
null-equivalent").

Consequence: **repairing only the setter leaves the round trip broken.** A
caller could create a present-empty variable and `GetEnvironmentVariable` would
still report it as absent. Any faithful repair is a change to the port's
null-representation convention across at least two public members, not one
parameter type.

### 3.3 A positive fact neither the finding nor the plan records

`GetEnvironmentVariables()` **already represents a present-empty variable
correctly** (`map_contains_present_empty=1`, `map_value_len=0`), because
`splitEnvEntry` rejects only an entry with no `=` and an entry whose `=` is
first — an empty value is accepted. The platform supports the state too
(`os_setenv_empty_in_block=1`). So the observability channel exists; only the
two single-name members conflate. This is what makes the test repair in §5
possible without settling anything.

---

## 4. The clause that is *not* true as written

> `EnvironmentTests.Set_Empty_RemovesVar` locks the incompatible behavior

It does not. The test's whole assertion is

```cpp
EXPECT_TRUE(Environment::GetEnvironmentVariable("SHARP_TEST_VAR2").empty());
```

and §3.2 measured that this getter answers `""` for **both** removal and a
present-empty value. The test therefore passes whether the key was removed, was
left present with an empty value, or — since the name is never asserted to
exist first — was never set at all. It does not lock the behaviour; it locks
nothing that distinguishes the two candidate contracts.

That is worse than the finding claims, and it is the part that matters for the
open decision: **today, changing `SetEnvironmentVariable` to store an empty
value instead of removing the key would leave the entire suite green.** The
decision in §6 cannot be taken safely while its outcome is unobservable.

---

## 5. The independently compatible subset — ticket #2312

Test-only. No production file, signature, layout, `noexcept` or symbol is
touched, and no behaviour changes; the current contract is simply asserted
through the channel that can see it (§3.3).

1. `Set_Empty_RemovesVar` asserts the key is present **before** the empty
   `Set`, and asserts its absence afterwards through
   `GetEnvironmentVariables()` membership rather than through the getter that
   cannot tell the difference. The weak getter assertion is kept beside it, now
   as a statement about the getter rather than about removal.
2. A POSIX-guarded companion pins §3.3 directly: a present-empty variable
   installed out of band survives in the map with an empty value, while both
   states read back identically through `GetEnvironmentVariable`.

This closes the finding's test clause and makes either outcome of §6
observable. It does **not** close SR-AUD-106, whose representational clause is
untouched.

---

## 6. The decision that cannot be taken here — ticket #2313 (`needs_user`)

Two independent grounds.

### 6.1 The reference premise is unverified and load-bearing

The finding asserts that in current .NET `string.Empty` "remains a valid present
environment variable with an empty value" while only `null` deletes. If that is
right, this port diverges. If instead .NET treats an empty value the same as
`null`, **this port is already correct and SR-AUD-106 is a false positive.**
The two readings imply opposite work, and nothing in this repository settles
it: the only in-repo statement is the port's own doc-comment ("Pass an empty
string for @p value to remove the variable"), which is a statement of this
port's contract, not of .NET's. `/rv` is absent, so the reference is
unavailable, and it must not be supplied from memory. The exact artefacts
needed are `SetEnvironmentVariableCore` in `Environment.Variables.Unix.cs` and
`Environment.Variables.Windows.cs`, and the documented remarks for
`Environment.SetEnvironmentVariable(String, String)`.

### 6.2 Every way of expressing "no value" has a price, and none is free

Measured with `build-probe/2311_probe3_options.cpp` (all three compile clean at
`-Wall -Wextra -Wpedantic -Werror`; the rejected fourth is §3.1):

| Option | Compiles today's call sites | Behaviour change | Cost |
|---|---|---|---|
| **A** — add `std::optional<std::string>` overload | **no** — ambiguous at every literal call site (§3.1) | — | source break, repo-wide and downstream |
| **B** — replace the parameter with `std::optional<std::string>` | yes | **yes, silent**: `Set(n, "")` stops deleting and starts storing an empty value | every existing caller that used `""` to delete is silently wrong; no diagnostic |
| **C** — additive `RemoveEnvironmentVariable(name)` | yes | none | does not fix the finding on its own: `Set(n, "")` still deletes, so a present-empty value is still unrepresentable |
| **D** — additive sentinel, `Set(n, Environment::NoValue)` | yes | none | same as C: additive only; the conflation in `Set(n, "")` survives |

C and D are compatible but incomplete: they add a way to say "delete" without
making `""` mean "present and empty", which is the state the finding says
callers cannot create. Making `""` mean that is B's silent behaviour change —
and it is silent precisely because the type of the value parameter does not
change.

**Also in scope of the decision, per §3.2:** whether `GetEnvironmentVariable`
keeps the empty-string-as-null convention. Leaving it changes the setter
without making the new state readable through the matching getter.

### 6.3 First-party consumers

`SetEnvironmentVariable` has **zero** first-party production call sites outside
`Environment` itself; the only caller is `EnvironmentTests.cpp`. So in-repo
migration for any option is one test file. Per project rule this does **not**
authorise a source break: this is a published static library and the header is
its contract, so options A and B are priced by downstream cost, which cannot be
measured from here.

---

## 7. Compatibility and ABI of what actually lands here

#2312 is test-only: no header, no `.cpp`, no signature, no layout, no vtable,
no `noexcept`, no symbol, no module-graph or component-dependency change. No
`PUBLIC_/PRIVATE_/TEST_DEPENDENCIES` edge changes, so selective-components is
not re-run.

## 8. Sanitizers

Not run, deliberately. The change is two GoogleTest bodies over `std::map`
lookups; there is no lifetime, memory, threading or overflow question in it,
and no sanitizer can observe a specification decision. The audit report's
separate "environ traversal under concurrent setenv" bullet is untouched and is
not this finding.

## 9. Disposition

SR-AUD-106 stays **`confirmed`**. It is a conjunction; its test clause closes
with #2312 and its representational clause is #2313 (`needs_user`). No
`SR-AUD-*` identifier is created; numbering stays frozen at 364.
