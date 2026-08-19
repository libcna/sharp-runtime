<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `OSArchitecture` reports the OS on Windows, and an unknown target is a build error (ticket #1983)

*2026-08-19.* The Windows branch returned `getProcessArchitectureProperty()` directly, so a
32-bit process on 64-bit Windows — WOW64 — reported **X86** as the *operating system's*
architecture. And `getProcessArchitectureProperty()` **fabricated `X64`** for a compilation target
it did not recognise.

Landed under `docs/StandingApprovals.md` **SA-5**. No signature, layout or vtable change; no
behaviour change on Linux, macOS or Emscripten.

---

## 1. Two of the ticket's three absences are gone

#1983 was *"BLOCKED on three independent absences, **all** of which must be resolved before any
code is written"*:

| Absence | Now |
|---|---|
| no Windows toolchain in this environment | **gone** — `x86_64-w64-mingw32-g++` is installed |
| no `/rv/tmp/runtime/src/libraries/` to confirm the `IsWow64Process2` mapping | **gone** |
| no mixed-bitness Windows host to observe the difference | **remains** |

The third gates *runtime observation*, not implementation — which is exactly the position #2378
was in, and this ticket takes its evidence pattern: cross-compile the Windows arm, prove by symbol
inspection that it is confined, and state the runtime limit rather than implying it away.

## 2. The Windows arm, transcribed

`RuntimeInformation.Windows.cs:34-113` is a two-step probe, and both steps are here:

1. **`IsWow64Process2`**, resolved at run time from `kernel32` because it exists only on Windows
   10 and later. Its `nativeMachine` out-parameter is an `IMAGE_FILE_MACHINE_*` constant. If the
   call itself fails, .NET falls back to `ProcessArchitecture` — and so does this.
2. Otherwise **`GetNativeSystemInfo`**, whose `wProcessorArchitecture` uses the
   `PROCESSOR_ARCHITECTURE_*` constants — **a different enumeration**, which is why there are two
   mapping tables and not one.

Two asymmetries are transcribed rather than tidied: the machine-constant default falls back to
`ProcessArchitecture`, while the processor-architecture default is `X86`.

## 3. An unknown target is now a build error, as it is in .NET

`getProcessArchitectureProperty()` ended in `return Architecture::X64;`. A build for an
unsupported architecture compiled cleanly and then reported x64 to every caller — the worst of the
three possible outcomes.

.NET refuses at **compile** time and offers nothing else: `ProcessArchitecture`'s chain of
`#if TARGET_*` ends in `#error Unknown Architecture` (`RuntimeInformation.cs:49-50`). There is no
runtime fallback because there is no correct runtime answer — the property is a statement about
the compilation target, and if the target is unknown the *build* is what is wrong.

## 4. Evidence, and what it cannot reach

| Mutation | Result |
|---|---|
| the Windows arm reverts to `ProcessArchitecture` | **caught** — §4.1 |
| the `#error` is replaced by a fabricated `X64` | **caught** — §4.2 |
| **the `IMAGE_FILE_MACHINE` default invents `X64` instead of falling back** | **NOT caught** — §4.3 |

**4.1** The Windows object imports `GetModuleHandleW`, `GetProcAddress` and `GetNativeSystemInfo`
and contains the `"IsWow64Process2"` string; the POSIX object imports **none** of them, contains
no such string, and still calls `uname`. Under the mutation the Windows imports drop to **0** and
the string to **0**. So symbol inspection discriminates whether the OS query exists at all.

**4.2** A probe lifts the preprocessor chain verbatim and compiles it with the recognised target
macros suppressed. The baseline is **rejected** (`#error` fires); the mutation **compiles**. The
system headers cannot be compiled with `__x86_64__` suppressed, which is why the chain is lifted
rather than the file recompiled.

**4.3 is the residual absence, honestly restated.** Changing one of the mapping arms alters no
symbol and no string, so symbol inspection passes it. The arm's **behaviour** is unverifiable
without executing it, and this is precisely the third of the three absences #1983 listed — the
only one that has not gone away. The mapping tables are transcribed constant by constant for that
reason, and the note is at the site.

Two of the four mutation attempts were **invalid as first written** — a bad splice past a closing
brace, and a verdict I printed backwards — and were reformulated rather than counted.

## 5. Downstream

`cna` and `mobile-eggbert` reference `RuntimeInformation` in **zero** code sites. Nothing changes
on any platform this project's CI runs.
