# Audit: `modules/net-http-json/include/System/Net/Http/Json/HttpContentJsonExtensions.hpp`

## Metadata

- AUDITED: synchronous/asynchronous JsonDocument content readers and ownership
  boundary.
- Validation: two content-only tests passed; ASan direct C++20 probe and a
  current .NET 10 counterpart exercised null content on 2026-07-27.

## SR-AUD-236 — high — null HttpContent crashes through a raw dereference instead of producing the required argument diagnostic

`ReadFromJson` immediately calls `content->ReadAsString()` with no shared_ptr
validity check.  The ASan probe passing an empty `shared_ptr<HttpContent>`
reports a null-address SEGV at the public header's line 32.  The equivalent
current-.NET `HttpContentJsonExtensions.ReadFromJsonAsync<object>(null)` throws
`ArgumentNullException`.  `ReadFromJsonAsync` delegates to the same path, so it
turns null input into a deferred task crash rather than a checked boundary.

## Assessment

The intentionally JsonDocument-only and synchronous-content adaptations are
clearly disclosed.  The null native representation is nevertheless publicly
reachable and unsafe.

## Other missing assertions and diagnostics

- Add exact null-content diagnostics for both methods, invalid JSON and empty
  content, task lifetime/cancellation, and ReadAsString failure propagation.

## Final assessment

SR-AUD-236 is ASan-confirmed by direct C++/current-.NET comparison.  No source
or test was changed during this audit.

## Post-audit remediation for SR-AUD-236 (ticket #1814, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged.

Ticket #1814 (`REMED-NET-HTTP-JSON-NULL-CONTENT`, P1, size S) rejects an empty
`std::shared_ptr<HttpContent>` in **both** `ReadFromJson` and `ReadFromJsonAsync`
with `ArgumentNullException("content")`, matching .NET, whose every public
`HttpContentJsonExtensions` overload opens with
`ArgumentNullException.ThrowIfNull(content)`.

Reproduced before any production change, one process per case
(`build-probe/1814_prefix_defects.cpp`, logs `1814_prefix_defects.log` and
`1814_postfix_defects.log`):

| Case | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `ReadFromJson(empty)` | UBSan *member access within null pointer* at `HttpContentJsonExtensions.hpp:32`, then **ASan SEGV on 0x0**, exit 1 | `ArgumentNullException` |
| 2 | `ReadFromJsonAsync(empty)` then `getResultProperty()` | same pair, on worker thread **T1** | same, thrown by the call itself |
| 3 | `ReadFromJsonAsync(empty)` and **never awaited** | same pair, on worker thread **T1** — the process still died | same |
| 4 | the valid synchronous path | `answer=42` | **byte-identical** |
| 5 | the valid asynchronous path | `answer=7` | **byte-identical** |

**Case 3 is the reason the async guard is placed where it is.** The finding says
`ReadFromJsonAsync` "turns null input into a deferred task crash"; measured, it is
worse than deferred. Because the dereference happened on the `std::async` worker
thread, a caller that started the task and never awaited it still lost the whole
process to a SEGV on thread T1. A guard placed only inside `ReadFromJson` would
have downgraded that to an exception stored on a task the caller never observes —
quieter, still wrong, and a divergence from .NET.

.NET avoids exactly this by code layout: its public `ReadFromJsonAsync` overloads
are **not** `async` methods. Each runs `ArgumentNullException.ThrowIfNull(content)`
and only then calls the separate `ReadFromJsonAsyncCore`, so a null argument is a
synchronous throw at the call site. This port now has the same shape, and a named
regression pins it.

**One component-metadata consequence, recorded because it is the first in this
remediation series.** `Net.Http.Json` is an `INTERFACE` (header-only) component,
so the guard lives in a public header and the header must include
`System/ArgumentNullException.hpp`. `Net.Http.Json` previously reached `Core.Base`
only transitively through `Net.Http`; the boundary validator correctly rejected
the undeclared public edge, so `modules/net-http-json/CMakeLists.txt` now declares
`Core.Base` in `PUBLIC_DEPENDENCIES` and `docs/ComponentCatalog.md` was
regenerated. The production graph goes from **90 to 91 direct edges**; module
count is unchanged at 41. No consumer include path, target name or link line
changes as a result.

Closure evidence: **7 new permanent regressions** in
`HttpContentJsonExtensionsTests.cpp` — both entry points rejected, the parameter
name on both, the synchronous-throw placement of the async guard, repeatability,
content whose body is the JSON literal `null` (an empty `shared_ptr` and a
document that parses to null are different things), and empty content still
reaching the parser rather than the new guard. `SharpRuntimeTests_Net_Http_Json`
is **15/15** (was 8), and the same 15 under AddressSanitizer +
UndefinedBehaviorSanitizer + LeakSanitizer with **zero reports**
(`build-asan/1814_net_http_json_asan.log`). Repository gate: 0 warnings, 0 errors,
**13,994 tests across 37 executables** (was 13,987).

Source and ABI consequences: none. No public signature, object layout, vtable or
exported symbol changed. Behavioural note for consumers: passing an empty
`shared_ptr` to either method now throws instead of crashing the process. No
in-repository caller did so.

**Deliberately out of scope.** `HttpClientJsonExtensions`'s
`GetFromJsonAsync`/`DeleteFromJsonAsync` already guard their response content, but
they map a null content body to the JSON literal `"null"`
(`content ? content->ReadAsString() : "null"`) rather than to a diagnostic. That is
a different contract on a different type, it carries no `SR-AUD-*` identifier, and
it was left exactly as found rather than changed under this ticket.
