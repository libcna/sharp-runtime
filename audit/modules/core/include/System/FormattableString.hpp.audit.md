# Audit: `modules/core/include/System/FormattableString.hpp`

## Metadata

- Audit status: AUDITED (155-line public header, fully read).
- Validation: dedicated `FormattableStringTests2.*` passed 11/11 in
  `SharpRuntimeTests_Core_Base`; complementary integration
  `FormattableStringTests.*:FormattableStringFactoryTests.*` passed 13/13 on
  2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-formattable-string-audit-probe.cpp` prints
  `second`, `{value}`, and `{1}` for three cases that require literal
  preservation, escaped-brace output, and a format error, respectively.

## Assessment

The class clearly documents its string-only, culture-agnostic adaptation and
implements safe argument-index bounds checks.  Its output method, however, is
not a composite-format parser: it repeatedly searches/replaces `{0}`, `{1}`
in the mutable result.  That reprocesses inserted text and cannot enforce the
grammar promised by a FormattableString composite format.

## Finding references

- **SR-AUD-015 (extended):** `ToString()` replaces each index in successive
  passes.  `FormattableString("{0}", {"{1}", "second"})` becomes `second`,
  so an argument's literal text is reinterpreted as a format item.  Escaped
  `"{{0}}"` becomes `"{value}"` instead of literal `"{0}"`, and an unresolved
  `"{1}"` with one argument remains literal instead of producing the .NET
  composite-format `FormatException`.  The independent probe confirms all
  three results.  This is the same bespoke-parser family as the confirmed
  `String::Format` brace defect, not merely a culture adaptation.

## Other missing assertions and diagnostics

- Direct and integration tests cover only simple sequential `{n}` items; they
  omit escaped braces, repeated/nonsequential items, missing indices,
  alignment/format components, malformed braces, and an argument whose text
  contains brace syntax.
- `getArgumentCountProperty()` narrows vector size to signed `intcs` with no
  representable-size diagnostic.
- The provider methods intentionally ignore providers, but no non-null provider
  test documents this boundary.

## Final assessment

Storage and normal substitution work, but composite-format grammar and
argument isolation are confirmed incorrect.  No source or test was modified
during this audit.

---

## Post-audit remediation record — ticket #1883 (2026-07-30), SR-AUD-015 FormattableString half

The historical text above is retained unchanged. This section is appended, per
the SR-AUD-081 / SR-AUD-362 convention.

**SR-AUD-015's `FormattableString` half is remediated for its compatible
portion. The finding stays `confirmed`**, because its headline claims — escaped
braces and malformed closing braces — are the approval-gated remainder, ticket
**#1884**.

### What was confirmed, and one correction

Measured against the shipped header on 2026-07-30
(`build-probe/1881_family_compare_probe.cpp`, log `1881_prefix.log`), all three
of this report's claims reproduce exactly:
`FormattableString("{0}", {"{1}", "second"}).ToString()` returned `"second"`,
`"{{0}}"` with one argument returned `"{value}"`, and an unresolved `"{1}"`
stayed literal instead of raising `FormatException`.

**Correction to the stated cause.** This report attributes the escaped-brace
result to brace handling. There was no escape handling in the engine at all:
`"{{0}}"` simply *contains* a literal `{0}`, which was substituted, leaving the
outer braces. The genuinely serious sibling — and the one this ticket
repairs — is the first case, where the engine re-read text it had itself
inserted. The mechanism was the **outer** loop: the inner `while` advanced `pos`
past each insertion, so an argument naming *its own* index was already safe
(`FormattableString("{0}", {"{0}"})` returned `"{0}"` correctly), but the outer
`for i` restarted at `pos = 0` for the next index, so index 1 rewrote what index
0 had inserted. Two adjacent cases, one correct by accident and one silently
wrong, from one loop.

### The repair

`ToString()` is now **one left-to-right pass over the format string** appending
to a separate output, so substituted text is never re-examined. A resolvable
item is `"{"` digits `"}"` naming a stored argument; anything else stays
literal, exactly as before. The index accumulator is capped so a long digit run
(`"{99999999999999999999}"`) cannot overflow.

The **accepted grammar is deliberately unchanged**, which is the whole
compatible/gated boundary for this file: `{{`/`}}` are still not escapes, a
stray `}` is still literal, a missing index is still literal rather than a
`FormatException`, and `{N,width}` / `{N:spec}` are still not recognised as
items. Every one of those four is a *currently-succeeding* call whose result
would change, so each is deferred to #1884 with an exact before/after row in
`docs/CompositeFormatBoundaryPlan.md` §20.1.

### What changed observably

**Exactly one row of the eight-case matrix**, which is the section 9.3 invariant
of the family plan stated as evidence: `FS("{0}", {"{1}", "second"})` now
returns `"{1}"` instead of `"second"`. The other seven — `"{{0}}"` → `"{value}"`,
`"{1}"` → `"{1}"`, `"value}"` → `"value}"`, `"{0}"` with argument `"{0}"` →
`"{0}"`, `"{0,6}"` → `"{0,6}"`, `"{0:D4}"` → `"{0:D4}"`, `"{1}{0}"` → `"BA"` —
are byte-identical before and after (`1881_prefix.log` versus
`build-probe/1883_postfix.log`).

The report's other bullets are **not** closed by this ticket and are unchanged:
`getArgumentCountProperty()`'s unsigned→signed narrowing has no representable-size
diagnostic, and the provider methods still ignore their provider by documented
design (a permanent culture deviation, not a defect).

### Evidence

- **20 permanent add-only regressions** in `FormattableStringTests.cpp`
  (`FormattableStringBoundaryTests`). All 11 pre-existing `FormattableStringTests2`
  cases and the 13 integration `FormattableStringTests` cases pass **unmodified**.
- Whole repository **14,568 tests across 37 executables**, from 14,548 after
  #1882 and 14,514 at the batch baseline; build clean with zero errors and zero
  warnings.
- **Mutation-checked**: restoring the per-index find/replace sweep fails **5**
  permanent tests — both argument-isolation cases, the whole-format argument
  case, and the two that prove `ToString(provider)` and the `Invariant`/
  `CurrentCulture` statics reach the same body. The header was rebuilt and the
  suite re-executed for the mutated run.
- **ASan + UBSan + LeakSanitizer**: `build-probe/1883_formattable_stress_probe.cpp`
  covers 29 format shapes × 0..4 arguments drawn at run-time-derived offsets from
  a 15-entry argument table, plus copy and move of a live object, a
  100 000-character format with an item at the very last byte, 20 000 consecutive
  items, and an unterminated item at the end of a 100 000-character buffer.
  Result: 243 095 output bytes, 290 bounds exceptions, **0 escaped**, exit 0,
  zero diagnostics and zero leaks (`build-probe/1883_postfix_asan.log`).
  **Freshness is structural here**: `FormattableString.hpp` is header-only, so
  compiling the probe with the sanitizer recompiles the changed code itself and
  no archive can be stale.
- **TSan: not applicable**, recorded rather than skipped — `ToString` is a pure
  function of `format_` and `args_`, with no shared mutable state, atomic, lock
  or cache.

### Source, ABI, layout and `noexcept` consequences

**None.** `ToString()` keeps its signature, its `virtual`, its `[[nodiscard]]`
and its vtable slot; `ToString(const IFormatProvider*)`, `Invariant` and
`CurrentCulture` are untouched; `format_` and `args_` keep their types and order.
Because the body is inline in a public header, a downstream consumer must
**recompile** to pick up the fix — the ordinary consequence of any inline change,
identical in kind to #1867/#1868/#1870, and not an ABI break.

### Performance

The replaced sweep was O(arguments × format length) with a full `find` pass per
argument and a whole-buffer copy per match. The single pass is O(format length +
total argument length) with one `reserve`.
