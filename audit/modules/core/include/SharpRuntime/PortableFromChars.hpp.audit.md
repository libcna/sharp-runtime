# Audit: `modules/core/include/SharpRuntime/PortableFromChars.hpp`

## Metadata

- AUDITED: 86-line portable floating `from_chars` wrapper, fully read.
- Validation: standalone C++20 probe calls the fallback with `["1", "2")`
  and prints `ptr_offset=2 ec=0 value=12` despite `last` at offset 1.
- Reference basis: C++ `std::from_chars(first, last, value)` range contract,
  the header's documented fallback rationale, and all three local call sites
  in Single, Double, and XPath.

## SR-AUD-180 — high — Apple fallback reads past the declared from_chars range and can parse a different value

`PortableFromCharsFloat` receives `first` and `last` but passes only `first`
to `strtof`/`strtod`; those C functions scan until NUL and have no `last`
bound.  The direct range probe exposes `"12"` as one-character input by
passing `last = text + 1`.  It returns success with value 12 and a pointer at
offset 2, while a real `from_chars` call may consume only `"1"`, return value
1, and stop at offset 1.

The header comments restrict current internal calls to a complete
NUL-terminated `std::string`, so those three call sites do not presently pass
a subrange.  However, the helper publicly advertises a drop-in
`from_chars(first,last,value)` shape and is selected exactly on Apple targets
where the standard overload is unavailable.  Any future subrange or
non-NUL-terminated caller can obtain a wrong value, pointer past `last`, or an
out-of-range read, so the fallback violates a memory/range contract on its
supported platform.

## Assessment

The requires-based feature detection and explicit correction of `strtof`'s
leading whitespace/plus-sign differences are well motivated.  The fallback
still needs to constrain the C parser to the supplied character range before
claiming the standard return contract.

## Other missing assertions and diagnostics

- No test forces the fallback path; Linux normally chooses native floating
  `std::from_chars`, masking every Apple deployment-target behavior.
- Add subrange, non-NUL buffer, empty, leading-plus/whitespace, overflow,
  partial-consumption, and special-token parity cases under a forced fallback
  build configuration.
- Current Single/Double/XPath tests exercise full `std::string` input only;
  retain an explicit API contract test for a caller whose `last` precedes the
  storage terminator.

## Final assessment

The Apple fallback has a confirmed high range-safety/correctness defect
(SR-AUD-180).  No source or test was modified.

---

## SR-AUD-180 — REMEDIATED (ticket #2221, 2026-08-10)

The original evidence above is retained unchanged. The family record is
`docs/CoreDefinedArithmeticBoundedParseFamilyPlan.md`. No `SR-AUD-*` identifier was issued;
numbering stays frozen at **364**. This finding is **not** a CCF-004 member and is not adjacent to
it: no arithmetic is involved. It is a memory-range contract defect and its repair shares nothing
with that family's.

**Three corrections to the finding's premises, every one measured.**

1. **SR-AUD-180 is NOT AddressSanitizer-decidable.** Placing `"12"` in a two-byte heap allocation
   with **no NUL anywhere in it** and calling the fallback over `[0,1)` forces `strtod` to read one
   past the allocation, and **ASan reports nothing at all** — the read happens inside glibc's own
   `strtod`, which is neither instrumented nor an ASan interceptor. A batch that trusted "this is an
   over-read, so ASan will show it" would have concluded the finding was already fixed.
   A **hardware guard page** does see it. `build-probe/2217_probe_guard.cpp` maps two pages,
   `mprotect`s the second `PROT_NONE`, and places `"12"` in the last two bytes of the readable page
   so that `last` is exactly the guard boundary:

   ```
   before, mode=std      -> survived  ec=0 ptr_offset=2 value=12
   before, mode=fallback -> Segmentation fault (exit 139)
   after,  mode=fallback -> survived  ec=0 ptr_offset=2 value=12
   ```

   That is the finding, its repair, and its verification, with no sanitizer involved.
2. **The header's own comment was false, and so is the report's conclusion that drew on it.** This
   report states that "the header comments restrict current internal calls to a complete
   NUL-terminated `std::string`, so those three call sites do not presently pass a subrange."
   Measured, `Single::tryParseCore` and `Double::tryParseCore` trim leading and trailing ASCII
   whitespace (ticket #1864) **before** forming `first`/`last`, so for `" 1.5 "` they hand the
   helper `first_offset=1`, `last_offset=4` over a five-character string — `last` is a **space**,
   not the terminator. Only the XPath caller matches the comment. Stated precisely rather than
   overstated: the trimmed-away characters are whitespace and the C parser stops at whitespace
   anyway, so **no in-repository input is known to produce a different value through
   `Single`/`Double`** — what changed is that the safety argument no longer rests on a false
   premise about the range, only on a property of the particular input.
3. **The consequence is a wrong *value*, not only a wrong pointer.** `"1e3"` restricted to `[0,1)`
   returned **1000** where a real `from_chars` returns 1, and `"-57"` restricted to `[0,2)`
   returned `-57` with a pointer three past a two-character range.

**What changed.** `[first, last)` is copied into a NUL-terminated local buffer, the C parser runs on
the copy, and the returned pointer is rebased into the caller's range. The copy is exactly
`length + 1` bytes — no multiplication and no amplification of a caller-controlled length — and uses
a 512-byte stack buffer for every realistic literal, falling back to a **nothrow** heap allocation
rather than truncating, because truncating a digit run changes the value.

`PortableFromCharsFloat` and `FromCharsFloat` **gain** `noexcept`. That is an added guarantee, not a
dropped one, so it needs no approval, and it is load-bearing: `std::from_chars` is itself `noexcept`
and `Single::tryParseCore`/`Single::TryParse` are too, so on the fallback platform a throwing helper
would have called `std::terminate`. The one non-standard status, `std::errc::not_enough_memory`, is
reachable only when a copy of the caller's own range cannot be allocated, and every caller here
treats any non-`errc{}` value as failure.

**The missing coverage this report asks for now exists, and on every platform.** The report says
"no test forces the fallback path; Linux normally chooses native floating `std::from_chars`". True
of `FromCharsFloat`, the dispatching wrapper — but not of `PortableFromCharsFloat`, which is an
ordinary public function template any platform can call directly. The new
`modules/core/tests/SharpRuntime/PortableFromCharsTests.cpp` calls it directly, so the Apple-only
fallback is exercised on the Linux gate: **23 tests**, covering empty, one character, shortest
valid, exact longest valid, one past the bound, malformed prefix and suffix, embedded NUL, leading
sign, trailing junk, all six whitespace characters, a 700-character digit run and the stack/heap
boundary at 509–513, all-zero and negative zero, maximum representable and one above, minimum
negative and one below, repeated/missing/unexpected delimiters, and every prefix length of one
backing literal. Acceptance, consumption, value and status are asserted **separately**, and wherever
the platform has a real `std::from_chars` the assertion is that the fallback **agrees with it**
rather than with a hand-written expectation.

**Closure evidence.** The guard-page probe survives in `fallback` mode with exactly
`std::from_chars`'s answer; all fourteen `P` cases of the family probe exit 0 under
`-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all` with leak detection on,
and each now returns the value and pointer the standard function returns. `PortableFromChars*`
23/23; the three consumers are unaffected (`Single*`/`Double*` 372/372, XPath 90/90).
**Four mutations, all killed**: dropping the terminator (11 of 23 tests fail), returning the copy's
pointer without rebasing (18 of 23 fail, and the guard probe reports a nonsense offset), parsing
`first` directly again — the original defect — (6 tests fail **and** the guard probe segfaults), and
truncating an over-long range instead of taking the heap path (1 test fails).

**Source, ABI and layout consequences.** Both entry points are free function templates, so no
mangled symbol exists to change. The only contract change is the **added** `noexcept`, pinned by
`static_assert`. No signature, parameter, return type or default argument changed. `<cstddef>`,
`<cstring>`, `<memory>` and `<new>` are now included.
