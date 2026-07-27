# Audit: `modules/core/include/System/ArgumentNullException.hpp`

## Metadata

- Audit status: AUDITED (77-line public declaration, fully read).
- Validation: the three-fixture argument-exception filter passed 64/64 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- ASan/UBSan reproducer: `/tmp/sharp-runtimervc-argumentexception-audit-probe.cpp`
  calls the public `const char*` constructor with a null pointer.  UBSan reports
  a null argument to `char_traits::length` and ASan reports a null-read SEGV in
  `makeMsg`.
- Reference: local .NET `ArgumentNullException.cs` exposes
  `ArgumentNullException(string? paramName)` and therefore permits a null
  parameter name.

## Assessment

The declared inheritance and pointer guard are appropriate C++ counterparts,
but the overload accepting `const char*` presents a nullable native pointer
without a null contract.  Its implementation also duplicates a formatted
parameter suffix before forwarding to the base constructor.

## SR-AUD-089 — high — ArgumentNullException null C-string parameter crashes during message construction

`ArgumentNullException(const char* paramName)` accepts a raw C-string but
forwards it to `makeMsg`, which concatenates it before the base constructor can
apply its null fallback.  The explicit public call
`ArgumentNullException(static_cast<const char*>(nullptr))` reaches
`std::char_traits<char>::length(nullptr)`; UBSan diagnoses the invalid null
argument and ASan confirms a read from address zero.

The local .NET constructor accepts a nullable parameter name and produces a
valid exception with no parameter suffix.  A C++ raw-pointer overload must
either handle null deterministically or reject it with a system diagnostic
before string construction.

## SR-AUD-090 — low — ArgumentNullException duplicates its parameter marker in `what()`

The parameter-name constructors compose `"Value cannot be null. (Parameter
'item')"` and then call the two-argument `ArgumentException` constructor,
which appends the same suffix again.  The direct probe prints `Value cannot be
null. (Parameter 'item') (Parameter 'item')`, while the local .NET base call
formats the marker once.  The parameter property and exception type remain
correct, but logs and user diagnostics are observably malformed.

## Other missing assertions and diagnostics

- The pointer guard tests only `int*`; they omit const/volatile pointers,
  `void*`, derived/base conversion, null parameter-name inputs, and the
  message/property pair after a null-name construction.
- No declaration states C-string ownership/encoding or whether a null
  `paramName` is valid.  The `std::string` overload cannot represent that state.
- Tests do not count the parameter marker, assert exact default diagnostics,
  or exercise copy/move and inner-exception lifetime.

## Final assessment

The normal non-null path passes, but one public raw-pointer constructor is
ASan-confirmed unsafe and its ordinary message has a duplicate marker.  No
source or test was modified during this audit.

## Post-audit remediation for SR-AUD-089 and SR-AUD-090 (ticket #1776, 2026-07-27): REMEDIATED

The audit evidence above is retained unchanged. Ticket #1776
(`REMED-CORE-ARGNULL-MESSAGE`, P2, size XS) closed both findings on branch
`feature/remediation-argument-null-message`.

This ticket was opened during ticket #1775 and its own notes described the
duplicate-suffix defect as a fresh, post-audit discovery with no covering
`SR-AUD-*` identifier. That description was inaccurate: SR-AUD-089 and
SR-AUD-090, recorded above, already covered exactly this file and exactly
this defect pair (the null-pointer crash and the doubled marker) as
`confirmed` findings within the frozen SR-AUD-001..364 range. This report
records the correction rather than silently rewriting the ticket's history;
`NEXT.md`, `plan.md`, and `audit/AUDIT_FINAL_REPORT.md` carry the same
correction.

Root cause: `ArgumentNullException(paramName)`'s private `makeMsg()` helper
concatenated the raw `const char*`/`std::string` parameter name into an
already-composed `"Value cannot be null. (Parameter 'x')"` string and passed
that composed text to the `ArgumentException(message, paramName)` base
constructor, whose own `appendParamName()` appended the identical suffix a
second time -- and, because `makeMsg()` performed unchecked C-string
concatenation before the base constructor's own null guard could run, a null
`paramName` reached `std::char_traits<char>::length(nullptr)` first.

Repair: the paramName-only constructors now pass the raw, unsuffixed default
message straight to `ArgumentException(message, paramName)`, exactly matching
.NET's own `ArgumentNullException(string? paramName) : base(SR.ArgumentNull_Generic,
paramName)`. This makes the base constructor the single site that both
appends the parameter suffix (once) and null-guards the C-string overload,
which resolves SR-AUD-090 directly and resolves SR-AUD-089 as a natural
consequence of removing the unsafe local concatenation, not as a separate
change. `getParamNameProperty()`, HResult (`E_POINTER`), the
`(paramName, message)` and `(message, innerException)` overloads, and sibling
`ArgumentException`/`ArgumentOutOfRangeException` behavior are all unchanged;
no public signature, virtual member, or inheritance changed.

Closure evidence: 20 new permanent regressions in `ArgumentNullExceptionTests.cpp`
(exact message per constructor overload, single-occurrence suffix counts, empty
and punctuated parameter names, copy/move, catch-through `ArgumentNullException&`/
`ArgumentException&`/`System::Exception&`, and a direct null-`const char*`
non-crash regression for SR-AUD-089), plus 3 new regressions each in
`ArgumentExceptionTests.cpp` and `ArgumentOutOfRangeExceptionTests.cpp` pinning
that those sibling types were never affected; `SharpRuntimeTests_Core_Base`
4,972/4,972 and `SharpRuntimeTests_Collections_Core` 1,732/1,732; the two
pre-existing exact-message workarounds this defect forced
(`DictionaryKeyAndViewContractTests.cpp`'s `expectNullKeyRejected` from ticket
#1775, `LinkedListNodeLifetimeTests.cpp`'s `ExpectArgumentNullMessage` from
ticket #1769) now assert the single-suffix message directly; the `Core.Base`
standalone public-header consumer fixture (`test/consumer/core_base.cpp`)
extended to construct, throw, and catch an `ArgumentNullException` through
`System::Exception` and compiles/runs under `-Werror`; and a
`scripts/local_ci_check.sh build` full-repository gate. This is a pure message-
composition fix with no allocation, ownership, or string-lifetime change, so a
dedicated sanitizer campaign was not run beyond the existing focused-suite
coverage.
