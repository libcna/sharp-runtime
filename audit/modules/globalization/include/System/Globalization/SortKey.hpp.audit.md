# Audit: `modules/globalization/include/System/Globalization/SortKey.hpp`

## Metadata

- Audit status: AUDITED.

## Assessment

The value holder preserves source text and a copied key byte vector.  Key
quality and culture/option semantics depend entirely on `CompareInfo`.

## Finding references

- SR-AUD-283 — medium — the producer uses incomplete ASCII-byte comparison
  semantics.

## Other missing assertions and diagnostics

- Test clone/copy independence, comparison/hash agreement, and sort-key
  ordering under every supported comparison option.

## Final assessment

No independent defect is confirmed.
