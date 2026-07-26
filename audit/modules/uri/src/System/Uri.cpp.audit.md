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
