# Audit: `modules/net/include/System/Net/CookieCollection.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [CookieCollection.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/CookieCollection.cs).
- Evidence: ASan/UBSan probe `/tmp/sharp-runtime-net-audit/cookie_collection_bounds.cpp`.

## Assessment

The thin vector adaptation provides ordered iteration, but its public indexers
convert signed input directly to `size_t` and use unchecked `operator[]`.

### SR-AUD-307 — high — negative or oversized collection indexes reach unchecked vector access

With one cookie, `collection[-1]` converts to a huge unsigned index and the
ASan probe terminates with a segmentation fault at the indexer call.  Managed
`CookieCollection` rejects invalid indexes deterministically.  The C++ API
must not expose raw-container undefined behavior at this public boundary.

Required remediation: validate `0 <= index < Count` and throw the repository's
standard index exception in both const and mutable indexers.

## Missing assertions and diagnostics

There is no direct CookieCollection test coverage.  Add negative, `Count`, and
empty-collection index cases under ASan/UBSan.

## Final assessment

Confirmed public out-of-bounds crash.
