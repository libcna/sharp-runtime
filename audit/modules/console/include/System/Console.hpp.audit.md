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
