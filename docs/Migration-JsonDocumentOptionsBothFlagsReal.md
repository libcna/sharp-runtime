<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — both inert `JsonDocumentOptions` flags are real (ticket #2115)

*2026-08-18.* `AllowTrailingCommas` and `AllowDuplicateProperties` were **validated, stored and
never consulted**. `[1,]` was rejected whichever way the first was set, and `{"x":1,"x":2}` was
accepted as `x = 2` whichever way the second was.

Landed under SA-5 on the user's decision of the same date (`docs/StandingApprovals.md`, SA-11).

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `AllowTrailingCommas = true`, `[1,]` | `JsonException` | **parses to `[1]`** |
| `AllowTrailingCommas = false`, `[1,]` | `JsonException` | **unchanged** |
| `AllowDuplicateProperties = false`, `{"x":1,"x":2}` | `x = 2` | **`JsonException`** |
| `AllowDuplicateProperties = true` (the default) | `x = 2` | **unchanged** |
| `JsonSerializer::Deserialize` with either flag | inert | **works, identically** |
| `Validate()` | said nothing about either | **unchanged, and now correct** |

## 2. The cheaper option was declined, on purpose

The user was told the two flags are **not equally cheap** — nlohmann/json supports neither
natively — and was offered the alternative of making `Validate()` throw `NotSupportedException`
for whichever could not be implemented. The answer was to implement both, on the grounds that **a
switch which does nothing is worse than a switch which does not exist**.

That also settles §7.3 of #2120, which had complained that `Validate()` was silent about the two
inert flags: there is no inertness left to report, so `Validate()` staying silent is now correct
rather than a gap.

## 3. Trailing commas — a scanner, not a search

nlohmann has no trailing-comma mode, so the text is rewritten before parsing — **only when the
flag is set**, so the default path is byte-identical and cannot regress.

A `,` inside a **string literal** is data, and a `,` inside a **comment** is nothing. Both are
tracked, and comments are skipped only when `CommentHandling` allows them, so a `,]` inside a block
comment is left exactly as nlohmann would see it.

It removes a comma **before a closer and nothing else**. .NET's `AllowTrailingCommas` permits one
trailing comma per container, not a missing element, so these stay malformed:

```
[1,,2]     a missing element
[,1]       a leading comma
```

A rewrite that dropped every comma next to a bracket would silently accept documents .NET rejects.

## 4. Duplicate properties — the callback, and a scope that must close

nlohmann silently overwrites, so the parser callback is the one place a key can be seen **before**
the overwrite. Keys are tracked in a stack of sets, pushed on `object_start` and popped on
`object_end`.

**The callback always returns `true`.** Returning `false` tells nlohmann to *discard* the element,
which would drop data silently instead of diagnosing it.

The message and its **15-character truncation** are both .NET's
(`ThrowHelper.Serialization.cs:361-385`) — the reference declines to echo an arbitrarily long key
into an exception. It appends three `.` characters, not an ellipsis.

## 5. A test that looked sufficient and was not

The obvious scope test is *"the same key in two different objects is not a duplicate"*:

```json
{"a":{"x":1},"b":{"x":2}}
```

**Measured, that passes even when the scope never closes** — the two `x` keys land in two different
leaked sets by accident. The shape that actually proves the pop happens is a root key reappearing
*after* a nested object:

```json
{"a":{"x":1},"x":2}
```

There the root's `x` would be looked up in the stale inner set. Mutation M6 (never popping) was
**not caught** until that row was added, and it is recorded here because the weaker test is the one
a reader would naturally write.

## 6. Evidence

Six mutations, all caught after M6's test was strengthened: the strip never running; the strip
running *unconditionally* (which would change the default path); dropping any comma rather than
only one before a closer; rewriting inside a string literal; never rejecting a duplicate; and never
closing the key scope.
