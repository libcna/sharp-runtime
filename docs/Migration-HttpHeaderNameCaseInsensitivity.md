<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — HTTP field names are compared case-insensitively (ticket #2068)

*2026-08-17.* `HttpRequestMessage`, `HttpResponseMessage` and `HttpClient`'s default headers
compared field names byte-for-byte, so `Content-Type` and `content-type` were two different
headers. And `HttpClientHandler` wrote `Host`, `User-Agent`, `Accept` and `Connection`
unconditionally, before the caller's map — so a caller who set `Host` sent **two** Host fields.

Landed under `docs/StandingApprovals.md` SA-5. **No public type changed, no layout moved, and no
signature moved** — see §3.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `setHeader("Content-Type", a)` then `setHeader("content-type", b)` | two entries, **both on the wire** | one entry, last write wins |
| `getHeader("content-type")` after `setHeader("Content-Type", …)` | `""` | the value |
| `getDefaultHeader("accept")` after `setDefaultHeader("Accept", …)` | `""` | the value |
| a client default merged onto a request that already has the field in another casing | merged; **both on the wire** | not merged |
| a caller-set `Host` | the handler's Host **and** the caller's, both sent | only the caller's |
| a caller-set `User-Agent`, `Accept` or `Connection` | duplicated the same way | only the caller's |
| the spelling stored in the map | as written | **as written** — unchanged |

The last row matters: the caller's casing is preserved, matching .NET, which keeps the string it
was given rather than canonicalising it. Only *comparison* changed.

## 2. Why the duplicate-`Host` half is here and not in a compatible ticket

Two `Host` fields in one request is request-smuggling-adjacent: intermediaries may disagree about
which one is authoritative. De-duplicating them needs a case-insensitive lookup — a caller who
writes `host` must suppress the handler's `Host` too — which is exactly what this ticket
introduces. Splitting them would have meant shipping half a defence.

.NET does the same: `HttpConnection.WriteHeadersAsync` writes its own `Host` only when the request
carries none.

## 3. The blocking premise was avoidable

`docs/SystemNetHttpNamespaceReviewPlan.md` recorded this ticket as blocked because *"the
comparator of the returned map is PUBLIC SURFACE: #2068 cannot make the lookup case-insensitive
without changing this type"* — a public source break under SA-2, with all five of its conditions.

That is true of one implementation and only one: swapping `std::unordered_map`'s hash and
equality. It is not true of the *rule*. The maps now hold at most one entry per field name because
`setHeader` erases case variants on the way in, and lookups scan case-insensitively. A header
collection holds a handful of entries, so the linear scan costs nothing measurable — and nothing
of SA-2's machinery either.

`getHeadersProperty()`'s return type is still `const std::unordered_map<std::string,
std::string>&`, and the two `static_assert`s that pinned it are kept, reworded to say that
changing it is still a break nobody needs to take.

## 4. To migrate

Nothing, if your header names were consistent. If you deliberately set two casings of one field
expecting both to be sent, that was never valid HTTP and .NET never did it.

If you were working around the duplicate `Host` by *not* setting one, you can set it now.

## 5. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `HttpClient` or `System::Net::Http` — **zero sites
in both**. Neither repository was modified.
