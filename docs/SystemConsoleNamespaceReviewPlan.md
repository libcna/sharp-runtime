<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Console` namespace review and remediation plan

*Ticket #2162. Opened 2026-08-09 on branch `claude/remediation-batch-1804-namespace-b1yjh5`, after
`modules/security-cryptography` was fully reconciled. Audit numbering is **frozen at 364** — this
review creates no `SR-AUD-*` identifier. Everything below was measured here; the `/rv` reference
tree is absent, and where that limits a claim the limit is stated instead of guessed past.*

---

## 1. Selection, re-measured

`modules/console` was the previous handoff's named alternative: **2 open findings, both medium,
both expected fully compatible.** Re-parsing `audit/AUDIT_FINDINGS_INDEX.md` after this batch's
security-cryptography closure confirms it — and the reason to take it now is what makes it unusual
among the remaining units:

- **Both findings carry a direct managed comparison inside the audit itself.** SR-AUD-243's probe
  recorded .NET as `color=exception:System.ArgumentException`; SR-AUD-244's recorded
  `cursor=exception:System.ArgumentOutOfRangeException`. The target behaviour is **measured**, not
  recalled, which matters because `/rv` is absent.
- **Neither is a documented reduction.** The header openly documents its ANSI, local-cache and
  keyboard reductions; the audit's own assessment separates these two out as *entry-boundary
  validation failures* rather than reductions.
- **The whole namespace is 622 header lines, 100 body lines and one component with no dependants
  inside the repository beyond `Core.Base`.** A full close is realistic in one pass, and this batch
  had context left after closing security-cryptography.

Larger unreviewed units (`core` 72, `globalization` 7, `time-zone` 7) each fail on at least one of:
not being a namespace, needing `/rv` plus ICU or tz data, or having zero high-severity findings
alongside heavy parity dependence.

---

## 2. Scope and surface inventory

**In scope:** `modules/console` — 7 public headers (`Console.hpp` and six enum/event types),
1 body, 3 test files (`SharpRuntimeTests_Console`, **123 tests** at the start of this review).
Component `Console`, public dependency `Core.Base`.

| Area | Surface | State |
|---|---|---|
| Output | `Write`/`WriteLine` over `std::string`, `const char*`, `char`, `intcs`, `longcs`, `uintcs`, `ulongcs`, `double`, `float`, `bool`, `vector<char>` (+ index/count) | working; `const char*` null and the `vector<char>` range were repaired by earlier tickets |
| Error stream | `Error`-prefixed writes | working |
| Input | `Read`, `ReadLine`, `ReadKey` | `ReadKey` is a documented stub |
| Redirection | `getIsInput/Output/ErrorRedirectedProperty` | real `isatty`; Emscripten reports redirected |
| Colour | `get/setForeground/BackgroundColorProperty`, `ResetColor` | **SR-AUD-243** |
| Cursor | `SetCursorPosition`, `GetCursorPosition`, `get/setCursorLeft/TopProperty`, `Clear` | **SR-AUD-244**; position is a documented local cache |
| Cursor appearance | `get/setCursorSizeProperty`, `get/setCursorVisibleProperty` | size stored, never applied |
| Window | `getWindowWidth/HeightProperty` (real `ioctl`/`GetConsoleScreenBufferInfo`), `Left`/`Top` = 0, `SetWindowSize`, `SetWindowPosition` | escape sequences, unvalidated |
| Buffer | width/height aliased to the window; setters and `SetBufferSize`/`MoveBufferArea` are documented no-ops | unvalidated |
| Keyboard state | `getCapsLock/NumberLockProperty` | documented `false` |
| Cancel | `ConsoleCancelEventArgs`, `removeCancelKeyPressHandler` | present |
| Encoding | — | **absent by design**: no `InputEncoding`/`OutputEncoding`; the port writes bytes |
| Beep, Title | `Beep()`, `Beep(freq, dur)` (documented BEL fallback), title via OSC | working |

**Not in scope:** `System::IO`'s console streams, and the `Text` encoding stack.

---

## 3. Confirmed finding inventory — measured current behaviour

`build-probe/2162_probe1_console.cpp`, log `2162_probe1_console_before.log`.

| ID | Sev | Audit claim | Measured here |
|---|---|---|---|
| SR-AUD-243 | med | a cast `ConsoleColor` 99 is stored and emitted as ANSI 181; .NET throws `ArgumentException` | **Confirmed, and wider.** 16 → `ESC[98m`, 99 → `ESC[181m`, −1 → `ESC[29m`, and the getter afterwards reads back the invalid value |
| SR-AUD-244 | med | `SetCursorPosition(-1, 0)` caches −1 and emits an invalid sequence; .NET throws `ArgumentOutOfRangeException` | **Confirmed.** `(-1,0)`, `(0,-1)` and both property aliases with −5 are all accepted and cached |

---

## 4. Corrections and extensions — measured, not inferred

### 4.1 `ConsoleColor` `INT_MIN` truncates the escape sequence rather than merely mis-colouring

The audit's worst case is "emits ANSI 181", a wrong but well-formed sequence. `ansiColor` formats
into `char buf[12]`. At `static_cast<ConsoleColor>(INT_MIN)` the probe emits

```
ESC[-21474836
```

— **truncated mid-number, with no terminating `m`**. `snprintf` is not a buffer overflow, but the
result is an *unterminated* escape sequence, and a terminal that receives one consumes the
characters that follow it looking for the terminator. So the consequence of an out-of-domain colour
is not only a wrong colour: it can silently swallow subsequent program output.

GCC says the same thing statically. Compiling the header with `-Wall -Wextra` reports

```
warning: '%d' directive output may be truncated writing between 1 and 11 bytes into a region of size 10
note: directive argument in the range [-2147483618, 37]
```

The repository build does not surface this today because no translation unit in it instantiates the
setter with a non-constant argument. The buffer was sized for the 0–15 domain the type actually
has; validating the domain is what makes the size correct rather than lucky.

### 4.2 The cursor and window escape sequences go through `std::printf`, not `std::cout`

`SetCursorPosition`, `SetWindowSize` and `SetWindowPosition` use `std::printf`; every other output
member uses `std::cout`. Redirecting `std::cout`'s stream buffer — the technique this component's
own tests use — therefore does **not** capture them, which is why the probe's cursor rows show an
empty capture rather than the escape the audit reports. Ordering is safe (`sync_with_stdio` is on
by default), so this is a **testability** observation and not a defect; it is recorded because it
determines what the regressions for SR-AUD-244 can and cannot assert, and it is *not* changed here.

### 4.3 Six adjacent doors accept the same shape of invalid input — and are **not** repaired here

Measured, all accepted silently: `setCursorSizeProperty` with 0, −1 and 101 (the value is stored,
and the getter reads back 101 against a documented 1–100 domain); `SetWindowSize(-1,-1)` and
`(0,0)`; `SetWindowPosition(-1,-1)`; `SetBufferSize(-1,-1)`; `setBufferWidthProperty(-1)`;
`MoveBufferArea(-1,-1,-1,-1,-1,-1)`.

.NET validates all of these. **But the audit has no managed probe for any of them, and `/rv` is
absent**, so the exact exception type, parameter name and message would be recollection. This batch
does not implement behaviour from recollection: they become **#2166, a deferred-verification
ticket**, and the measured current behaviour is **pinned by test** so the question cannot be
answered silently later.

The two findings are different: the audit's own probe recorded .NET's answer for each of them.

---

## 5. Root causes

### CN-A — a public enum parameter is trusted as if the language enforced its domain (SR-AUD-243)

A scoped enumeration's *type* is not its *value set*: any `intcs` can be cast into `ConsoleColor`
and every arm of this component treats the result as valid. The value is stored, formatted, read
back and — at the extremes — formatted into a buffer that cannot hold it.

### CN-B — a coordinate is validated by the terminal, which never sees an invalid one (SR-AUD-244)

`SetCursorPosition` caches then emits. Nothing rejects a negative, and because the cache is local
(a documented reduction), the invalid value survives in the process even where the terminal ignores
the sequence. Both property aliases inherit it by delegation.

---

## 6. Compatible / deferred matrix

| Cause | Finding | Ticket | Class |
|---|---|---|---|
| CN-A | SR-AUD-243 | **#2163** | **compatible** |
| CN-B | SR-AUD-244 | **#2164** | **compatible** |
| — | contract, pins, reconciliation | **#2165** | **compatible** |
| — | six adjacent doors (§4.3) | **#2166** | **deferred verification** — needs `/rv` or a managed probe |

Nothing here is approval-gated, and nothing changes a layout, a signature, or a vtable: both repairs
add validation at the top of existing bodies.

---

## 7. The deliberate behavioural break

Calls that used to succeed will now throw. That **is** the finding, and it is the same class of
deliberate, documented break as ticket #2148's `CompressionMode` rejection. Recorded here so it is
not discovered as a surprise:

- `setForegroundColorProperty` / `setBackgroundColorProperty` with a value outside 0–15 →
  `ArgumentException`.
- `SetCursorPosition`, `setCursorLeftProperty`, `setCursorTopProperty` with a negative coordinate →
  `ArgumentOutOfRangeException`.

**Message and parameter-name choice, stated rather than assumed.** The audit measured the exception
*types*; it did not record .NET's message text, and `/rv` is absent. The colour message therefore
follows this repository's own established precedent for an enum-domain rejection —
`ArgumentException("Enum value was out of legal range.", "value")`, as settled by #1954, #1992 and
#2148 — and the cursor rejection uses the existing
`ArgumentOutOfRangeException::ThrowIfNegative` helper, whose text is the repository's standard.
Both are recorded as **type-verified, text-unverified**, and the text question rides along on #2166.

**Upper bounds are deliberately not added.** .NET is believed to reject a coordinate at or above
`short.MaxValue`, but that is recollection: the probe measured `SetCursorPosition(32767, 0)` and
`(INT_MAX, 0)` as accepted here, and both are **pinned as-is** rather than changed. #2166 owns it.

---

## 8. Test matrix

Colour: every valid enumerator 0–15 accepted and emitting its current sequence unchanged; 16, 99,
−1, `INT_MIN` and `INT_MAX` rejected on both setters; the getter unchanged after a rejected set;
`ResetColor` unaffected; the exception type and parameter name.

Cursor: `(0,0)` and other valid positions accepted and cached; `(-1,0)`, `(0,-1)`, `(-1,-1)`,
`(INT_MIN,0)` rejected; both property aliases rejected the same way; the cache unchanged after a
rejected set; `Clear()` still resets to `(0,0)`.

Pins (behaviour deliberately unchanged): `SetCursorPosition(32767, 0)` and `(INT_MAX, 0)` still
accepted; `setCursorSizeProperty(101)` still stored; `SetWindowSize`/`SetWindowPosition`/
`SetBufferSize`/`setBufferWidthProperty`/`MoveBufferArea` still accept negatives.

Invariance: the escape sequence emitted for every valid colour is byte-identical before and after.

---

## 9. Sanitizer matrix

| Tool | Used for | Discriminating? |
|---|---|---|
| ASan | the `ansiColor` buffer under the full argument range | yes |
| UBSan | the enum-to-int conversion and the `30 + c` / `90 + (c - 8)` arithmetic, which at `INT_MIN` is **signed overflow** | **yes — this is the sharp one** |
| LSan | no ownership changes | weak |
| TSan | — | **no**: this component's state is process-global `static inline` with no documented thread-safety contract, so TSan is not a discriminating tool and is not claimed |

---

## 10. Exclusions

1. The six adjacent doors (§4.3) — #2166.
2. Cursor upper bounds (§7) — #2166.
3. The `printf`/`cout` split (§4.2) — a testability observation, not a defect.
4. `GetCursorPosition`'s local cache, the `ReadKey` stub, `CapsLock`/`NumberLock`, the buffer
   no-ops and the `Beep` fallback — all **documented reductions**, which the audit's own assessment
   separates from these findings.
5. Encoding: `InputEncoding`/`OutputEncoding` do not exist in this port. Adding them is a public
   surface addition, not a repair, and the `System::Text` encoding stack has its own open
   design tickets (#2013–#2021).
6. The real-TTY / Windows / Emscripten platform matrix the `Console.cpp` audit asks for: it needs a
   pseudo-terminal fixture and two toolchains this environment does not have.

---

## 11. Completion criteria

SR-AUD-243 and SR-AUD-244 both `remediated`; every §8 row a permanent test; the emitted sequence for
every valid colour byte-identical; zero warnings, zero errors, two jobs maximum; graph, seams and
fixtures unchanged.

---

## 12. Implementation record — #2163 and #2164

*Filled in on completion.*

## 13. Implementation record — #2165, and the namespace reconciliation

*Filled in on completion.*
