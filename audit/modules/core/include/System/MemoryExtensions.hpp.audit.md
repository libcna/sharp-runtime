# Audit: `modules/core/include/System/MemoryExtensions.hpp`

## Metadata

- Audit status: AUDITED (832-line header-only implementation, fully read).
- Validation: `MemoryExtensionsTests.*` passed 92/92 in
  `SharpRuntimeTests_Core_Base` on 2026-07-25.
- Independent probe: `/tmp/sharp-runtimervc-memoryextensions-audit-probe.cpp`,
  built with `-fsanitize=address,undefined -fno-omit-frame-pointer`.  LeakSanitizer
  is disabled only because the sandbox tracer cannot support it.

## Assessment

The vector/string slice checks use the recently corrected unsigned subtraction
form and the equality-oriented algorithms deliberately account for floating
NaN.  The header nevertheless has three new public-contract failures:
`CopyTo` neither checks destination capacity nor preserves a general
overlapping source, the order-sensitive algorithms use C++ relational operators
instead of .NET's comparison contract, and whitespace trimming is limited to
the active C locale's single bytes despite its stated .NET counterpart.

References: [.NET `MemoryExtensions.CopyTo` contract](https://learn.microsoft.com/en-us/dotnet/api/system.memoryextensions.copyto?view=net-10.0),
[current .NET `SequenceCompareTo` source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/MemoryExtensions.cs.html),
[current .NET `Sort` source](https://source.dot.net/System.Private.CoreLib/src/libraries/System.Private.CoreLib/src/System/MemoryExtensions.cs.html),
[`Single.CompareTo` NaN ordering](https://learn.microsoft.com/en-us/dotnet/api/system.single.compareto?view=net-10.0),
and [.NET Unicode whitespace definition](https://learn.microsoft.com/en-us/dotnet/api/system.char.iswhitespace?view=net-10.0).

## Finding references

### SR-AUD-046 — medium — order-sensitive MemoryExtensions methods use C++ operators instead of the .NET comparison contract

`SequenceCompareTo` (lines 325–332), `BinarySearch` (472–480), and default
`Sort` (446–447) use raw `==`/`<` rather than the `IComparable<T>.CompareTo`
contract documented by the managed counterparts.  This is observable for
`float`: the probe sorts `{3, NaN, 1}` to `{1, 3, NaN}`, returns `-1` when
searching that span for `NaN`, and returns zero for comparing `{NaN}` with
`{1}`.  .NET considers NaN less than a number and considers two NaNs equal for
`Single.CompareTo`; its sequence comparison explicitly uses `CompareTo`.

Besides returning incompatible results, passing a range containing NaN to
`std::sort` gives that algorithm a comparator which is not a strict weak
ordering.  The local custom-comparator overload is intentionally a C++
predicate adaptation; this finding covers the default .NET-named APIs.

### SR-AUD-047 — high — MemoryExtensions.CopyTo writes past a shorter destination

`CopyTo(ReadOnlySpan<T>, Span<T>)` immediately invokes `std::copy` at lines
426–428 without first comparing destination and source lengths.  The `Span`
member `CopyTo` does validate this public precondition, but the static helper
does not.  The probe copies two `int`s into a one-element vector and ASan
reports a heap-buffer-overflow in `MemoryExtensions::CopyTo<int>`.

The managed copy contract throws `ArgumentException` when the destination is
shorter.  This is a reachable memory-safety defect, not merely a differing
exception type.  Its same forward-copy implementation also independently
reproduces SR-AUD-044: copying three overlapping nontrivial `Cell` values one
place to the right changes `abcd` to `aaaa` instead of `aabc`.

### SR-AUD-048 — medium — character-span whitespace trim recognizes only C-locale byte whitespace

`TrimStart` and `TrimEnd` call `std::isspace(static_cast<unsigned char>(...))`
at lines 560–575.  They operate on the UTF-8 bytes of `std::string`, not
`SharpRuntime::charcs` Unicode code units, and `std::isspace` does not recognize
the two-byte UTF-8 encoding of U+00A0 in the default C locale.  The probe trims
`"\\xC2\\xA0value\\xC2\\xA0"` and reports length 9, retaining both no-break
spaces; .NET's Unicode whitespace set includes U+00A0, so the counterpart
returns `"value"`.

The same port-level UTF-8/Unicode-classification gap also appears in the
already-audited string trimming paths.  This report records the directly
reproduced `MemoryExtensions` surface; remediation should establish one
documented encoding and Unicode-whitespace policy rather than adding an
ASCII-only special case here.

## Required post-audit verification

Make static `CopyTo` delegate to the checked, overlap-safe span operation (or
perform the same length check and direction-aware copy).  Add a throwing short
destination test under ASan/UBSan and left/right overlap tests using `int`,
`std::string`, and an observable nontrivial type.  Keep SR-AUD-044's
`TryCopyTo` no-write rule intact.

Define a comparison adapter for ported primitive and comparable types, then
use it for `SequenceCompareTo`, `BinarySearch`, and default `Sort`.  Test NaN
ordering, NaN search, and a custom comparable type whose equality and ordering
cannot be inferred from C++ operators.

Decide whether character spans are UTF-8 bytes or managed UTF-16-like code
units.  If managed parity is intended, decode before Unicode classification
and test U+00A0, U+0085, U+2000, and U+3000.  If byte semantics are deliberate,
rename/document this API rather than claiming the .NET char-span counterpart.

## Other missing assertions and diagnostics

- The 92-test suite covers only an equal-length, non-overlapping integer copy;
  it has no short destination, overlap, or nontrivial-copy assertion.
- The suite tests NaN equality paths but no NaN order, `SequenceCompareTo`,
  default sorting, or binary search case.
- Whitespace tests use only ASCII spaces/tabs; no Unicode or locale-independence
  assertion identifies the encoding boundary.
- `Overlaps` compares raw pointers from unrelated allocations with relational
  operators.  The normal disjoint test happens to pass on this platform, but no
  test or diagnostic records the C++ object/provenance assumption needed for
  portable address-range comparison.
- The `size_t` to 32-bit `intcs` narrowing in full-vector and full-string
  `AsSpan` overloads remains an inherited SR-AUD-043 malformed-length risk for
  objects larger than `intcs::max()`.

## Final assessment

Normal algorithms pass their focused suite, but the static copy helper has an
ASan-confirmed out-of-bounds write, its default ordering is incompatible with
.NET NaN behavior, and character trimming loses Unicode whitespace.  No
implementation was modified during this audit.
