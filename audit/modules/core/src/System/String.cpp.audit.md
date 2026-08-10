# Audit: `modules/core/src/System/String.cpp`

## Metadata

- Audit status: AUDITED (893 lines, full read).
- Direct probe compiled against `build/libsharp_runtime_core.a` on 2026-07-25:
  `g++ -std=c++20 -Imodules/core/include /tmp/sharp_runtimervc_string_audit_probe.cpp build/libsharp_runtime_core.a -o /tmp/sharp_runtimervc_string_audit_probe`.
- The current `StringTests` suite has no escaped-brace or substring-spill
  `LastIndexOf` assertion; the probe exposes both paths below.

## Assessment

The file has broad static utility coverage and several carefully hardened range
checks, including unsigned `ToCharArray` bounds validation.  Existing code also
contains good regressions for `Split` empty input, single-argument `Substring`,
large format indices, and backward search start-index fixups.  The bespoke
composite formatter and substring range implementation remain high-risk because
they reconstruct framework parser/range semantics manually.

## SR-AUD-015 — medium — `String::Format` rejects valid escaped braces and accepts malformed closing braces

`replaceArg` substitutes a format item before brace escaping is parsed, while
`FinalizeFormat` rejects every remaining `{` but never examines a `}`.  The
result is both directions of incorrect behavior:

- `String::Format("{{0}}", 42)` throws `FormatException` instead of returning
  the literal `{0}`;
- `String::Format("value}", 42)` returns `value}` instead of reporting malformed
  composite formatting.

The direct probe printed `escaped_exception=...` followed by `stray=value}`.
Microsoft’s composite-format specification requires `{{` and `}}` to produce
literal braces and treats malformed format strings as errors:
<https://learn.microsoft.com/en-us/dotnet/standard/base-types/composite-formatting#escaping-braces>.

### Required post-audit verification

Replace the sequential replacement parser with a single-pass parser that
distinguishes escaped braces, format items, alignment/specifier syntax, and
malformed closing braces.  Add exact tests for `{{0}}`, `{{{0:D}}}`, literal
`}}`, unmatched `{`/`}`, and out-of-range indices.  Preserve the current
multi-digit-index regression.

## SR-AUD-016 — medium — substring `LastIndexOf` can return a match that extends past the requested search end

The four-argument string overload calls `std::string::rfind(substr, startIndex)`
and validates only the match **start** against the lower range bound.  C++
`rfind` treats `startIndex` as the latest permitted start, not the final
character of a bounded .NET search range.  Consequently the probe reports:

```
last_index=3
```

for `String::LastIndexOf("abcde", "de", 3, 4)`.  The requested range is
indices 0 through 3, but the alleged match consumes index 4.  The .NET contract
defines `count` positions ending at `startIndex` as the searched section:
<https://learn.microsoft.com/en-us/dotnet/api/system.string.lastindexof?view=net-10.0>.

### Required post-audit verification

Require `matchStart + substr.length() <= startIndex + 1` as well as the lower
bound, using overflow-safe range arithmetic.  Add both a spilling negative
case (`"abcde", "de", 3, 4 -> -1`) and a same-range positive case.  Test empty
value and `startIndex == Length` behavior separately because this port has an
explicit compatibility fixup there.

## Other missing assertions and diagnostics

- `fmtInt`/`fmtDouble` use `std::stoi` for specifier precision without a
  controlled format-error translation; oversized/non-numeric precision can
  leak `std::invalid_argument`/`std::out_of_range` instead of
  `System::FormatException`.
- Replacement, trim, case, comparison, and indexing operate over UTF-8 bytes
  or C locale classification.  Their relationship to .NET UTF-16 and culture
  semantics needs explicit documented adaptation tests; ASCII-only checks are
  insufficient evidence.
- Size totals in `Concat`/`Join` have no overflow diagnostic before `reserve`;
  this is untested and relevant only near allocation limits.

## Final assessment

Useful broad helpers with two confirmed public formatting/search range defects.
The next repair phase should prioritize a parser-based formatter and bounded
substring search tests rather than adding more happy-path overload assertions.

---

## Post-audit remediation record — ticket #1882 (2026-07-30), SR-AUD-015 String half

The historical text above is retained unchanged. This section is appended, per
the SR-AUD-081 / SR-AUD-362 convention.

**SR-AUD-015's `String::Format` half is remediated. The finding stays
`confirmed`** until ticket #1883 closes its `FormattableString` half, because
one identifier covers both files.

### What the finding understated

The audit describes a grammar-fidelity problem. Measured against the shipped
library on 2026-07-30 (`build-probe/1881_family_compare_probe.cpp` and
`build-probe/1881_extra_probe.cpp`; logs `1881_prefix.log`,
`1881_ubsan_prefix.log`, `1881_extra_prefix.log`), the same root cause also
produced:

- **Non-termination from an ordinary public call.** `replaceArg` reset its scan
  cursor to `0` after every substitution (`String.cpp:122` as it then stood), so
  it re-read text it had just inserted. `String::Format("{0}", "{0}")` and
  `Format("[{0}]", "x{0}y")` never returned. This required no malformed format
  string and no attacker-controlled format: `Format("{0}", userText)` hung
  whenever `userText` contained `{0}`.
- **A second, unrelated non-termination.** `fmtInt`'s decimal padding was
  `while ((int)s.size() < width) s = "0" + s;` — one whole-string copy per
  padding character — so `Format("{0:D999999999}", 7)` was ~10^18 byte copies.
- **UBSan-confirmed undefined behaviour.** `std::abs(std::stoi(...))` on
  `INT_MIN`: `String.cpp:149:41: runtime error: negation of -2147483648 cannot
  be represented in type 'int'`, reachable from
  `Format("{0:D-2147483648}", 42)`.
- **Silent corruption in a second shape the audit did not name.** `extractSpec`
  returned the **first** occurrence's specifier for every occurrence of an
  index, so `Format("{0:X}/{0:D3}", 255)` returned `"FF/FF"` and
  `Format("{0:D3}/{0:X}", 255)` returned `"255/255"`.
- **A spurious failure on valid input.** `Format("{0}{1}", "{1}", "{0}")` threw
  `FormatException` for an index the call never used, because the two arguments
  rewrote each other.

The severity as filed (`medium`) understates a reachable public-input hang.

### The repair

The audit's "required post-audit verification" asked for exactly this: "replace
the sequential replacement parser with a single-pass parser". Ticket #1882 did
so. `extractSpec`, `replaceArg` and `FinalizeFormat` are gone; `formatCore`
walks the format string **once** and appends to a separate output, so inserted
argument text is never re-examined — the property that makes .NET's own
`AppendFormatHelper` immune to all of the above by construction. All **22**
`String::Format` overloads route through it, and so therefore do the 11
`StringBuilder::AppendFormat` and 11 `Console::Write`/`WriteLine` wrappers.

`fmtInt`/`fmtDouble` gained a bounded, non-throwing specifier parse that keeps
`std::stoi`'s prefix semantics (so every specifier that formatted successfully
still produces identical text, including the `std::abs`-derived `"{0:D-3}"` →
`"007"`) while adopting the reference's own digit bound
(`Number.Formatting.Common.cs:93-105`): the accumulator is checked **before**
each multiply against `100'000'000`, so the accepted range is the reference's
`0..999'999'999` and not a stricter one this port would be inventing. Padding is
one construction rather than a prepend loop, and an allocation failure inside a
specifier is translated to `System::OutOfMemoryException` — .NET's own outcome —
rather than letting `std::bad_alloc` escape and reintroduce the very defect
class this ticket removes.

The **accepted grammar is deliberately unchanged**: `{{`/`}}` are still not
escapes, a stray `}` is still literal text, and `{N,width}` still parses without
padding. Those four rows change what currently-succeeding calls return and are
gated on explicit user approval as ticket **#1884**
(`docs/CompositeFormatBoundaryPlan.md` §20).

### What did change observably

Only defects. Termination; `"{1}X"` and `"{1}|Y"` instead of `"XX"` and
`"Y|Y"`; `"FF/255"` instead of `"FF/FF"`; `"{1}{0}"` instead of a spurious
throw; `System::FormatException("Format specifier was invalid.")` — the
reference's `SR.Argument_BadFormatSpecifier` — where `std::out_of_range` used to
escape; a defined value where `std::invalid_argument` used to escape and where
`std::abs` was undefined; and one factually wrong message corrected, an unclosed
item now reporting `"Input string was not in a correct format."` instead of the
index-out-of-range text. **No exception type changed on any path.**

The two "Other missing assertions" bullets about `std::stoi` above are therefore
closed by this ticket. The UTF-8/culture bullet is **not** — that is CCF-015's
subject (SR-AUD-048) — and neither is the `Concat`/`Join` overflow bullet.
SR-AUD-016 (`LastIndexOf`) is untouched and remains `confirmed`.

### Evidence

- **34 permanent add-only regressions**: 30 in `StringTests.cpp`
  (`StringFormatBoundaryTests`) and 4 in `StringBuilderTests.cpp` pinning the
  wrapper. Every one of the 38 pre-existing `String::Format` tests passes
  **unmodified**, including `Format_TwoDigitIndexDoesNotAliasSingleDigitIndex`
  and `Format_UnclosedBrace_Throws`.
- `SharpRuntimeTests_Core_Base` 5,385/5,385; whole repository **14,548 tests
  across 37 executables**, from 14,514; build clean with zero errors and zero
  warnings.
- **Mutation-checked, three ways**, each rebuilt and re-executed (object file
  verified newer than the mutated source before every run):
  1. re-parsing the rendered argument text — `SelfReferentialArgument_Terminates`
     dumps core on infinite recursion;
  2. accepting an oversized specifier instead of rejecting it — **5** permanent
     tests fail;
  3. one bounded round of the replaced engine's cross-argument reinterpretation —
     **5** permanent tests fail, including both argument-isolation tests.
- **UBSan**: `String.cpp:149` is silent after the repair over the whole case
  matrix. The probe is compiled **with** `-fsanitize=undefined` **together with
  `String.cpp` itself**, so the changed code is instrumented directly and no
  stale archive is involved.
- **ASan + UBSan + LeakSanitizer**: `build-probe/1882_format_stress_probe.cpp`
  drives **3,675** cases — 51 format shapes × 13 argument shapes across the 1-,
  2-, 3- and 4-argument overloads, plus numeric overloads at `INTCS_MIN`,
  `INTCS_MAX`, `LONGCS_MIN`, `±DBL_MAX` and `1/3` — with every format string and
  every operand built at **run time** from a table and from `argv`, so constant
  folding cannot suppress a diagnostic. Included: empty format, lone `{`, lone
  `}`, format ending mid-item, a 100 000-character format with an item at the
  very last byte, 20 000 consecutive items, and an unterminated item at the end
  of a 100 000-character buffer. Result: **2,759 returned a value, 916 threw a
  `System` exception, 0 escaped**, exit 0, zero AddressSanitizer,
  UndefinedBehaviorSanitizer and LeakSanitizer reports
  (`build-probe/1882_postfix_asan.log`). That "0 escaped" is load-bearing: the
  probe catches only `System::Exception`, so a leaked `std::` exception would
  have terminated the process.
- **TSan: not applicable**, recorded rather than skipped — the formatter has no
  shared mutable state, atomic, lock or cache, and is a pure function of its
  arguments.

### Source, ABI, layout and `noexcept` consequences

**None.** All 22 public `Format` signatures are unchanged; no `noexcept`
specification, virtual function, vtable slot, calling convention or data member
changed; `String` has no data members. The change is confined to one `.cpp`, so
no consumer recompilation is even required.

### Performance

The replaced engine copied the whole buffer once per argument, restarted `find`
from position 0 after every substitution, and re-scanned the format once per
argument in `extractSpec`; a 2-argument call performed at least four full scans
and two full string copies. `formatCore` is one scan with one `reserve` and one
append per segment.

### SR-AUD-015 fully remediated — ticket #1884 (2026-07-31)

Approved by the batch instruction in the exact words of
`docs/RemainingApprovalDecisions.md` §C.8 item (2), which points at
`docs/CompositeFormatBoundaryPlan.md` §20.7. The approval-gated half of this
finding — the half that *is* its headline — has landed: `{{` and `}}` are escapes
producing one literal brace, an unescaped `}` outside a format item is a
`FormatException`, an index with no argument is a `FormatException` in
`FormattableString` too, and the `,alignment` component pads the substituted
text. All fourteen rows of §20.1, measured before and after against a
`git worktree` checkout of the pre-change tree
(`build-probe/1884_{prefix,postfix}_plain.log`, 36 cases).

**The result the finding never named.** One shared scanner,
`System::detail::runCompositeFormat`, transcribed from
`ValueStringBuilder.AppendFormat.cs`'s `AppendFormatHelper`, now drives **both**
engines. Before #1884 they disagreed with each other, not only with .NET:
`"{{0}}"` was a `FormatException` from `String::Format` and `"{v}"` from
`FormattableString`; a missing index threw in one and stayed literal in the
other; `"{0,6}"` consumed the alignment in one and emitted it verbatim in the
other. A test pins that they can no longer diverge.

**Premise correction.** §20.1's row 5 (`"value}"`) is right that it now throws,
but its reason is not `Format_UnexpectedClosingBrace`: a **trailing** `}` makes
.NET's `MoveNext` step past the end of the string first, so the reference reports
`Format_UnclosedFormatItem`. The port matches .NET exactly and the test pins both
spellings rather than treating them as interchangeable.

**Deliberately not adopted.** .NET also accepts whitespace inside a format item
(`"{0 }"`, `"{0, 6}"`); the port still rejects it. That is a *widening* of the
accepted grammar, no row of §20.1 asks for it, and §20.7 authorises only those
rows — recorded in the plan §21.4 rather than smuggled in.

+22 permanent tests, including the flip of all seven `PinsCurrentGrammar_*` and
edge tests written to be flipped by this ticket, and one in `modules/text`
pinning that `StringBuilder::AppendFormat` inherits the grammar transitively.
No signature, `noexcept`, virtual, vtable, data member or layout change. ASan and
UBSan clean over all 36 probe cases with `String.cpp` compiled into the probe.
**`SR-AUD-015 → remediated`; CCF-012 is complete** (#1881 design, #1882, #1883,
#1884).

---

## Remediation record — ticket #2224 (SR-AUD-016), 2026-08-10

**`remediated`.** Original evidence retained unchanged.

Both substring overloads now search from `startIndex + 1 - substr.size()` — the last position at
which a match still *fits* inside the section — through one file-local `lastIndexOfBounded` helper,
and the size comparison is made in `size_t` so a substring longer than the section cannot produce a
negative intermediate.

**Premise correction, measured.** The finding named only the four-argument overload. The
**three-argument** overload (`String.cpp:592`) carried the identical unchecked
`rfind(substr, startIndex)` and failed the same way: `LastIndexOf("abcde", "de", 3)` returned 3.
A third shape the finding did not name also failed — `LastIndexOf("abcde", "abcde", 2, 3)` returned
**0**, a match overrunning `startIndex` by three positions, because the lower-bound check
(`pos >= begin`, `begin == 0`) passed it. Three failing cases, not one.

The single-character overloads were verified **unaffected** and left alone: a one-character match
cannot overrun its own start.

**Evidence.** `build-probe/2223_probe1_before.log` → `_after.log`: the SR-AUD-016 group goes 3 wrong
→ 0 with all five positive controls unchanged, including the documented `startIndex == Length`
off-by-one fixup and the empty-substring return. **+7 permanent regressions** in
`modules/core/tests/System/TextInputBoundaryTests.cpp`.

No signature, `noexcept`, layout, vtable or ABI change.
