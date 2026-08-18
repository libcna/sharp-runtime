<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — seven `Console` doors now reject invalid arguments (ticket #2166)

*2026-08-18.* `SetCursorPosition`, `CursorSize`, `SetWindowSize`, `SetWindowPosition`,
`SetBufferSize`, the two buffer-extent setters and both `MoveBufferArea` overloads now raise
`ArgumentOutOfRangeException` for arguments they used to accept silently.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `SetCursorPosition(32767, 0)` | accepted | `ArgumentOutOfRangeException("left")` |
| `SetCursorPosition(INT_MAX, 0)` | accepted | rejected |
| `SetCursorPosition(32766, 32766)` | accepted | **accepted** — the bound is exclusive |
| `setCursorSizeProperty(0)`, `(-1)`, `(101)` | stored and read back | `ArgumentOutOfRangeException("value")` |
| `SetWindowSize(-1,-1)`, `(0,0)` | accepted | rejected |
| `SetWindowPosition(-1,-1)` | accepted | rejected |
| `SetBufferSize(-1,-1)`, `(0,1)`, `(32767,1)` | accepted | rejected |
| `setBufferWidthProperty(-1)`, `setBufferHeightProperty(-1)` | accepted | rejected |
| `MoveBufferArea(-1,…)` — any of the six | accepted | rejected, each naming itself |
| any valid argument | — | **unchanged** |

## 2. The cursor bound is not a belief about a platform layer

#2165 pinned all of this as *"behaviour this review deliberately did NOT change, because .NET's
answer for it is recollection rather than measurement"*. The reference supplies the measurement,
and it corrects the framing on the most important row:

```csharp
public static void SetCursorPosition(int left, int top)
{
    // Basic argument validation.  The PAL implementation may provide further validation.
    if (left < 0 || left >= short.MaxValue)
        throw new ArgumentOutOfRangeException(nameof(left), left, SR.ArgumentOutOfRange_ConsoleBufferBoundaries);
    if (top < 0 || top >= short.MaxValue)
        throw new ArgumentOutOfRangeException(nameof(top), top, SR.ArgumentOutOfRange_ConsoleBufferBoundaries);
    ConsolePal.SetCursorPosition(left, top);
}
```
*(`Console.cs:550-559`.)*

That is in `Console.cs` itself, **before** dispatch, so it applies on every platform. The
comparison is `>=`, so **32766 is the last accepted column and 32767 is the first rejected one** —
the off-by-one that is easy to get wrong, and both sides of it are pinned.

## 3. The other six are a different case, and the difference is stated rather than glossed

**.NET's Unix pal throws `PlatformNotSupportedException` for every one of them**: `CursorSize`'s
setter (`ConsolePal.Unix.cs:193-197`), `SetWindowSize` (`:387-394`), `SetWindowPosition`
(`:744-747`), `SetBufferSize` (`:727-730`), the buffer setters (`:305-315`) and `MoveBufferArea`
(`:717-725`). So on this port's runtime platform .NET states **no range for them at all**.

The ranges adopted are .NET's **Windows** pal's, because they are the only ones .NET defines:

| Door | Check | .NET |
|---|---|---|
| `CursorSize` | `[1, 100]` | `ConsolePal.Windows.cs:588-590` |
| `SetWindowSize` | both `> 0` | `:1020-1021` |
| `SetWindowPosition` | both `>= 0` | `:995-1001` |
| `SetBufferSize`, buffer setters | `[1, short.MaxValue)` | `:897-901` |
| `MoveBufferArea` | all six `[0, short.MaxValue)` | `:735-751` |

**Refusing outright — copying .NET's Unix answer — would remove a feature this port offers rather
than repair one**, which is the wrong direction for a validation ticket. `SetWindowSize` and
`SetWindowPosition` really do emit xterm escape sequences here.

## 4. What is deliberately not reproduced

Several of .NET's Windows checks are **buffer-relative**: `SetBufferSize`'s lower bound is the
current window's right edge, `SetWindowPosition`'s upper bound is the buffer's width, and
`MoveBufferArea`'s bounds are the buffer's dimensions. This port has no buffer geometry to compare
against, so it enforces the half of each check that needs none — the sign and the `short.MaxValue`
ceiling. That is a subset of .NET's rejection, never a superset: nothing .NET accepts is refused.

## 5. The exception identity

Every rejection is `ArgumentOutOfRangeException` with .NET's parameter name, the offending value
as the actual value (so the composed message carries `Actual value was N.`) and .NET's exact
resource text:

* `ArgumentOutOfRange_ConsoleBufferBoundaries` — *"The value must be greater than or equal to zero
  and less than the console's buffer size in that dimension."*
* `ArgumentOutOfRange_CursorSize` — *"The cursor size is invalid. It must be a percentage between
  1 and 100."*
* `ArgumentOutOfRange_ConsoleWindowPos` — *"The window position must be set such that the current
  window size fits within the console's buffer, and the numbers must not be negative."*
* `ArgumentOutOfRange_ConsoleBufferLessThanWindowSize` — *"The console buffer size must not be less
  than the current size and position of the console window, nor greater than or equal to
  short.MaxValue."*

This is what #2163 and #2164 could not do: their exception *types* were probe-verified and their
*texts* were not, and the ticket recorded that the texts rode along on this one.

## 6. To migrate

Clamp before calling, or catch. A rejected `CursorSize` is **not stored**, so a caller that reads
the property back gets its previous value rather than the invalid one.

## 7. Evidence

| Mutation | Caught |
|---|---|
| The cursor ceiling becomes inclusive (the off-by-one) | ✅ |
| The cursor size upper bound is dropped | ✅ |
| Store the cursor size **before** validating | ✅ |
| `MoveBufferArea` checks only its first argument | ✅ |
| `SetWindowSize` accepts zero (both parameters) | ✅ |
| The buffer extent lower bound becomes 0 instead of 1 | ✅ |

Two mutations were invalid as first written and were reformulated rather than counted: one was a
no-op (`(void)0;` before an unchanged store) and one was rejected by `-Werror` as an unused
parameter. A third was **masked** — changing only `SetWindowSize`'s `width` check left `height`
rejecting `(0,0)` anyway — so it had to change both.

## 8. Downstream

Neither `cna` nor `mobile-eggbert` calls any of these doors — zero sites in both.
