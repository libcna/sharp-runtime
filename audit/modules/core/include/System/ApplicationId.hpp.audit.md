# Audit: `modules/core/include/System/ApplicationId.hpp`

## Metadata

- Audit status: AUDITED (108-line inline value implementation, fully read).
- Validation: `ApplicationIdTests.*` passed 16/16 in the 22-test identity
  filter on 2026-07-26.
- Reference basis: local .NET `System/ApplicationId.cs:9-83`.

## SR-AUD-124 — medium — ApplicationId replaces binary/null-aware identity fields with undocumented strings and skips required name validation

Current .NET takes and clones a `byte[]` public-key token, rejects null/empty
Name, and permits nullable Culture/ProcessorArchitecture.  This port instead
takes all three as mandatory `std::string` values (`ApplicationId.hpp:35-42`),
accepts an empty name, and documents neither byte encoding nor a null/empty
sentinel.  Arbitrary binary key material and the distinction between a null and
an explicitly empty optional component are therefore unrepresentable or
ambiguous.

The green tests use only printable `"token123"`, nonempty name, and
`"neutral"`/`"amd64"`; they never challenge constructor validation, binary
bytes, empty/null adaptation, or copy isolation of token storage.

## SR-AUD-125 — medium — ApplicationId.ToString omits the public-key token and uses a different identity grammar

Current .NET writes lowercase quoted `culture`, `version`, `publicKeyToken`
(uppercase hex bytes), and `processorArchitecture`, omitting nullable fields.
The port instead emits unquoted capitalized `Version`/`Culture`/
`ProcessorArchitecture`, never includes the token, and always includes the
last two fields (`ApplicationId.hpp:101-105`).  This makes its string unable to
identify unequal ApplicationIds that differ only by token and incompatible with
the current manifest identity representation.

Tests merely search for fragments, so all 16 pass without checking exact text,
token distinction, optional omission, escaping, or byte-to-hex conversion.

## Other missing assertions and diagnostics

- Equality includes every stored field as .NET does, but hash tests do not
  verify equality/hash coupling across differing token/culture/architecture.
- `Copy` is a normal value copy; no test shows token representation/lifetime.

## Final assessment

Core scalar storage works, but validation, binary/nullable modeling, and text
identity compatibility are materially incomplete.  No source or test was
modified during this audit.
