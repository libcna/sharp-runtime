# Audit: `modules/io-hashing/README.md`

## Metadata

- AUDITED: component description and dependency claims.
- Evidence: the description correctly identifies non-cryptographic hashing and
  the two declared public dependencies.

## Assessment

The README is concise but does not document the raw pointer/length substitute
for managed spans or its required validation semantics. That omission leaves
the risks recorded in SR-AUD-260 and SR-AUD-261 undiscoverable to C++ callers.

## Other missing assertions and diagnostics

- Document null, zero-length, and negative-length behavior for every raw
  pointer overload, plus the exact cross-endian XXH guarantee.

## Final assessment

No separate finding. No source or test changed.
