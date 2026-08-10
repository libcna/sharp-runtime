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
