# Audit: `modules/core/include/System/TryWriteInterpolatedStringHandler.hpp`

## Metadata

- AUDITED: 123-line inline manual interpolation handler, fully read.
- Validation: `TryWriteInterpolatedStringHandlerTests.*` passed 13/13 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reproduction: the format probe prints `bool=1`, `hex=255`, and
  `double=3.140000`; the ASan null-destination probe reaches a null write in
  `appendRaw` and exits 134.
- Reference basis: local .NET `System/MemoryExtensions.cs:5684-5710,6055-6250`.

## SR-AUD-132 — high — public raw destination pointer can be null and reaches an ASan-confirmed write crash

The two constructors accept arbitrary `char*` plus a positive length without
validation. `TryWriteInterpolatedStringHandler(nullptr, 1).AppendLiteral("x")`
passes the capacity check and reaches `std::memcpy(dest_ + pos_, ...)`; ASan
reports a zero-page write and the isolated probe aborts with exit 134. The
counterpart accepts a `Span<char>`, which cannot represent this public
nonempty-null destination state.

The C-string literal overload independently forwards a null `value` to
`std::strlen`, so both raw-pointer entry boundaries rely on native undefined
behavior rather than a documented failure result. Represent destination as the
project Span abstraction or reject invalid pointer/length combinations before
any pointer arithmetic, and define the null-literal policy.

## SR-AUD-133 — medium — AppendFormatted ignores format and replaces .NET formatting with hardcoded C++ spellings

The explicit format overload discards its format string, while the unformatted
route routes all arithmetic through `std::to_string` and every unsupported
type to `"[?]"`; it never performs the documented IFormattable fallback.
The standalone probe therefore emits `1` for `true`, `255` for `255` with
`"X2"`, and `3.140000` for `3.14`. Current .NET formats those normal cases as
`True`, `FF`, and the general `3.14` respectively, and its handler honors
IFormattable/ISpanFormattable, provider, alignment, and custom formatter
paths.

This makes interpolated text observably wrong even when capacity succeeds.
Implement format/provider-aware established project formatting and retain a
failure result for a short destination, or reduce/rename the surface as a
strictly documented primitive C++ formatter rather than the .NET handler.

## Other missing assertions and diagnostics

- Missing null destination/literal, zero-length, overlapping source,
  embedded-NUL, exact-boundary, and failure-prefix preservation vectors.
- Missing format, alignment, provider, enum, custom type/IFormattable,
  ISpanFormattable, string-view, and Unicode formatting coverage.
- The class is a manually used ordinary C++ object, not a compiler-generated
  `ref struct`; no diagnostic prevents copying, escaping, or calling it without
  the .NET `MemoryExtensions.TryWrite` completion boundary.
- `pos_ + len` is unchecked `size_t` arithmetic and no test exercises hostile
  length arithmetic or exception propagation from string/format creation.

## Final assessment

Ordinary literal accumulation works, but raw-pointer safety and formatting
semantics have the confirmed SR-AUD-132/133 gaps. No source or test was
modified during this audit.

## Post-audit remediation for SR-AUD-132 (ticket #1810, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-133 is untouched and
stays `confirmed`** — this ticket repaired the raw-pointer boundaries only, not
the formatting semantics. That finding asks for format/provider-aware formatting
or an explicit renaming of the surface, which is a design decision about what this
type is, not a safety repair.

Ticket #1810 (`REMED-CORE-INTERPOLATED-HANDLER-NULL-DEST`, P1, size S) validates
both raw-pointer boundaries the finding named, and settles the null-literal policy
its closing sentence asked for. Measured before any production change, one process
per case (`build-probe/1810_prefix_defects.cpp`, log
`build-probe/1810_prefix_defects.log`):

| # | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `handler(nullptr, 1).AppendLiteral("x")` | UBSan *null pointer passed as argument 1, which is declared to never be null* at line 108, **ASan SEGV on 0x0**, exit 1 | `ArgumentNullException` — *(Parameter 'destination')* |
| 2 | the four-argument constructor, same input | same | same |
| 3 | `handler(nullptr, 0).AppendLiteral("x")` | refused: `appended=0 written=0 success=0` | **byte-identical** |
| 4 | `AppendLiteral((const char*)nullptr)` | same UBSan text at line 64, **ASan SEGV** in `strlen`, exit 1 | `ArgumentNullException` — *(Parameter 'value')* |
| 5 | `handler(nullptr, 1).getString()` | `string=''` | rejected at construction |
| 6 | fill the buffer, then append one more | `appended=0 written=8 success=0` | **byte-identical** |
| 7 | ordinary use | `written=4 success=1 string='x=42'` | **byte-identical** |

Case 1 is a **write**, not a read: the capacity check passed — `1 >= 1` — and
`std::memcpy(dest_ + pos_, ...)` then wrote to the zero page. That is the most
severe shape in this file and is why the ticket was taken ahead of the other
remaining public-input findings.

**What the .NET counterpart gets for free.** .NET's
`TryWriteInterpolatedStringHandler` takes a `Span<char>`, which cannot represent a
nonempty null destination at all, so there is no .NET validation to copy — the
check here restores by validation what the .NET type gets from its parameter
type. A null paired with a capacity of **zero** stays valid: it is this port's
spelling of an empty destination, case 3 shows it already behaved correctly, and
it is the rule tickets #1774 and #1805 settled for the same pointer/length shape.

**The null-literal policy is decided, not inherited.** In .NET this handler is
compiler-generated and `AppendLiteral` receives only literal text, so no .NET
behaviour applies. `AppendLiteral(const char*)` now throws
`ArgumentNullException("value")` rather than treating null as empty, because the
`std::string` overload cannot be null — `""` is already how an empty literal is
spelled — and the `bool` result already means "did it fit". Succeeding silently on
null would give that result a second meaning and hide the caller's bug. One
permanent test asserts `""` and `nullptr` behave differently, which is the
assertion that would fail if a later change collapsed them.

**Two further defects in the same members are closed by the same change** and are
disclosed rather than filed under noise, both from the audit report's own "Other
missing assertions and diagnostics" list:

- `appendRaw` tested capacity as `pos_ + len > destLen_`. Both operands are
  `size_t`, so the sum can wrap and let an oversized append pass the very check
  meant to stop it. It is now `len > destLen_ - pos_`, which cannot wrap because
  `pos_ <= destLen_` is an invariant — `pos_` only ever advances by a `len` this
  test has already accepted.
- `std::memcpy` is undefined for a null pointer even with a length of zero, and
  both ends can legitimately be null (`dest_` for a zero-capacity handler, `data`
  for an empty `std::string`), so `appendRaw` now returns early at `len == 0`.
  `getString()` had the same problem: it formed `std::string(dest_, pos_)`
  unconditionally, and `[nullptr, nullptr)` is not a valid range even at count
  zero.

Closure evidence: 12 new permanent regressions in
`TryWriteInterpolatedStringHandlerTests.cpp` — null destination, null destination
through the four-argument constructor (including that its `shouldAppend`
out-parameter is deliberately left unwritten, because an exception reports a
destination that does not exist while `shouldAppend = false` reports one that
exists and is too small), the destination parameter name, null with zero capacity
still valid, its `getString()`, null literal, the literal parameter name, the
empty-versus-null policy, an empty `std::string` literal, the exact capacity
boundary in both directions, a zero-capacity non-null destination, and the
unchanged valid path. `SharpRuntimeTests_Core_Base` **4,994/4,994** (was 4,982),
and the same 4,994 under AddressSanitizer + UndefinedBehaviorSanitizer +
LeakSanitizer with zero reports (`build-asan/1810_core_base_asan.log`). Repository
gate: 0 warnings, 0 errors, **13,970 tests across 37 executables** (was 13,958).
Doxygen 1,941 of the 1,942 ceiling, unchanged.

Source and ABI consequences: none. The class is header-only and gains one private
static helper; no public signature, object layout, vtable or exported symbol
changed. Behavioural note for consumers: a null destination with a positive
capacity, or a null C-string literal, now throws at the boundary instead of
crashing. Neither had any in-repository caller.

The report's remaining structural observation — that this is an ordinary C++
object rather than a compiler-generated `ref struct`, with no diagnostic
preventing it from being copied, escaping, or being used without the .NET
`MemoryExtensions.TryWrite` completion boundary — is **not** addressed here. It is
a design question about the shape of the type, and it belongs with SR-AUD-133.

## Post-audit remediation for SR-AUD-133 (review #2303, tickets #2304/#2305, 2026-08-11): REMEDIATED

The audit evidence above is retained unchanged. `docs/InterpolatedHandlerFormattingPlan.md`
is the design record; this note is the summary.

**One root cause.** `formatValue<T>` was a hand-written type→text map that did
not consult this repository's own formatters and **had no parameter for the
specifier at all**. The discarded format string and the hardcoded C++ spellings
are two faces of that single fact, so one delegation change closes both.

**The finding's three examples are all real, and the finding is understated in
four places.** Measured over a bounded 75-cell matrix before any production
change (the 75-cell matrix of `docs/InterpolatedHandlerFormattingPlan.md` §6; `build-probe/` is untracked, so the values are transcribed there):

| Input | Before | After |
|---|---|---|
| `true` | `'1'` | `'True'` |
| `255` with `"X2"` | `'255'` | `'FF'` |
| `3.14` | `'3.140000'` | `'3.14'` |
| **`"lit"` (a string literal)** | **`'[?]'`** | `'lit'` |
| `2.5f` | `'2.500000'` | `'2.5'` |
| `42` with `"Q"` | `'42'` | `FormatException` |
| a real `IFormattable` | **`'[?]'`** | its own text |

1. **A string literal lost its value entirely.** `T` deduces to `char[N]` for a
   literal, which never matched the `const char*` arm, so the most ordinary
   interpolation argument imaginable fell into the placeholder branch. Every
   other divergence renders the value *differently*; this one **discards it**.
   `std::string_view` and any `char[N]` behaved the same.
2. `float` diverges as well as `double`.
3. **Every unrecognised or malformed specifier was silently accepted** — the
   exact fallback CCF-006 (#1847/#1849) removed from all twelve numeric
   wrappers.
4. The advertised `IFormattable` fallback did not merely ignore the format: it
   produced `"[?]"`. The class doc-comment claiming that fallback was false.

**No formatting semantics were guessed.** `/rv` is absent and was not needed:
every arm now delegates to a public, tested, already-remediated sibling in the
same module and component — `Boolean::ToString`; `SByte`/`Int16`/`Int32`/`Int64`
and `Byte`/`UInt16`/`UInt32`/`UInt64::ToString`, chosen by width and signedness;
`Single`/`Double::ToString`; and the value's own `ToString(format)` (which is
`System::IFormattable`'s sole pure virtual) or `ToString()`. No format grammar
is written in this header, so the emitted text cannot be wrong unless the
sibling API is, and cannot drift from it later.
`System::detail::runCompositeFormat` was evaluated and correctly **rejected**:
it parses a *composite format string* into items, a problem this handler does
not have, because the value and the specifier arrive already split.

**Two boundaries are deliberately unchanged rather than guessed**, documented at
the point of decision: `long double` keeps `std::to_string` (this repository has
no extended-precision formatter, and narrowing to `double` would silently lose
precision), and `char16_t` stays on the integral path (whether that door means
"a character" or "a 16-bit integer" is not answerable from this type's own
contract). A type with no `ToString` at all still yields `"[?]"` — narrowed in
reach, not removed.

**Compatibility.** 35 of 75 cells changed; **40 are byte-identical**, including
every integer, `char`, `std::string`, a `const char*` lvalue, and — measured
separately — **all capacity and failure behaviour and all layout and trait
facts**. The only transition from a succeeding call to a throw is five
malformed specifiers, which is inherited from CCF-006's settled policy rather
than invented, and which is validated **before** any byte reaches `appendRaw`,
so a rejected specifier leaves `pos_` and `success_` exactly as they were and
the handler stays usable. Two tests pinned the defect and were replaced:
`AppendFormatted_Bool` asserted `"1"`, and `AppendFormatted_WithFormat_Ignored`
asserted only that the result was non-empty — an assertion that held equally
well whether or not the format was honoured, so it could never have detected
this repair. **There is no first-party production consumer**: changing the
header recompiled exactly one object file, its own test.

**Source and ABI consequences: none.** No public signature, overload set,
template constraint, member layout (`sizeof`/`alignof` 32/8), vtable, `noexcept`
specification, exported symbol or module dependency edge changed. The two new
concepts are used only inside `if constexpr`, never as constraints, so the set
of calls that compile is unchanged. The header's new transitive `__int128`
exposure through `Int64.hpp`/`Double.hpp` is disclosed in the plan §9.

Closure evidence: **+31 permanent tests (25 → 56)**, one per production door and
per boundary. Nine mutations, eight valid and **all eight caught**; the ninth is
proved equivalent over eleven wrappers rather than assumed. Two mutations
initially survived and are why two tests are stronger than they would otherwise
have been — `2.5` is exact in both binary precisions, so the float test uses
`0.1f`; every unformatted integer reads the same at any width, so the width test
pins `-1` in hexadecimal. Sanitizers were run **only where they discriminate**:
ASan is the sole thing that separates the bounded array read from
`std::string(v)`, and it caught that mutation. Repository gate: 0 errors, 0
warnings, **16,972 tests across 38 executables** (was 16,941), with the six
inherited failures unchanged (5 × `PingTests`, #1962; 1 × `SocketTests`, no
usable IPv6 in this environment).

**Cross-cutting: adjacency, not membership, and no `CCF-*` is minted or
extended.** This handler contains no brace grammar — it never sees a composite
format string — so it is not a third implementation of CCF-012's grammar, and
CCF-012 remains open and unchanged, closable only by #2020. It is not a numeric
wrapper, so it is not a CCF-006 member either; what it shared was that closed
cause's *shape*, which is now gone here by delegating to the cause's own
remediated members. The repair required no new policy, only the application of
two that already exist.

**A separate defect found in the same member — ordinary ticket #2305, no
`SR-AUD-*` identifier, numbering still frozen at 364.**
`AppendFormatted(const char*)` reached `return std::string(v);`, whose
NUL-terminated-array precondition a null violates. `AppendLiteral(const char*)`
has rejected null since #1810; this door did not. Measured
(transcribed in `docs/InterpolatedHandlerFormattingPlan.md` §7; `build-probe/` is untracked): libstdc++ diagnoses the
precondition itself and throws `std::logic_error`, which escapes this
`System`-shaped public API with nothing to catch it, so **the process aborts,
exit 134**. **ASan and UBSan report nothing** — and that silence was the
discriminating evidence, establishing that the mechanism is an escaping
non-`System` exception rather than a memory error. Fixed with
`ArgumentNullException("value")`, the same exception type and parameter name
#1810 settled for the sibling door.

The report's structural observation that this is an ordinary C++ object rather
than a compiler-generated `ref struct`, with no diagnostic preventing it from
being copied, escaping, or being used without the .NET `MemoryExtensions.TryWrite`
completion boundary, is **still not addressed**. It is a design question about
the shape of the type, not about its formatting semantics, and it is unaffected
by this ticket.
