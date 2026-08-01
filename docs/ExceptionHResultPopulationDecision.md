<!-- SPDX-License-Identifier: MIT -->

# Exception HResult population decision (#1875)

## 1. Scope and authority

Ticket #1875 is the inactive post-audit sweep created by #1873 for the 45
`*Exception.hpp` types outside `modules/core/include/System/` that contained no
explicit `setHResultProperty` call.  The user confirmed in the 2026-08-01 batch
prompt that this no-approval-needed ticket was still wanted after #1927, #1928,
#1929 rows 5-6, and #1880.  This document records the independent comparison
required by the ticket; it issues no new `SR-AUD-*` identifier.

The reference snapshot is the official `dotnet/runtime` `main` commit
[`0eb5481340ea675857c7a7abf18f68a60b52a686`](https://github.com/dotnet/runtime/commit/0eb5481340ea675857c7a7abf18f68a60b52a686),
committed at `2026-08-01T02:32:34Z`.  The exact source files, the official test
files found for this population, the recursive tree response, and its commit
metadata are retained under `build-probe/1875-reference/`.  The numeric values
come from that snapshot's `src/libraries/Common/src/System/HResults.cs`.

## 2. Correction to the ticket's binary premise

The ticket expected every row to fall into one of two categories: a
type-specific assignment or deliberate inheritance.  Source inspection found
three categories instead:

- 12 types assign a type-specific constant in current .NET;
- 30 types make no assignment and inherit their reference base HResult;
- 3 types contain conditional propagation logic (`HttpRequestException`,
  `WebException`, and `WebSocketException`).

This correction does not overwrite #1875's historical text.  The two
inner-exception propagation mismatches are separable from the constant sweep
and are extracted to inactive ticket #1932.  `WebSocketException`'s native
error overloads are already explicitly outside the ported surface; the
represented enum/message constructors inherit `E_FAIL` once the shared
`Win32Exception` root is corrected.  Existing SR-AUD-250 (discarded WebSocket
inner cause) is not changed by this work.

## 3. Reference matrix

### 3.1 Type-specific assignments (12)

| Type | Current .NET assignment | Prefix port value | Remediation |
|---|---:|---:|---|
| `KeyNotFoundException` | `0x80131577` `COR_E_KEYNOTFOUND` | `0x80131501` | Assign in all 3 represented constructors. |
| `PathTooLongException` | `0x800700CE` `COR_E_PATHTOOLONG` | `0x80131620` | Assign in all 3 represented constructors. |
| `AmbiguousImplementationException` | `0x8013106A` `COR_E_AMBIGUOUSIMPLEMENTATION` | `0x80131501` | Assign in both represented constructors; do not alter the still-open hierarchy/API finding SR-AUD-158. |
| `ExternalException` | `0x80004005` `E_FAIL` | `0x80131501` | Assign in all 3 represented constructors; do not add the still-open error-code API from SR-AUD-159. |
| `VerificationException` | `0x8013150D` `COR_E_VERIFICATION` | `0x80131501` | Assign in both represented constructors; no new constructor or support classification. |
| `AbandonedMutexException` | `0x8013152D` `COR_E_ABANDONEDMUTEX` | `0x80131501` | Assign in all 6 represented constructors. |
| `SynchronizationLockException` | `0x80131518` `COR_E_SYNCHRONIZATIONLOCK` | `0x80131501` | Assign in all 3 represented constructors. |
| `ThreadAbortException` | `0x80131530` `COR_E_THREADABORTED` | `0x80131501` | Assign in both represented constructors; retain the documented unsupported-abort adaptation. |
| `ThreadInterruptedException` | `0x80131519` `COR_E_THREADINTERRUPTED` | `0x80131501` | Assign in all 3 represented constructors. |
| `ThreadStartException` | `0x80131525` `COR_E_THREADSTART` | `0x80131501` | Assign in all 3 exposed constructors; do not alter SR-AUD-196's accessibility finding. |
| `ThreadStateException` | `0x80131520` `COR_E_THREADSTATE` | `0x80131501` | Assign in all 3 represented constructors. |
| `WaitHandleCannotBeOpenedException` | `0x8013152C` `COR_E_WAITHANDLECANNOTBEOPENED` | `0x80131600` | Assign in all 3 represented constructors. |

### 3.2 Pure reference inheritance (30)

| Type | Reference base/result | Prefix comparison | Disposition |
|---|---:|---|---|
| `Win32Exception` | `ExternalException` / `0x80004005` | Port derives from `Exception`, yielding `0x80131500`. | Assign the inherited reference value in both represented constructors without changing the public base. |
| `UnreachableException` | `Exception` / `0x80131500` | Exact | Pin. |
| `CultureNotFoundException` | `ArgumentException` / `0x80070057` | Exact | Pin. |
| `ZLibException` | `IOException` / `0x80131620` | Exact | Pin. |
| `FileFormatException` | `FormatException` / `0x80131537` | Exact | Pin. |
| `InvalidDataException` | `SystemException` / `0x80131501` | Exact | Pin. |
| `HttpIOException` | `IOException` / `0x80131620` | Exact | Pin. |
| `HttpProtocolException` | `HttpIOException` / `0x80131620` | Exact | Pin. |
| `NetworkInformationException` | `Win32Exception` / `0x80004005` | Prefix `0x80131500`. | Corrected by the shared Win32 root. |
| `PingException` | `InvalidOperationException` / `0x80131509` | Exact | Pin. |
| `CookieException` | `FormatException` / `0x80131537` | Exact | Pin. |
| `ProtocolViolationException` | `InvalidOperationException` / `0x80131509` | Exact | Pin. |
| `SocketException` | `Win32Exception` / `0x80004005` | Prefix `0x80131500`. | Corrected by the shared Win32 root. |
| `AuthenticationTagMismatchException` | `CryptographicException` / `0x80131501` | Exact | Pin. |
| `CryptographicUnexpectedOperationException` | `CryptographicException` / `0x80131501` | Exact | Pin. |
| `AuthenticationException` | `SystemException` / `0x80131501` | Exact | Pin. |
| `InvalidCredentialException` | `AuthenticationException` / `0x80131501` | Exact | Pin. |
| `JsonException` | `Exception` / `0x80131500` | Exact | Pin. |
| `RegexMatchTimeoutException` | `TimeoutException` / `0x80131505` | Exact | Pin. |
| `RegexParseException` | `ArgumentException` / `0x80070057` | Exact | Pin. |
| `ChannelClosedException` | `InvalidOperationException` / `0x80131509` | Exact | Pin. |
| `TaskCanceledException` | `OperationCanceledException` / `0x8013153B` | Exact | Pin; do not alter SR-AUD-230's Task lifetime finding. |
| `TaskSchedulerException` | `Exception` / `0x80131500` | Exact | Pin. |
| `BarrierPostPhaseException` | `Exception` / `0x80131500` | Exact | Pin. |
| `LockRecursionException` | `Exception` / `0x80131500` | Exact | Pin. |
| `SemaphoreFullException` | `SystemException` / `0x80131501` | Exact | Pin. |
| `InvalidTimeZoneException` | `Exception` / `0x80131500` | Exact | Pin. |
| `TimeZoneNotFoundException` | `Exception` / `0x80131500` | Exact | Pin. |
| `UriFormatException` | `FormatException` / `0x80131537` | Exact | Pin. |
| `XmlSchemaValidationException` | `XmlSchemaException` / `0x80131941` | Exact | Pin. |

### 3.3 Conditional propagation (3)

| Type | Current .NET behavior | Port measurement | Decision |
|---|---|---|---|
| `HttpRequestException` | Ordinary constructors inherit `0x80131500`; an inner exception makes the outer HResult equal `inner.HResult`. | Ordinary control is exact. Runtime-generated `0x81234567` inner remains outer `0x80131500`. | Preserve control; extract propagation to #1932. |
| `WebException` | Ordinary constructors inherit `0x80131509`; an inner exception makes the outer HResult equal `inner.HResult`. | Ordinary control is exact. Runtime-generated `0x81234567` inner remains outer `0x80131509`. | Preserve control; extract propagation to #1932. |
| `WebSocketException` | Enum/message constructors inherit `E_FAIL`; represented native failures conditionally copy a negative HRESULT. | Port deliberately omits native-error overloads. Represented constructors yielded `0x80131500` through the reduced Win32 base. | Shared Win32 correction makes the represented surface `E_FAIL`; native overload scope and SR-AUD-250 remain unchanged. |

The failing conditional probe is retained as
`build-probe/1875-conditional-prefix-tests.json`.  It constructs the inner
value at runtime so no compiler constant folding can conceal the result.

## 4. Reproduction and permanent tests

Before production changes, the new integration fixture produced 13 failing
tests and 2 passing controls.  It observed every wrong prefix value shown in
§3, including all 36 represented constructors of the 12 type-specific rows and
five Win32-family controls.  The exact GoogleTest output is retained as
`build-probe/1875-prefix-tests.json`.

`tests/integration/System/ExceptionHResultPopulationTests.cpp` adds 15
permanent tests with 70 assertions:

- every represented public constructor of all 12 type-specific rows;
- both represented Win32 constructors and one control from each affected
  derived family (`NetworkInformationException`, `SocketException`, and
  `WebSocketException`);
- one pinning construction for every pure-inheritance row;
- ordinary no-inner controls for both extracted conditional rows.

The post-fix focused run passes 15/15 and is retained as
`build-probe/1875-postfix-tests.json`.

## 5. Compatibility and validation consequences

The implementation changes only inline constructor bodies by adding one
integer property store.  It changes no public declaration, overload,
accessibility, base class, field, virtual member, exception specification,
`constexpr`, calling convention, or mangled name.  Therefore object size,
alignment, offsets, vtables, virtual slots, and undefined-symbol sets remain
unchanged.  The intentional semantic consequence is limited to the HResult of
the 16 affected constructed types (12 direct types plus Win32 and its three
represented derived families).  Messages, inner pointers, native error fields,
and producer ordering are unchanged.

These are exception-construction paths, not normal hot paths; no performance
benchmark is warranted.  The additional store is the correct diagnostic work.
UBSan is relevant to the unsigned-literal-to-`intcs` conversions; ASan is run
with it in the retained combined sanitizer tree as supporting evidence.  ASan,
LSan, and TSan do not establish HResult semantics, and this change introduces
no ownership, allocation, or shared-state mechanism for them to validate.

The combined `-fsanitize=address,undefined` integration object was built at
`2026-08-01 10:50:06 +0200`, more than ten minutes after every changed header,
and its symbol table contains both `__asan_*` and `__ubsan_*` handlers. The
focused run passes 15/15 with zero diagnostics
(`build-probe/1875-asan-ubsan-tests.json`). CMake's first post-link test
discovery triggered LeakSanitizer's known sandbox `ptrace` fatal; rebuilding
with `ASAN_OPTIONS=detect_leaks=0` permitted discovery and the focused semantic
run. No LSan-clean claim is made, and disabling leak detection cannot affect the
ASan/UBSan checks or the permanent exact-value assertions.

## 6. Audit disposition

The HResult half of SR-AUD-157 is remediated by this ticket, so SR-AUD-157 may
move to remediated.  SR-AUD-158, SR-AUD-159, SR-AUD-196, SR-AUD-230,
SR-AUD-250, and every other adjacent finding retain their prior status.  Audit
numbering remains frozen at 364.  The cross-cutting #1875 sweep is complete
after the 45-row matrix, constant fixes, inheritance pins, and #1932 extraction;
no public API or ABI choice was made.
