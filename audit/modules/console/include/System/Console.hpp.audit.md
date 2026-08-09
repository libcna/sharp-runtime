# Audit: `modules/console/include/System/Console.hpp`

## Metadata

- AUDITED: output/input overloads, formatting, colors, cursor/window/buffer
  APIs, keyboard/cancel stubs, ANSI mapping, and static state.
- Validation: direct native/current-.NET probes for invalid color/cursor input;
  complete Console fixture passed 123/123.

## SR-AUD-243 — medium — Console color setters accept values outside the ConsoleColor range

Both color setters assign any cast enum value and build an ANSI sequence from
it.  The native probe calling ForegroundColor 99 prints
`color_threw=0` and emits `ESC[181m`; current .NET prints
`color=exception:System.ArgumentException` because Unix ConsolePal rejects
values outside the 0–15 color range.  The invalid value also remains observable
through the C++ foreground/background getter.

## SR-AUD-244 — medium — negative cursor coordinates are stored and emitted rather than rejected

`SetCursorPosition(-1, 0)` assigns the local cache and emits `ESC[1;0H`.
The same native probe prints `cursor_threw=0 left=-1 top=0`, whereas current
.NET prints `cursor=exception:System.ArgumentOutOfRangeException` before the
platform PAL.  The setter/property aliases therefore permit a state that no
managed caller can create.

## Assessment

The header explicitly documents several pragmatic ANSI, local-cache, and
keyboard/buffer reductions, and ordinary test paths work.  SR-AUD-243/244 are
not documented reductions: both are entry-boundary validation failures with
direct managed behavioral comparisons.

## Other missing assertions and diagnostics

- Add invalid color and negative/short.MaxValue cursor regressions
  (SR-AUD-243/244) for direct and property setters.
- Capture every Write/WriteLine/Error overload, test locale/format edge cases,
  null C-string behavior, EOF input, ReadKey/intercept, cancel handlers,
  redirected streams, ANSI-disabled terminals, cursor cache drift, and all
  no-op buffer/window operations.

## Final assessment

SR-AUD-243 and SR-AUD-244 are directly reproduced. No source or test was
changed during this audit.

---

## Remediation record — tickets #2163, #2164 and #2165 (2026-08-09)

SR-AUD-243 and SR-AUD-244 both **remediated**. Repair, evidence and mutations:
`docs/SystemConsoleNamespaceReviewPlan.md` §12–13.

Both findings reproduce exactly as recorded, and both are repaired at the entry boundary this report
identified: the colour setters reject anything outside 0–15 with `ArgumentException` and the cursor
doors reject a negative coordinate with `ArgumentOutOfRangeException`, in each case **before** the
value is stored or emitted, so a rejected call leaves the getter, the cache and the terminal
untouched. The exception *types* come from this report's own managed probes; the message texts do
not, and are recorded as unverified under #2166.

**One consequence this report understates.** Its worst colour case is a wrong-but-well-formed
`ESC[181m`. `ansiColor` formats into `char buf[12]`, so at `static_cast<ConsoleColor>(INT_MIN)` the
emitted sequence was `ESC[-21474836` — **truncated mid-number with no terminating `m`**. A terminal
that receives an unterminated escape consumes the output that follows it, so an out-of-domain colour
could silently swallow later program output rather than merely mis-colour it. GCC reports the same
statically under `-Wall -Wextra`; the repository build does not surface it because no translation
unit instantiates the setter with a non-constant.

**One defect neither this report nor the finding names**, found by pinning rather than by repairing:
because no upper bound is enforced on a cursor coordinate, `INTCS_MAX` is reachable, and
`left + 1` — the 0-based-to-ANSI conversion — was **signed integer overflow** there. UBSan reported
it; the conversion is now computed in a wider type. No well-defined output changed. Fixed with **no**
new `SR-AUD-*` identifier; numbering stays frozen at 364.

**Deliberately not repaired, and pinned instead of left silent:** the six adjacent doors this report
does not name (`setCursorSizeProperty` outside 1–100, `SetWindowSize`, `SetWindowPosition`,
`SetBufferSize`, the buffer setters, `MoveBufferArea`) and the cursor upper bound. .NET is believed
to reject all of them, but no managed probe measured any of them and `/rv` is absent, so
implementing them would be recollection. Ticket **#2166** owns the question; three `PIN_` tests hold
the current behaviour so it cannot change silently.

**The assertions this report asked for are permanent** (+15 tests): invalid colour and negative
cursor regressions for both the direct and the property setters, `INT_MIN` and `INTCS_MAX` at both
ends, the exact exception types and parameter names, the getter and cache proved unchanged after a
rejected set, and all 32 valid colour sequences asserted byte-for-byte so the repair cannot move
one. The broader Write/WriteLine, EOF, `ReadKey`, redirected-stream and ANSI-disabled matrix this
report also lists is **not** delivered here and is not claimed.
