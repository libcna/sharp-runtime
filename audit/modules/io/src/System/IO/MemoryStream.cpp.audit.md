# Audit: `modules/io/src/System/IO/MemoryStream.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-341 — high — MemoryStream raw-buffer constructor dereferences null input before it can report an argument error

`MemoryStream(const bytecs* buffer, intcs size, ...)` initializes the vector with `buffer, buffer + size` before validating either argument.  The ASan/UBSan direct probe `MemoryStream(nullptr, 1)` terminates with a null read in the vector range copy.  This bypasses the managed-style `ArgumentNullException` that the public raw-buffer constructor must provide and differs from the class's otherwise explicit Read/Write argument validation.

## Missing assertions and diagnostics

- Stream tests cover Read/Write invalid buffers but omit constructor null source, negative size, and source-lifetime/copy assertions (SR-AUD-341).
- No near-limit capacity/position diagnostic verifies allocation failures or raw vector-range preconditions.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.

## Post-audit remediation for SR-AUD-341 (ticket #1805, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged, including the "Missing assertions
and diagnostics" section, whose first bullet is this ticket's own charge sheet.

Ticket #1805 (`REMED-IO-MEMORYSTREAM-NULL-BUFFER`, P1, size S) moved the argument
checks ahead of the copy. `MemoryStream(const bytecs*, intcs, bool)` no longer
initializes `data_` from `buffer, buffer + size` directly; `data_` is initialized
from a file-local `validatedBufferCopy(buffer, size)` that validates first and
copies second, so no invalid input reaches pointer arithmetic or the vector range
constructor. A null `buffer` paired with a nonzero `size` throws
`ArgumentNullException("buffer")`; a negative `size` throws
`ArgumentOutOfRangeException("size", "Non-negative number required.")`. Null takes
precedence over a bad size, matching .NET's own ordering, in which
`ArgumentNullException.ThrowIfNull(buffer)` precedes the count check.

The ticket reproduced the finding before changing any production source, one
process per input so that a crash in one case could not hide another
(`build-probe/1805_prefix_defects.cpp`, log `build-probe/1805_prefix_defects.log`):

    case1 (nullptr, 1)  UBSan "load of null pointer of type 'const unsigned char'"
                        then AddressSanitizer SEGV on address 0x0, exit 1
    case2 (nullptr, 0)  constructed, length=0, exit 0
    case3 (data,   -1)  other-exception "cannot create std::vector larger than max_size()"
    case4 (data,    3)  constructed, length=3, exit 0

The identical source post-fix (`build-probe/1805_postfix_defects.log`) reads:

    case1  ArgumentNullException       "Value cannot be null. (Parameter 'buffer')"
    case2  constructed, length=0                          <- byte-identical to pre-fix
    case3  ArgumentOutOfRangeException "Non-negative number required. (Parameter 'size')"
    case4  constructed, length=3                          <- byte-identical to pre-fix

Case 3 records a second defect in the same constructor that the original finding
text did not name: a negative size was not merely unvalidated, it escaped as a raw
`std::length_error` — a standard-library exception crossing a public API whose
purpose is to mirror .NET's argument diagnostics — after first forming
`buffer + size`, which is out-of-bounds pointer arithmetic in its own right. It is
repaired by the same change and is recorded here rather than filed under noise.

A null pointer paired with a size of **zero** is deliberately still accepted, and
that decision is load-bearing rather than an omission. This port's parameter is a
pointer/length pair, not .NET's `byte[]` object: `(nullptr, 0)` is the ordinary
spelling of an empty range, `std::vector<bytecs>().data()` is permitted to return
null and does on this toolchain, and `BinaryData::ToStream()` reaches this
constructor exactly that way for empty content. Case 2 above shows the input was
already well defined and already produced a correct empty stream, so rejecting it
would have been a regression rather than a repair. This is the same rule ticket
#1774 settled for the identical pointer/length shape on `ICollection::CopyTo`.
`UnmanagedMemoryStream` diverges from both and rejects null unconditionally
(pinned by `UnmanagedMemoryStreamTests.NullPointer_ThrowsArgumentNullException`)
because it *retains* the caller's pointer, and .NET's own `UnmanagedMemoryStream`
rejects null unconditionally too.

Closure evidence: 13 new permanent regressions in `StreamTests.cpp` covering
null/positive, null/large, the parameter name in the message, null-before-negative
precedence, null/zero, null/zero still writable, `std::vector::data()` on an empty
vector, negative, an explicit assertion that `std::length_error` no longer escapes,
`INT32_MIN`, zero size with a non-null source, the unchanged valid path, and the
source-lifetime/copy independence the audit listed as missing; 1 new regression in
`BinaryDataTests.cpp` pinning the in-repository caller that makes the accepted
`(nullptr, 0)` rule load-bearing. `SharpRuntimeTests_IO` 541/541 (was 527), and the
same 541 under AddressSanitizer + UndefinedBehaviorSanitizer + LeakSanitizer with
zero reports, LeakSanitizer proved active by a bounded self-test
(`build-probe/1805_lsan_selftest.cpp`, which is reported as a 4,096-byte definite
leak). Repository gate `scripts/local_ci_check.sh build`: 0 warnings, 0 errors,
13,937 tests across 37 executables (was 13,923). Doxygen 1,941 of the 1,942
ceiling, unchanged. No public signature, object layout, vtable, or symbol changed,
so no consumer rebuild is required on this ticket's account.

The second bullet of "Missing assertions and diagnostics" — the absent near-limit
capacity/position diagnostic — is **not** closed by this ticket. Exercising it
needs a multi-gigabyte allocation, which is a different kind of test with a
different cost, and it is not part of SR-AUD-341's crash contract.
