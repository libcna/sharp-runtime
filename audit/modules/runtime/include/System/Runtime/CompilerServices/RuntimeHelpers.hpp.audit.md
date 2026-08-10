# Audit: `modules/runtime/include/System/Runtime/CompilerServices/RuntimeHelpers.hpp`

## Metadata

- AUDITED: 247-line inline low-level helper declaration, fully read.
- Validation: `RuntimeHelpersTests.*` passed 5/5 on 2026-07-27.
- Reference basis: local current-.NET `RuntimeHelpers.cs` and its focused
  RuntimeHelpers test source.

## Assessment

The implemented native operations have coherent C++ meanings: raw/shared
identity, address-derived identity hashing, by-value copying, vector range
copying, cleanup-on-normal/exceptional exit, and conservative reference-content
classification.  The local tests exercise those representative paths.

CLR-dependent operations consistently throw `PlatformNotSupportedException`
where no native type-loader, RVA field metadata, boxing, or reflection object
can supply a fabricated result.  CER/stack preparation, class/module runtime
construction, and `TryEnsureSufficientExecutionStack` are explicitly
documented no-op/best-effort adaptations.  Current .NET has real stack and
runtime metadata behavior, but the header does not silently claim parity; no
separate defect is classified for those permanent native boundaries.

## Other missing assertions and diagnostics

- Cleanup tests omit empty callbacks, a cleanup callback that throws after
  normal or exceptional code, user-data round-trip identity, nested cleanup,
  and preservation of an original exception when cleanup itself fails.
- Identity/hash tests omit raw addresses, hash stability/collisions, lifetime
  reuse, pointer-alias forms, and the fact that a zero/nonzero identity hash is
  not a uniqueness guarantee.
- GetSubArray tests omit empty/from-end/full ranges, nontrivial element copies,
  a null-like native container policy, and oversized-vector conversion beyond
  `intcs` range.
- Reference-content tests omit raw pointers, weak/unique pointers, trivially
  copyable wrappers, and user-defined types that own native resources without
  containing managed-style references.
- Unsupported and no-op APIs lack capability diagnostics beyond their current
  fixed result/exception; no test distinguishes a future real stack probe from
  today's always-success adaptation.

## Final assessment

The meaningful native helpers are tested on normal paths and unsupported CLR
mechanisms fail explicitly.  No confirmed source defect and no source or test
modification resulted from this review.
