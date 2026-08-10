<!-- SPDX-License-Identifier: MIT -->

# Network-exception inner-HResult propagation design (#1932)

## 1. Scope, authority, and approval boundary

Ticket #1932 is the inactive follow-up extracted from #1875 after the complete
derived-exception HResult sweep found two conditional reference behaviors that
were not constant-population fixes. This document designs those behaviors for:

- `System::Net::Http::HttpRequestException`; and
- `System::Net::WebException`.

It does **not** implement either behavior. The 2026-08-01 batch prompt expressly
withholds semantic approval for #1932, and matching .NET is not itself approval.
No `SR-AUD-*` identifier is created; audit numbering remains frozen at 364.

The .NET reference is the official `dotnet/runtime` `main` snapshot at commit
[`0eb5481340ea675857c7a7abf18f68a60b52a686`](https://github.com/dotnet/runtime/commit/0eb5481340ea675857c7a7abf18f68a60b52a686),
committed at `2026-08-01T02:32:34Z`. The exact source and available official
tests retained by #1875 are under `build-probe/1875-reference/`:

- [`HttpRequestException.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Net.Http/src/System/Net/Http/HttpRequestException.cs);
- [`HttpRequestExceptionTests.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Net.Http/tests/UnitTests/HttpRequestExceptionTests.cs); and
- [`WebException.cs`](https://github.com/dotnet/runtime/blob/0eb5481340ea675857c7a7abf18f68a60b52a686/src/libraries/System.Net.Requests/src/System/Net/WebException.cs).

The deterministic port probe is
`build-probe/1932_hresult_matrix.cpp`; its raw output and symbol inventory are
retained as `build-probe/1932_hresult_matrix.log` and
`build-probe/1932_hresult_symbols.log`. It was compiled with GCC 14.2.0,
`-std=c++23 -O2 -DNDEBUG`, against the live production headers and the live
`Exception`, `SystemException`, `InvalidOperationException`, and
`FormatException` implementations. It makes no production change.

## 2. Correction to the inherited premise

The original #1932 record is preserved. Two clarifications are required before
it can be implemented:

1. Inner-HResult copying is **not a project-wide exception rule**. Most .NET
   derived exceptions keep their own type-specific HResult even when they have
   an inner exception. `Exception` itself, `IOException`, `PingException`, and
   the other #1875 rows do not establish universal propagation.
2. `HttpRequestError`, `HttpStatusCode`, and `WebExceptionStatus` are not
   explicit HResult sources in current .NET. They do not take precedence over
   an inner exception. Both affected .NET types copy the inner HResult whenever
   their causal constructor receives a non-null `System.Exception`, including
   when status/error metadata is also supplied.

The correct design question is therefore not “copy every `exception_ptr`” and
not “let explicit status win.” It is whether the two existing C++ types should
adopt their own .NET conditional rule for a pointer that rethrows a
`System::Exception`, while defining safe behavior for the C++-only case where
`std::exception_ptr` contains a non-System exception.

## 3. Complete affected-surface inventory

### 3.1 Shared base storage

`System::Exception` owns the sole `hResult_` field, initialized to
`COR_E_EXCEPTION` (`0x80131500`). Its message/inner constructor stores the
`std::exception_ptr` but deliberately does not inspect or copy an inner
HResult. `SystemException` changes the value to `COR_E_SYSTEM`; every
`InvalidOperationException` constructor changes it to
`COR_E_INVALIDOPERATION` (`0x80131509`). `setHResultProperty` writes the existing
base field. No new storage is needed for any #1932 option that only changes
propagation.

The live probe measured:

| Type | `sizeof` | `alignof` | Copy constructible | Move constructible | Nothrow copy/move |
|---|---:|---:|---|---|---|
| `HttpRequestException` | 176 | 8 | yes | yes | no / no |
| `WebException` | 168 | 8 | yes | yes | no / no |

Both types are polymorphic through the inherited virtual destructor. Neither
declares a new virtual member. Their constructors are inline, non-`constexpr`,
and have no `noexcept` specification.

### 3.2 `HttpRequestException` constructors and state

The port exposes five constructible shapes (four declarations, because the
last declaration has default arguments):

| ID | Existing public construction | Current initialization | Additional public state |
|---|---|---|---|
| H1 | `HttpRequestException()` | `Exception()` | error `Unknown`; no status |
| H2 | `HttpRequestException(message)` | `Exception(message)` | error `Unknown`; no status |
| H3 | `HttpRequestException(message, inner)` | `Exception(message, inner)` | error `Unknown`; no status |
| H4 | `HttpRequestException(message, inner, status)` | `Exception(message, inner)` | error `Unknown`; status present |
| H5 | `HttpRequestException(error, message = "", inner = nullptr, optionalStatus = nullopt)` | `Exception(message, inner)` | supplied error/status |

There is no serialization constructor, cloning API, separate native-error
field, or out-of-line constructor in this port. The .NET status overload takes
a nullable `HttpStatusCode?`; the C++ H4 overload takes a non-nullable enum,
while H5 carries the optional form. That pre-existing surface difference is not
changed by #1932.

### 3.3 `WebException` constructors and state

The port exposes five declarations:

| ID | Existing public construction | Current initialization | Additional public state |
|---|---|---|---|
| W1 | `WebException()` | `InvalidOperationException()` | status `UnknownError` |
| W2 | `WebException(message)` | `InvalidOperationException(message)` | status `UnknownError` |
| W3 | `WebException(message, inner)` | `InvalidOperationException(message, inner)` | status `UnknownError` |
| W4 | `WebException(message, status)` | `InvalidOperationException(message)` | supplied status |
| W5 | `WebException(message, inner, status)` | `InvalidOperationException(message, inner)` | supplied status |

The .NET full constructor also accepts `WebResponse?`; the port deliberately
does not represent `WebRequest`, `WebResponse`, the `Response` property, or
legacy formatter serialization. The .NET serialization constructor delegates
to its base serialization path and does not run the causal-constructor
propagation block. There is no corresponding port path to change. The C++ type
has implicit copy/move construction but no cloning API.

### 3.4 Related networking exceptions

The complete nearby inventory disproves a family-wide propagation rule:

| Type/path | HResult rule relevant to #1932 |
|---|---|
| `HttpIOException` / `HttpProtocolException` | Keep the `IOException` HResult (`0x80131620`) for represented constructors; an inner exception does not replace it. |
| `PingException` | Keeps `InvalidOperationException`'s HResult; its wrapper is independent of #1932. |
| `CookieException` | Keeps `FormatException`'s HResult. |
| `ProtocolViolationException` | Keeps `InvalidOperationException`'s HResult. |
| `SocketException` / `NetworkInformationException` | The represented surface inherits `Win32Exception`'s `E_FAIL`; native error accessors are separate state. |
| `WebSocketException` | Current .NET's represented enum/message constructors retain `E_FAIL`; native-error overloads conditionally replace HResult only when the native value is negative. This is a distinct precedence rule and the port deliberately omits those native overloads. |
| `NetworkStream` wrapping | Wraps a `SocketException` in `IOException`; `IOException` keeps its own HResult. |
| WebSocket async rethrow | `ClientWebSocket::ConnectAsync` catches, disposes, and uses bare `throw;`; it does not construct either #1932 type. |

No related constructor should be changed as an inseparable part of #1932.

### 3.5 Producers, catches, and sync/async paths

Every current built-in `HttpRequestException` producer supplies **no inner
exception**:

- DNS resolution, connection, and send failures use H2;
- malformed status line, `Content-Length`, and chunk size use H2;
- premature response completion uses H5 with `ResponseEnded` and null inner;
- `HttpResponseMessage::EnsureSuccessStatusCode` uses H5 with error `Unknown`,
  an explicit status, and null inner.

Consequently the recommended propagation rule would not change any current
built-in producer result. It changes direct consumer construction and any
future producer that supplies a causal System exception.

`HttpClient::Send`, `Get`, `Post`, `GetString`, and `GetByteArray` pass the
exception through. Their async counterparts execute those synchronous methods
inside `TaskT`. `TaskT` records `std::current_exception()` and rethrows the same
`exception_ptr` from `getResultProperty()`/`Wait()`, retaining dynamic type,
message, HResult, inner pointer, and HTTP metadata. `HttpMessageInvoker` likewise
forwards both synchronous and asynchronous handler exceptions without wrapping.

There is no production `WebException` construction, wrapping, catch, or rethrow
site in the represented legacy-net subset. It is currently a directly
constructible compatibility type.

Existing permanent coverage pins no-inner HResults in
`ExceptionHResultPopulationTests.cpp`, HTTP status/error accessors and producer
exceptions in `HttpClientTests.cpp`, and basic `WebException` status/message
state in `NetTests.cpp`. No permanent test asserts the unapproved inner-HResult
semantic. The two focused temporary assertions retained by #1875 fail on the
current port exactly as ticket #1932 states.

## 4. Measured sharp-runtime behavior

### 4.1 Deterministic inner corpus

The probe used all of the following:

- null inner;
- base `Exception` with default `0x80131500`;
- `FormatException` with type-specific `0x80131537`;
- base `Exception` changed at runtime to `0x81234567`;
- base `Exception` changed at runtime to zero;
- non-System `std::runtime_error` (a C++-only case);
- nested `HttpRequestException` changed to `0x82345678`; and
- nested `WebException` changed to `0x83456789`.

Every non-null inner pointer was recovered byte-for-byte equal through
`getInnerExceptionProperty()`, rethrew with its dynamic type and message, and
retained its own HResult. The mismatch is only the outer HResult.

### 4.2 Current constructor matrix

| ID / scenario | Current outer HResult | Message | Error/status | Inner observation |
|---|---:|---|---|---|
| H1 default | `0x80131500` | empty | `Unknown`; none | null |
| H2 message | `0x80131500` | supplied | `Unknown`; none | null |
| H3 null inner | `0x80131500` | supplied | `Unknown`; none | null |
| H3 default inner | `0x80131500` | supplied | `Unknown`; none | inner `0x80131500` |
| H3 format inner | `0x80131500` | supplied | `Unknown`; none | inner `0x80131537` |
| H3 custom inner | `0x80131500` | supplied | `Unknown`; none | inner `0x81234567` |
| H3 zero inner | `0x80131500` | supplied | `Unknown`; none | inner `0x00000000` |
| H3 non-System inner | `0x80131500` | supplied | `Unknown`; none | `std::runtime_error`; no HResult |
| H3 nested HRE | `0x80131500` | supplied | `Unknown`; none | inner `0x82345678` |
| H3 nested WebException | `0x80131500` | supplied | `Unknown`; none | inner `0x83456789` |
| H4 status + format inner | `0x80131500` | supplied | `Unknown`; `502` | inner `0x80131537` |
| H5 error/status + custom inner | `0x80131500` | supplied | `ConnectionError`; `503` | inner `0x81234567` |
| H5 error + null inner | `0x80131500` | supplied | supplied; none | null |
| W1 default | `0x80131509` | port default invalid-operation text | `UnknownError` | null |
| W2 message | `0x80131509` | supplied | `UnknownError` | null |
| W3 null inner | `0x80131509` | supplied | `UnknownError` | null |
| W3 default inner | `0x80131509` | supplied | `UnknownError` | inner `0x80131500` |
| W3 format inner | `0x80131509` | supplied | `UnknownError` | inner `0x80131537` |
| W3 custom inner | `0x80131509` | supplied | `UnknownError` | inner `0x81234567` |
| W3 zero inner | `0x80131509` | supplied | `UnknownError` | inner `0x00000000` |
| W3 non-System inner | `0x80131509` | supplied | `UnknownError` | `std::runtime_error`; no HResult |
| W3 nested HRE | `0x80131509` | supplied | `UnknownError` | inner `0x82345678` |
| W3 nested WebException | `0x80131509` | supplied | `UnknownError` | inner `0x83456789` |
| W4 status, no inner | `0x80131509` | supplied | supplied | null |
| W5 status + format inner | `0x80131509` | supplied | supplied | inner `0x80131537` |

Copy and move construction preserve the source object's already-stored outer
HResult, message, inner pointer, and error/status fields. The probe changed the
source values to `0x8456789A` (HRE) and `0x856789AB` (WebException); both copy
and move results retained those exact values. Wrapping either outer object with
`std::make_exception_ptr` and rethrowing retained its dynamic type and all state.

Default-message differences between the C++ port and .NET are pre-existing and
outside #1932. No option below changes message text.

## 5. Current .NET behavior, constructor by constructor

### 5.1 `HttpRequestException`

Current .NET implements the causal rule in `(message, inner)`:

```text
base(message, inner)
if (inner != null) HResult = inner.HResult
```

Every more-specific public constructor delegates through that constructor.
Therefore:

| ID | Fixed outer value when no inner | Non-null inner | Does explicit metadata win? |
|---|---:|---|---|
| H1 default | `COR_E_EXCEPTION` | not applicable | no metadata |
| H2 message | `COR_E_EXCEPTION` | not applicable | no metadata |
| H3 message + inner | `COR_E_EXCEPTION` for null | exact `inner.HResult` | not applicable |
| H4 message + inner + status | `COR_E_EXCEPTION` for null | exact `inner.HResult` | **no**; status is orthogonal |
| H5 error + message + inner + status | `COR_E_EXCEPTION` for null | exact `inner.HResult` | **no**; error and status are orthogonal |

The condition is only `inner != null`. Zero is copied as zero. A default base
exception copies `COR_E_EXCEPTION`; a type-specific exception copies its own
code; nested HRE/WebException values propagate transitively. There is no
selection by HResult family, sign, facility, or network origin.

### 5.2 `WebException`

Current .NET funnels the causal public overload through the full
`(message, inner, status, response)` constructor. That constructor assigns the
status/response and then copies `innerException.HResult` when the inner is
non-null.

| ID | Fixed outer value when no inner | Non-null inner | Does explicit metadata win? |
|---|---:|---|---|
| W1 default | `COR_E_INVALIDOPERATION` | not applicable | default status |
| W2 message | `COR_E_INVALIDOPERATION` | not applicable | default status |
| W3 message + inner | `COR_E_INVALIDOPERATION` for null | exact `inner.HResult` | not applicable |
| W4 message + status | `COR_E_INVALIDOPERATION` | not applicable | status does not create HResult |
| W5 message + inner + status | `COR_E_INVALIDOPERATION` for null | exact `inner.HResult` | **no**; status is orthogonal |

`CreateCompatibleException(HttpRequestException)` uses the full constructor and
therefore copies the HRE HResult while mapping status separately. Its
`TaskCanceledException` path supplies null inner and retains
`COR_E_INVALIDOPERATION`. The obsolete serialization constructor delegates to
base serialization; it is not part of the causal precedence rule and has no
port counterpart.

The two .NET types intentionally use the **same conditional propagation rule**.
They differ only in their no-inner base HResult and their ancillary fields.

## 6. Design options

### Option 1 — always retain the outer type-specific/base HResult

This is the current port behavior: HRE always retains `COR_E_EXCEPTION` and
WebException always retains `COR_E_INVALIDOPERATION`, regardless of inner.

- **Constructor matrix:** every row remains exactly §4.2.
- **Compatibility:** no source, binary, layout, or semantic change.
- **Observable consequence:** permanent divergence from current .NET for H3,
  H4, H5, W3, and W5 with a non-null System inner.
- **Migration risk:** none, but callers cannot recover the causal code from the
  outer accessor without rethrowing the inner pointer themselves.
- **Consistency with #1875:** preserves #1875's no-inner controls, but leaves
  the conditional population deliberately unresolved.
- **Tests:** turn the measured divergence into permanent pinning tests and
  document it as supported behavior.
- **Rollback:** not applicable.

### Option 2 — conditional System-inner propagation

The minimum option originally described as “when no explicit outer error is
supplied” needs one correction: none of the represented status/error arguments
is an HResult. The coherent variant, **Option 2R**, is:

1. begin with the existing outer base HResult;
2. if the pointer is null, retain it;
3. rethrow a non-null pointer;
4. if it is catchable as `const System::Exception&`, copy its HResult exactly,
   including zero;
5. if it is any non-System exception, retain the existing outer HResult; and
6. never let `HttpRequestError`, `HttpStatusCode`, or `WebExceptionStatus`
   override either result.

- **Constructor matrix:** H1/H2/W1/W2/W4 unchanged; H3/H4/H5/W3/W5 follow the
  five-step conditional rule.
- **Source compatibility:** declarations, calls, overload resolution, and
  default arguments are unchanged.
- **ABI/layout/vtable/symbols:** an inline-body-only implementation adds no
  field, base, virtual, slot, declaration, or mangled name. Sizes remain
  176/8 and 168/8. Existing weak inline constructor symbols retain their names.
- **`noexcept`/`constexpr`:** unchanged; none of these constructors carries
  either promise.
- **Observable consequence:** outer HResult changes only for a non-null pointer
  containing a System exception whose HResult differs from the existing outer
  base value. Messages, inner identity, and metadata are unchanged.
- **Migration risk:** code relying on the divergent outer value changes. As the
  bodies are inline, every consumer must be rebuilt to avoid mixed old/new
  constructor semantics across translation units.
- **Performance:** one `exception_ptr` rethrow/catch on causal construction;
  no request hot path changes today because all built-in producers use null/no
  inner. Accessors and ordinary construction remain unchanged.
- **Consistency with #1875:** preserves all fixed/type-specific population
  work and applies only the two conditional rows #1875 explicitly extracted.
- **Tests:** the complete matrix in §8 is mandatory.
- **Rollback:** restore the two causal constructor bodies and their status/error
  delegations; no state migration is required.

**Recommended: Option 2R**, but only after explicit user approval.

### Option 3 — propagate only selected platform/network HResults

Possible filters include negative values, `FACILITY_WIN32`, known socket codes,
or a type allow-list.

- It necessarily rejects valid .NET cases such as zero, custom codes, and
  non-network System exception codes.
- It makes nested precedence depend on an unstable classifier rather than the
  constructor contract.
- It needs a new mapping policy and a much larger permanent corpus.
- It changes semantics without improving source/ABI consequences over 2R.

Rejected: neither .NET nor the port's existing exception taxonomy supplies the
required selection rule.

### Option 4 — add a separate native/network error field

HRE already carries `HttpRequestError` and optional `HttpStatusCode`; WebException
already carries `WebExceptionStatus`. Another integer could preserve the outer
HResult and expose a cause code separately.

- **Source/API:** needs a new accessor and contract.
- **ABI/layout:** adds state (or another indirection) to public exception
  objects; sizes/offsets and possibly symbols change.
- **Semantics:** does not make existing `getHResultProperty()` match .NET.
- **Migration:** callers must opt into a new property.
- **Rollback:** cannot safely remove an adopted public field/accessor.

Rejected: redundant, incompatible, and not approved.

### Option 5 — preserve and document the divergence

This is Option 1 with an explicit permanent-deviation decision, documentation,
and tests. It is the correct fallback if the user values current C++ behavior
over .NET conditional parity. It has zero implementation risk but leaves
`getHResultProperty()` observably divergent on causal construction.

Not recommended, but viable and cleanly reversible because it requires no
production change.

### Option 6 — additive constructor or factory

Examples include `FromInnerHResult(...)`, a tag-dispatched constructor, or a
factory that opts into propagation while preserving existing constructors.

- **Source/API:** adds a new spelling and potentially creates overload/default
  argument ambiguity.
- **ABI/symbols:** an out-of-line factory adds public symbols; an inline factory
  still expands the public header surface.
- **Semantics:** existing .NET-shaped constructors remain divergent.
- **Migration:** callers must discover and adopt a port-only API.
- **Rollback:** removing a shipped public spelling is a source/ABI break.

Rejected: more surface with less parity.

### Explicitly rejected extra variant — synthesize an HResult for non-System exceptions

`std::exception_ptr` is broader than .NET's `Exception?`. Mapping
`std::runtime_error`, integer exceptions, or arbitrary objects to an HResult
would invent a new taxonomy. Option 2R instead retains the outer base HResult
for those C++-only values while preserving the pointer unchanged.

## 7. Exact recommended implementation shape and ticket split

No multi-ticket implementation split is warranted. After approval, one bounded
#1932 implementation ticket should atomically:

1. change only the existing H3/H4/H5 and W3/W5 inline constructor paths to
   apply Option 2R;
2. keep all declarations, members, bases, virtuals, defaults, accessors, and
   producer call sites unchanged;
3. add the direct, copy/move, `exception_ptr`, and sync/async tests in §8; and
4. repeat the layout/symbol/declaration comparison and focused sanitizer run.

There is no current producer-integration sub-ticket: every built-in HRE producer
uses null/no inner, and the port has no WebException producer. A future producer
that begins wrapping a causal exception must be reviewed on its own behavior;
it must not be invented to make #1932 appear larger.

The implementation should remain inline to avoid adding exported constructor
symbols. A small private/internal helper may be used only if it introduces no
public declaration or externally visible symbol. The implementation must not
move this policy into `Exception`, because that would silently change every
derived exception with an inner pointer and contradict the measured .NET
matrix.

## 8. Permanent proof required after approval

### 8.1 Direct constructor matrix

For each causal constructor H3/H4/H5/W3/W5, assert:

- null retains `0x80131500` or `0x80131509` as applicable;
- base default HResult propagates;
- `FormatException`'s type-specific HResult propagates;
- a runtime-set custom HResult propagates;
- zero propagates exactly;
- nested HRE and nested WebException values propagate;
- a non-System exception retains the outer base value;
- outer type, message, inner identity/dynamic type/message/HResult, and every
  error/status accessor remain exact.

Pin H1/H2/W1/W2/W4 as unchanged controls. Include empty and UTF-8 messages and
non-standard enum values because constructors store them without validation.

### 8.2 Copy, move, and exception transport

Assert implicit copy and move preserve an already-calculated outer HResult and
all other state; they must not re-run propagation. `std::make_exception_ptr`
and repeated rethrow must preserve the dynamic outer type, outer HResult,
message, inner pointer, and metadata.

### 8.3 Sync and async HTTP paths

Use a test handler that throws a preconstructed causal HRE. Assert that
`HttpClient::Send` and `SendAsync(...).Wait()` plus
`HttpMessageInvoker::Send`/`SendAsync` expose the same dynamic exception and
state. Pin one ordinary built-in H2 failure and
`EnsureSuccessStatusCode`'s H5/null-inner path as unchanged. No network socket
is needed for the handler transport proof; existing socket-enabled HTTP tests
remain enabled for the built-in producer controls.

WebException has no represented producer, so direct/transport tests are the
complete port surface; do not manufacture a legacy WebRequest subsystem.

### 8.4 Structural and sanitizer proof

- compare public declarations and constructor default arguments;
- compare `sizeof`, `alignof`, vtable/virtual slots, defined/undefined symbols,
  mangled names, `noexcept`, and `constexpr` status;
- prove changed production objects are newer than their changed sources;
- run the focused integration and owning module suites;
- run ASan/UBSan against the rebuilt objects.

ASan can detect bad exception-object lifetime/access and UBSan can detect
invalid casts, but neither proves the HResult precedence rule; exact assertions
do. LSan adds no specific evidence because Option 2R adds no allocation or
ownership. TSan is not indicated because the constructor adds no shared mutable
state.

## 9. Migration and rollback

Option 2R changes only an observable diagnostic integer on causal construction.
Callers that compare the outer HResult to the port's old base value must instead
expect the inner System exception's value. Code that already inspects
`getInnerExceptionProperty()` sees no change. Error/status branching remains
identical.

Because constructors are inline, release notes must require a clean consumer
rebuild. Mixing objects compiled against old and new headers can yield both
semantics in one process without any linker error even though mangled names are
unchanged. Rollback is a two-header/test revert followed by the same clean
rebuild; serialized state and persisted data are unaffected.

## 10. Recommendation and exact approval wording

Recommend **Option 2R**. It is the smallest rule that matches both affected
.NET types, has an exact answer for arbitrary C++ exception pointers, reuses
existing storage, leaves every built-in producer unchanged today, and creates
no source/API/ABI/layout/vtable/symbol/`noexcept`/`constexpr` change.

Rejected alternatives are Options 1/5 (permanent divergence), 3 (invented
classifier), 4 (redundant incompatible state), 6 (port-only public surface),
and synthetic mapping for non-System exceptions.

Copyable approval wording:

> Approve #1932 Option 2R only: for every existing
> `HttpRequestException` and `WebException` constructor that accepts
> `std::exception_ptr`, if the non-null pointer rethrows a
> `System::Exception`, copy that inner exception's HResult exactly, including
> zero; if the pointer is null or contains a non-System exception, retain the
> current outer base HResult. `HttpRequestError`, `HttpStatusCode`, and
> `WebExceptionStatus` do not override this rule. Do not add constructors,
> fields, accessors, or producer wrapping, and do not change public
> declarations, ABI, layout, vtables, symbols, `noexcept`, or `constexpr`.
> Preserve messages, inner identity, and all error/status fields, and add the
> complete direct/copy/move/exception-pointer and sync/async transport matrix
> from `docs/NetworkExceptionHResultPropagationDesign.md`.

#1932 may appear in the same consolidated packet as the other remaining
decisions, but it should be approved as its **own independent group**. It does
not share a contract with #1894, #1899, #1925, #1926, or the remaining #1929
rows and should not be hidden inside a vague “approve all fixes” instruction.

## 11. Approved Option 2R implementation closure (2026-08-01)

The user approved the exact §10 wording for #1932 alone. The implementation is
limited to H3/H4/H5 and W3/W5. H3 and W3 preserve the supplied
`std::exception_ptr`, rethrow it only to classify the contained object, catch
`const System::Exception&`, and copy `getHResultProperty()` exactly. A null
pointer bypasses classification and a final non-System catch leaves the
already-constructed outer base value unchanged. H4/H5 and W5 delegate to their
respective causal constructor, then publish the existing status/error fields;
the metadata therefore cannot override the copied value. No producer was made
to wrap an exception.

The retained prefix and postfix matrices establish the complete result:

| Family / input | Prefix outer HResult | Option 2R outer HResult |
|---|---:|---:|
| HRE no inner or null | `0x80131500` | `0x80131500` |
| HRE base `Exception` default | `0x80131500` | `0x80131500` |
| HRE zero inner | `0x80131500` | `0x00000000` |
| HRE `FormatException` | `0x80131500` | `0x80131537` |
| HRE custom inner | `0x80131500` | exact custom value |
| HRE nested HRE/WebException | `0x80131500` | exact nested value |
| HRE non-System inner | `0x80131500` | `0x80131500` |
| WebException no inner or null | `0x80131509` | `0x80131509` |
| WebException base `Exception` default | `0x80131509` | `0x80131500` |
| WebException zero inner | `0x80131509` | `0x00000000` |
| WebException `FormatException` | `0x80131509` | `0x80131537` |
| WebException custom inner | `0x80131509` | exact custom value |
| WebException nested HRE/WebException | `0x80131509` | exact nested value |
| WebException non-System inner | `0x80131509` | `0x80131509` |

Explicit `HttpStatusCode`, `HttpRequestError`, and `WebExceptionStatus` rows
produce the same HResult column as the corresponding inner row while retaining
their exact metadata. Copy construction, move construction, copy assignment,
move assignment, repeated `exception_ptr` rethrow, message text, dynamic type,
and inner identity remain exact. Existing `HttpClient` and
`HttpMessageInvoker` synchronous and asynchronous forwarding paths preserve the
same causal exception. The built-in HTTP producer controls still construct
without an inner pointer and retain `0x80131500`; no represented WebException
producer exists.

`NetworkExceptionHResultPropagationTests` adds 13 permanent tests. The prefix
semantic run was 2/12 passing and 10/12 failing only on the missing outer value;
the postfix suite is 13/13, including its structural test. Together with the
15 existing #1875 tests, the focused value gate is 28/28. The socket-enabled
repository gate is 15,071/15,071 across 37 executables, Integration 893 and
Core.Base 5,585.

The combined ASan+UBSan build contains both instrumentation runtimes, its
changed objects and test binary are newer than these headers, and all 13 tests
pass with leak discovery disabled. A leak-enabled run completes all assertions
but LeakSanitizer then fails its discovery phase because the sandbox is
ptraced; no LSan-clean claim is made. Sanitizers support lifetime, cast, and
destruction checks; the permanent assertions and constructor matrix—not the
sanitizers—establish HResult precedence.

Public declarations, default arguments, mangled-name sets, defined- and
undefined-symbol sets, bases, fields, offsets, vtables, virtual slots,
`noexcept`, and `constexpr` state are unchanged. Measured size/alignment remain
176/8 for HttpRequestException and 168/8 for WebException. Because the bodies
are inline, the clean-consumer-rebuild warning in §9 still applies. No material
design premise differed at implementation time, no separate defect was found,
and no inactive ticket or audit identifier was added.
