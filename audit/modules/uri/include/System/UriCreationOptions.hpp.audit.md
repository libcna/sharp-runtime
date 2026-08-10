# Audit: `modules/uri/include/System/UriCreationOptions.hpp`

## Metadata

- AUDITED: 23-line value type, fully read.
- Validation: `UriCreationOptionsTest.*` passed 3/3 within the selected 38-test
  URI value-type filter on 2026-07-27.
- Reference basis: local current Uri constructors and TryCreate overloads.

## SR-AUD-149 — medium — UriCreationOptions is a disconnected mutable flag with no Uri constructor or TryCreate consumer

The C++ header explicitly says its flag is not enforced, and audited `Uri.hpp`
has neither `Uri(string, UriCreationOptions)` nor options-bearing TryCreate.
Current .NET exposes both constructor and TryCreate overloads and uses the flag
to control documented path/query canonicalization behavior. C++ callers can
set an inert field but cannot pass it to a Uri operation at all.

## Other missing assertions and diagnostics

- Tests only verify ordinary mutable storage and never show a Uri operation
  receiving or observing the option.
- The C++ struct has a public field rather than the .NET property shape; this
  is secondary to the complete missing consumer route.

## Final assessment

The value itself stores a bool, but it cannot provide the public option
contract. No source or test was modified during this audit.
