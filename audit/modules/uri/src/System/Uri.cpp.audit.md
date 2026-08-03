# Audit: `modules/uri/src/System/Uri.cpp`

## Metadata

- AUDITED: 363-line implementation, fully read.
- Validation: `UriTests.*` passed 57/57 on 2026-07-27.
- Reference/probe: local current .NET Uri implementation and matching C++/C#
  probe output recorded under `/tmp/sharp-runtimervc-uri-audit-probe`.

## SR-AUD-142 — medium — Uri retains raw case/default-port text as its semantic identity

`parse` stores scheme/host exactly as supplied and `operator==` hashes/compares
the original raw URI string. C++ therefore reports `HTTP`, `EXAMPLE.COM`, and
false equality for `HTTP://EXAMPLE.COM:80/Path` versus
`http://example.com/Path`. Current .NET canonicalizes Scheme/Host and considers
these two Uris equal. The header even says parsed Scheme is lower-case. This
also leaves `GetHashCode` incompatible for semantically equal URI values.

## SR-AUD-143 — medium — opaque mailto URIs bypass the documented default-port table

`defaultPortForScheme` declares `mailto` port 25, but the opaque URI branch
unconditionally assigns `port_ = -1`. The probe prints C++ `mailto_port=-1`
for `mailto:user@example.com` while the reference prints `25`. Normal HTTP,
HTTPS, and FTP tests never exercise an opaque scheme with a built-in port.

## SR-AUD-144 — medium — base Uri resolution treats query, fragment, and network-path references as ordinary paths

The `(baseUri, relativeUri)` constructor strips a query/fragment then always
merges the remaining text as a path under the base authority. For base
`http://example.com/a/b?old#old`, C++ produces
`http://example.com/a/?new`, `http://example.com/a/#new`, and
`http://example.com//other.example/c` for `?new`, `#new`, and
`//other.example/c`. Current .NET respectively preserves the full base path
(and old query for fragment-only) or switches authority to
`http://other.example/c`.

## SR-AUD-145 — medium — Uri accepts malformed IPv6 authority text and invalid UriKind enum values

The parser accepts malformed `http://[::1/path` and stores host `[:`; .NET
throws UriFormatException. Separately, a public `static_cast<UriKind>(99)`
matches neither guarded branch and silently behaves as RelativeOrAbsolute,
where .NET throws ArgumentException. Both inputs cross public validation
boundaries and produce ordinary objects rather than a clear error.

## Other missing assertions and diagnostics

- Existing tests omit case/default-port equality/hash, `mailto` Port,
  query-only/fragment-only/network-path resolution, malformed bracketed IPv6,
  and invalid `UriKind` values.
- The documented no-percent-encoding/no-AbsoluteUri-canonicalization boundary
  is broader than these proven defects and should be resolved as an explicit
  compatibility policy rather than silently repaired piecemeal.
- `TryCreate` catches all exceptions, so its allocation/system-error policy is
  not distinguished from invalid URI input.

## Final assessment

The normal HTTP and simple dot-segment paths are green, but supported identity,
opaque-port, resolution, and validation behavior materially diverge from .NET.
No source or test was modified during this audit.

---

## Post-audit review corrections — ticket #1987 (2026-08-03)

The historical text above is preserved verbatim. Everything below was **measured** on
2026-08-03 by `build-probe/1987_probe1_uri_boundaries.cpp` (output retained as
`build-probe/1987_probe1_before.log`) during the `System::Uri` namespace review, and is
recorded in `docs/SystemUriNamespaceReviewPlan.md` §4.

1. **SR-AUD-145 fabricates a *port* as well as a host.** The finding says the parser
   "stores host `[:`". Measured, `http://[::1/path` yields **`host="[:"` *and* `port=1`**:
   the authority is split on `rfind(':')`, which for `"[::1"` lands on index 2, so `"1"`
   becomes the port. A caller that connects to `Port` reaches port 1 rather than failing.
   Two further bracketed shapes the finding does not name are accepted the same way:
   `http://[::1]junk/path` → `host="[::1]junk"`, and `http://[]/path` → `host="[]"`.
   Ticket **#1991** repairs the shape — the bracket structure is never validated — rather
   than the named site.

2. **SR-AUD-143 is not `mailto`-specific.** The opaque branch assigns `port_ = -1`
   unconditionally, so **every** opaque scheme with a table entry loses its default:
   `telnet:host.example.com` also reports `-1` where `defaultPortForScheme` says 23.

3. **A second, unnamed site loses the default port.** `http://example.com:/` (a bare
   trailing colon) reports `port=-1`, while `http://example.com/` reports `80`. `parse`
   consults `defaultPortForScheme` only on the branch where the authority contains no colon
   at all. Folded into ticket **#1989** as the same root cause.

4. **The largest defect in this file has no finding.** `parse` locates the scheme with
   `uriString.find("://")` — a search for `"://"` *anywhere* — and only falls back to the
   grammar-correct `findSchemeColon` when that fails, although `findSchemeColon`'s own
   doc-comment states the RFC 3986 rule. Measured consequences, all ordinary input:
   `/path?redirect=http://evil.com`, `search?url=https://example.com`,
   `mailto:a@b.com?body=see http://x` and `foo:bar://baz` **all throw**, because the
   substring match landed inside a query or after a first colon. Repaired by ticket
   **#1988**; the repair is a proved strict widening (plan §9.1). **No `SR-AUD-*`
   identifier was issued** — audit numbering stays frozen at **364**.

5. **Five further post-audit defects, recorded as inactive tickets, no `SR-AUD-*`
   identifier** (a sixth, #2004, belongs to `UriBuilder.hpp` and is recorded in that file's
   report): an empty authority is accepted (`http://`, `http:///`, `http://:80/path`
   all yield `host=""` with the default port) — **#2000**; the two-`Uri` constructor with an
   **opaque** base fabricates an authority (`Uri(Uri("mailto:a@b.com"), "c")` →
   `mailto:///c`) — **#2001**; a relative `Uri` never splits its query or fragment out of
   `AbsolutePath` — **#2002**; an embedded NUL crosses the parser into every component —
   **#2003**; and surrounding whitespace is rejected while an internal space in the host is
   accepted — **#2005**, a deferred verification.

6. **Re-measured, not assumed:** `UriTests.*` still passes 57/57 on 2026-08-03, so this
   report's validation line has not drifted. The probe directory it cites,
   `/tmp/sharp-runtimervc-uri-audit-probe`, **no longer exists**, and
   `/rv/tmp/runtime/src/libraries/` is **absent from this environment**; where a repair
   depended on .NET's behaviour, the plan §7 names the surviving evidence and defers the
   items that have none.
