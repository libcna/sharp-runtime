# Audit: `modules/core/include/System/ObsoleteAttribute.hpp`

## Metadata

- Audit status: AUDITED (47-line value attribute, fully read with its
  dedicated fixture).
- Validation: `ObsoleteAttributeTest.*` passed 6/6 in the 77-test focused
  Core.Base attribute filter on 2026-07-26.
- Reference basis: local .NET `System/ObsoleteAttribute.cs:9-40`.

## SR-AUD-115 — medium — ObsoleteAttribute stores an error flag but cannot mark a declaration or produce its documented compiler diagnostic

`ObsoleteAttribute` is an ordinary runtime object with no declaration wrapper,
compiler attribute, or registry (`ObsoleteAttribute.hpp:11-45`).  Its public
comment says `isError = true` makes use a compile-time error (`:26-30`), but
constructing or mutating the object cannot alter use of any C++ symbol.  .NET
attaches this metadata to a declaration and its compiler emits a warning or
error based on `IsError`.

The green tests assert only stored strings and booleans.  They never compile a
deprecated declaration, distinguish warning from error, or test target and
non-inheritance restrictions.

## SR-AUD-116 — medium — ObsoleteAttribute collapses nullable .NET string properties into indistinguishable empty strings

In current .NET `Message`, `DiagnosticId`, and `UrlFormat` are nullable
`string?` properties, so a default attribute exposes `null` and callers can
distinguish it from an explicitly supplied empty string.  This port stores
three non-nullable `std::string` fields initialized empty (`:12-15`) and
returns references to them (`:33-39`), erasing that observable state without a
documented `optional<string>` adaptation.  The test fixture enshrines empty
strings for the default `Message`, `DiagnosticId`, and `UrlFormat`.

## Other missing assertions and diagnostics

- No test verifies construction with an empty message separately from default
  construction, UTF-8 diagnostics, copy/move state, or derived-type policy.
- Equal-valued object comparisons follow SR-AUD-114 rather than .NET's default
  fieldwise `Attribute.Equals` behavior.

## Final assessment

The scalar payload round-trips, but diagnostic behavior and nullable state are
not faithfully represented.  No source or test was modified during this audit.
