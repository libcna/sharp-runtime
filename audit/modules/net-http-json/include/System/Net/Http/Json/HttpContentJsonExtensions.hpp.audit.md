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
