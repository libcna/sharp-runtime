# Audit: `modules/security-cryptography-random/CMakeLists.txt`

## Metadata

- AUDITED: 16-line Security.Cryptography.Random component registration,
  including its Windows-specific private `bcrypt` linkage.
- Validation: module-boundary validation passed with 41 physical modules and
  90 dependency edges on 2026-07-27; generated catalogue check passed; the
  `sharp_runtime_security_cryptography_random` target built successfully.
- Reference basis: `cmake/SharpRuntimeComponents.cmake`,
  `docs/ComponentCatalog.md`, and the audited random-provider source tree.

## Assessment

The registration declares static component
`Security.Cryptography.Random` with `Core.Base` as its only public dependency.
On Windows it links `bcrypt` privately through the registered setup callback.
The generated catalogue has the same ownership, target, public closure, and
private platform dependency.  Module-boundary validation found no undeclared
edge.

## Missing assertions and diagnostics

- Linux component builds cannot validate the Windows-only `bcrypt` link or the
  matching BCrypt implementation path; platform CI remains necessary.
- CMake source discovery is configured-tree behavior, not an explicit audited
  source-list check.

## Final assessment

The component registration and generated metadata agree.  No new finding and
no source or test change.
