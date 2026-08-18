<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — every `ArgIterator` member throws, as .NET's does (ticket #2276)

*2026-08-18.* Three `ArgIterator` members returned quietly where .NET reports an unsupported
platform, the exception type was wrong, one overload was missing, and one return type diverged.

Landed under `docs/StandingApprovals.md` **SA-9**, with **SA-10** for the `noexcept` drops and the
return-type change, under SA-2's five conditions. **This decreases the test count by 5** — nine
narrow cases replaced by four broader ones.

---

## 1. The open question, and how the reference answered it

The ticket asked whether the unreachable instance members should become **reachable**, become
**static**, or **stay as they are**.

They stay instance members, because .NET's are, and making them `static` would diverge from the
very shape this stub exists to present.

But `ArgIterator.cs:10-58` settled something the ticket had not asked: **every member of .NET's
portable `ArgIterator` throws `PlatformNotSupportedException`** — constructors, `End`, `Equals`,
`GetHashCode` and the rest alike.

## 2. What changed

| Member | Was | Is |
|---|---|---|
| `End()` | `noexcept`, **silent no-op** | throws `PlatformNotSupportedException` |
| `Equals(const ArgIterator&)` | `noexcept`, returned **`false`** | throws |
| `GetHashCode()` | `noexcept`, returned **`0`** | throws |
| both constructors, `GetNextArg()`, `GetRemainingCount()` | threw `NotSupportedException` | throw **`PlatformNotSupportedException`** |
| `GetNextArgType()` | returned `TypedReference` | returns **`RuntimeTypeHandle`** |
| `GetNextArg(RuntimeTypeHandle)` | **absent** | present, throws |
| the message | this port's own, one variant per member | **.NET's single sentence** |

The three quiet members are the important half: a caller who reached one received a **plausible
answer** where .NET reports an unsupported platform, which is the worse of the two failures.

## 3. The exception type change is narrower than it looks

`PlatformNotSupportedException` derives from `NotSupportedException`, so existing `catch` blocks
on the base still catch it. Only code that caught `NotSupportedException` **and inspected the
message** is affected — and the message changed too, from this port's invented per-member text to
.NET's single `SR.PlatformNotSupported_ArgIterator` sentence:

> ArgIterator is not supported on this platform.

A test asserts the **derived** type specifically, because a test that only caught the base would
pass either way — which is exactly how the old type survived this long.

## 4. To migrate

Nothing constructs an `ArgIterator` successfully, in this port or in .NET, so there is no working
code to migrate. If you caught `NotSupportedException` around one of the three formerly-quiet
members without expecting a throw, you now get one — which is the point.

## 5. The test-count decrease

The gate moves **17,283 → 17,278**. Nine narrow cases (one per member, plus two message pins) were
replaced by four that assert more: every door's exception, the derived type specifically, the
added overload, and the two return types. Nothing was disabled, weakened or skipped.

## 6. Downstream, measured

Neither `cna` nor `mobile-eggbert` mentions `ArgIterator` — **zero sites in both**. Neither
repository was modified.
