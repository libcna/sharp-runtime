<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `UriBuilder` validates its scheme and promotes a relative string (ticket #1996, groups G-3 and G-4)

*2026-08-19.* `UriBuilder::setSchemeProperty` rejects text that is not a scheme, and
`UriBuilder(string)` promotes a relative string to `http://…` instead of rendering an unparseable
result.

**G-3 is a narrowing** — a setter that never threw now throws — which is exactly what SA-5 grants
("including where a call that succeeds today starts throwing"). No signature, layout, vtable or
`noexcept` change.

---

## 1. G-3 — the scheme setter

Transcribed whole from `UriBuilder.cs:108-134`:

```csharp
if (value.Length != 0)
{
    if (!Uri.CheckSchemeName(value))
    {
        int index = value.IndexOf(':');
        if (index != -1) value = value.Substring(0, index);
        if (!Uri.CheckSchemeName(value))
            throw new ArgumentException(SR.net_uri_BadScheme, nameof(value));
    }
    value = value.ToLowerInvariant();
}
```

**The truncate-at-colon retry is the half the ticket's summary omitted.** §14.2 says only
*"throw for an invalid one"*, which would reject `"http:"` — .NET **accepts** it and stores
`http`, and accepts `"HTTPS://"` the same way. Only text that is still not a scheme *after*
truncation is refused. An **empty** scheme is accepted too, because the whole block is guarded on
`value.Length != 0` — easy to lose when transcribing a guard that throws.

The message and parameter name are .NET's: *"Invalid URI: The URI scheme is not valid."* and
`nameof(value)`.

**One pre-existing test's subject moved, and the new guarantee is stronger.** A row in G-2's pin
asserted that a *non-ASCII* scheme is folded invariantly — an ASCII-only fold, so a Turkish locale
could not change what a scheme means. G-3 makes such a scheme **unstorable**: `CheckSchemeName`
accepts only ASCII letters, digits and `+-.`, and `HÜTTP` has no `:` to truncate at. The locale
hazard is now closed by construction rather than by the fold's implementation.

## 2. G-4 — the string constructor

```csharp
// setting allowRelative=true for a string like www.acme.org
_uri = new Uri(uri, UriKind.RelativeOrAbsolute);
if (!_uri.IsAbsoluteUri)
    _uri = new Uri(Uri.UriSchemeHttp + Uri.SchemeDelimiter + uri);
SetFieldsFromUri();                                                    // UriBuilder.cs:29-40
```

Before this, `UriBuilder("www.example.com/path")` rendered `:///www.example.com/path` — an
unparseable string with an empty scheme and empty host.

**The ticket's summary is wrong about the host.** §14.2 says the string is promoted *"to the
default `http` scheme and `localhost` host"*. The reference prefixes `http://` and **reparses**, so
the host comes out of the string itself: `www.example.com/path` yields host `www.example.com`, not
`localhost`. A `localhost` host appears only where the string supplies none, which is the
pre-existing default-field behaviour and not this promotion. A mutation that uses
`http://localhost/` is caught.

The promotion runs **only** when the parse is not absolute, so nothing that already worked moves.

## 3. A wrong expectation of mine, corrected by the reference

The promoted result renders `http://www.example.com:80/path`. My first test asserted
`http://www.example.com/path` and **failed** — and the code was right, not the test. .NET's
`SetFieldsFromUri` does `_port = _uri.Port` (`UriBuilder.cs:307`), `Uri("http://…").Port` is 80,
and `ToString()` appends the port whenever `_port != -1` (`:381`). **.NET renders the default port
too.**

Measured, both routes are identical — `UriBuilder("www.example.com/path")` and
`UriBuilder("http://www.example.com/path")` produce the same string — which is the strongest form
of the assertion and is what the test now makes.

## 4. To migrate

```cpp
b.setSchemeProperty("bad scheme");   // was: stored verbatim.  now: ArgumentException
b.setSchemeProperty("http:");        // accepted, stores "http"  (unchanged in effect)
UriBuilder("www.example.com/path");  // was: ":///www.example.com/path"
                                     // now: "http://www.example.com:80/path"
```

If you were storing unvalidated scheme text in a builder, that value could never render a
parseable URI.

## 5. Evidence

Six mutations; results in the ticket record. Two are worth naming: dropping the
**truncate-at-colon retry** (which would reject `"http:"`, the half the summary omitted), and
promoting through `http://localhost/` instead of `http://` (the host the summary wrongly
predicted).

Two pre-existing pins were **inverted** — `Decl1996_G3AndG4AreNotTakenAndStayPinned` existed
solely to record that these groups were untaken — and one row of a third moved its subject, as
§1 describes.

Gate: **17,533 run, 17,533 passed, 0 failed, 0 skipped** across 38 executables — `+5` on 17,528
(`SharpRuntimeTests_Uri` 300 → 305; two pins inverted in place, one row's subject moved). No other
executable moved. Module graph unchanged at 41/93.

## 6. Downstream, measured

`UriBuilder` appears in **zero** places in `cna` and **zero** in `mobile-eggbert`. Neither
repository was modified.

## 7. Scope

This closes **#1996 entirely**: G-1 and G-2 landed earlier the same day, G-3 and G-4 land here.
One boundary is deliberately still not taken and stays pinned — .NET's `Host` setter throws
`ArgumentException(net_uri_BadHostName)` for `"contoso.com/path"` and this port stores it, which
belongs to G-1's block and was never in scope for either group.
