# Audit: `modules/text/include/System/Text/ASCIIEncoding.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET Encoding.GetString](https://learn.microsoft.com/en-us/dotnet/api/system.text.encoding.getstring?view=net-10.0).

## Assessment

The declaration exposes signed raw-buffer offsets and inherited fallback
settings without documenting or enforcing their managed argument and fallback
contracts.

## Finding references

- SR-AUD-286 — high — signed raw decode ranges are not validated.
- SR-AUD-292 — medium — configured fallback policies are ignored.

## Other missing assertions and diagnostics

- Test negative index/count, null nonempty input, exception fallback, and
  non-ASCII decode offsets.

## Final assessment

SR-AUD-286 and SR-AUD-292 apply.
