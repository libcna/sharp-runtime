# Audit: `modules/uri/include/System/UriBuilder.hpp`

## Metadata

- AUDITED: 251-line inline implementation, fully read.
- Validation: `UriBuilderTest.*` passed 27/27 on 2026-07-27.
- Reference/probe: local current .NET `UriBuilder.cs` and its functional tests,
  plus `/tmp/sharp-runtimervc-uri-builder-audit-probe` built against the C++
  module and a matching C# probe compiled with `mcs`/run with `mono`.

## SR-AUD-138 — medium — UriBuilder copied credentials are fused into UserName and later Password writes corrupt them

`setFieldsFromUri` assigns the whole `Uri::getUserInfoProperty()` (`user:pass`)
to `userName_` and never populates `password_`. Current .NET splits at the
first colon in `SetFieldsFromUri`. The probe constructs from
`http://user:pass@example.com/path` and prints C++ `copied_user=user:pass`,
`copied_password=`; after replacing Password it serializes
`http://user:pass:replacement@example.com:80/path`. The reference prints
`copied_user=user`, `copied_password=pass`, then the correct
`http://user:replacement@example.com:80/path`.

## SR-AUD-139 — medium — UriBuilder string construction fails to promote relative input to the default HTTP URI

Current .NET accepts `new UriBuilder("www.example.com/path")`, creates a
relative-or-absolute Uri, and promotes a relative result to `http://...` before
copying fields. C++ passes it directly to `Uri`, then overwrites its default
scheme/host with the relative URI's empty fields. Its unconditional
`scheme_ + "://"` rendering produces invalid `:///www.example.com/path`;
the reference produces `http://www.example.com:80/path`.

## SR-AUD-140 — medium — UriBuilder Equals and GetHashCode compare raw rendered strings instead of URI identity

`Equals` compares `ToString()` directly and `GetHashCode` hashes that same raw
Uri text. Current .NET delegates to `Uri.Equals`; its functional tests require
equality for builders differing only in user info or fragment. The C++ probe
prints false for both a credential-only and a fragment-only difference, while
the reference prints true. Consequently equal managed-style builders would
also have incompatible C++ hash values.

## SR-AUD-141 — medium — Scheme and IPv6 Host setters skip required normalization and validation

`setSchemeProperty` stores text verbatim: C++ keeps `HTTP` rather than lower
`http`, and accepts `bad scheme`, rendering `bad scheme://localhost/` instead
of throwing `ArgumentException`. The local .NET setter lowercases valid schemes
and rejects the invalid one; both paths are reproduced by the probe. Similarly
`setHostProperty("::1")` serializes invalid `http://::1/`; .NET recognizes the
IPv6 literal and emits `http://[::1]/`. These errors stem from raw assignment
in the public setters rather than a documented C++ adaptation.

## Other missing assertions and diagnostics

- Direct tests omit copied username/password separation, relative string
  promotion, malformed/uppercase scheme handling, IPv6 host bracketing, and
  equality/hash differences involving credentials or fragments.
- User-info escaping and component text escaping are not independently
  classified here because `Uri.hpp` explicitly documents its broader missing
  percent-encoding/decoding policy; the public UriBuilder contract needs a
  separate policy decision before treating those limits as defects.
- No test covers a Uri source with a password followed by field mutation, the
  shortest sequence exposing SR-AUD-138.

## Final assessment

Normal HTTP construction and query/fragment prefix smoke paths pass, but
constructor state transfer, relative input, identity, and public component
normalization materially diverge from current .NET. No source or test was
modified during this audit.

---

## Post-audit review corrections — ticket #1987 (2026-08-03)

The historical text above is preserved verbatim. Measured on 2026-08-03 by
`build-probe/1987_probe1_uri_boundaries.cpp`; recorded in
`docs/SystemUriNamespaceReviewPlan.md` §4.5.

1. **SR-AUD-140 has an availability half the finding does not name.** `GetHashCode()`
   builds a `Uri` from `ToString()`, so for a builder whose scheme the `Uri` parser rejects
   it **throws** where `Equals` on the same object returns `true`:

   ```
   self-Equals with invalid scheme      : 1
   GetHashCode with invalid scheme      : THROWS Invalid character in URI scheme
   ```

   An object that compares equal to itself has no obtainable hash. Recorded as inactive
   ticket **#2004** and deliberately **not** folded into #1995: #1995's repair (delegate
   identity to `Uri`) makes the throw *more* reachable, not less.

2. **SR-AUD-138 is confirmed exactly as written** and is repaired by ticket **#1993**;
   the repair leaves `ToString()` byte-identical for an unmodified builder and corrects only
   the two getters.

3. SR-AUD-139 and SR-AUD-141 are confirmed exactly as written and are approval-gated as
   ticket **#1996**, split into four independently answerable groups (plan §14.2).

**No `SR-AUD-*` identifier was issued** for #2004 — audit numbering stays frozen at **364**.
