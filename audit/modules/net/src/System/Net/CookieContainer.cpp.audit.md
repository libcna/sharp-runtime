# Audit: `modules/net/src/System/Net/CookieContainer.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [CookieContainer.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/CookieContainer.cs).
- Evidence: `/tmp/sharp-runtime-net-audit/net_contracts.cpp`.

## Assessment

Domain/path matching for stored valid cookies follows the intended simplified
RFC 6265 logic.  Insertion trusts a supplied domain instead of validating it
against the source URI, and storage has no bounded-resource policy.

### SR-AUD-305 — high — `Add(uri, cookie)` accepts an unrelated explicit domain and later emits that cookie for it

The probe adds a cookie from `origin.invalid` with explicit domain
`.unrelated.invalid`; `GetCookieHeader` for `unrelated.invalid` then returns
`session=isolated`.  The reference calls `Cookie.VerifyAndSetDefaults` before
storage and rejects an invalid domain relation with `CookieException`.

Required remediation: validate explicit domain and path defaults against the
source URI before storage, including the leading-dot and host-only rules.

### SR-AUD-308 — medium — cookie storage is unbounded despite a public network-container role

The implementation deliberately never applies capacity, per-domain capacity,
maximum cookie size, expired-cookie cleanup on insertion, or LRU eviction.
Current .NET has bounded defaults and cleans/ages cookies before accepting new
ones.  The limitation is documented but no API exposes an opt-in limit, so a
long-running client can retain arbitrary header-derived state.

Required remediation: implement bounded policy compatible with the documented
defaults, or expose explicit limits and make the unbounded adaptation opt-in.

## Missing assertions and diagnostics

There is no `CookieContainer` test file.  Add source-domain rejection,
host-only/domain-cookie, default-path, expiration cleanup, size, per-domain,
and total-capacity cases; include a diagnostic for rejected cookie attributes.

## Final assessment

One isolation failure and one resource-governance omission are confirmed.
