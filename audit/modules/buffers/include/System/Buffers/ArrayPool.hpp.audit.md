# Audit: `modules/buffers/include/System/Buffers/ArrayPool.hpp`

## Metadata

- Audit status: AUDITED (85-line public header-only implementation, fully
  read).
- Validation: `ArrayPoolTest.*` passed 7/7 in `SharpRuntimeTests_Buffers` on
  2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-arraypool-config-audit-probe.cpp` builds
  with C++20 and prints `1,1`: both `Create(0, 1)` and `Create(1, 0)` return a
  usable pool.
- Reference: local .NET `ArrayPool.cs`, `ConfigurableArrayPool.cs`, and
  `SharedArrayPool.cs` were reviewed.

## Assessment

The shared local-static lifetime, negative Rent validation, zero-length result,
and optional clear operation work for ordinary default-constructible vectors.
The C++ type intentionally allocates a new vector on every Rent, which is a
documented performance adaptation. Its configurable factory, however, exposes
two parameters with neither the source validation nor any behavior.

## SR-AUD-076 — medium — ArrayPool.Create configuration is silently ignored and invalid limits are accepted

`ArrayPool<T>::Create(intcs maxArrayLength, intcs maxArraysPerBucket)` names
the same public configuration as .NET, but discards both arguments and always
returns the unconfigured `SharedArrayPool`. The standalone probe shows that
zero for either required-positive input returns a pool. Current .NET's
`ConfigurableArrayPool` throws `ArgumentOutOfRangeException` for zero/negative
`maxArrayLength` and `maxArraysPerBucket`, then uses the limits to build its
buckets.

The C++ comment admits that the values are unused, but an API-compatible
factory cannot silently accept invalid public configuration and present it as
applied. It must either validate and honor a documented bounded adaptation or
not expose the overload until those controls have semantics.

## Finding references

- **SR-AUD-070 (extended):** both `Rent` and `Return(clearArray=true)` use
  vector sized/default construction and `T{}`, repeating the undocumented
  default-constructible element requirement confirmed for ArrayBufferWriter
  and MemoryPool.

## Other missing assertions and diagnostics

- The direct test calls Create only with `(1024, 10)` and checks non-null; it
  omits zero/negative values, a configured pool's behavior, and any diagnostic
  that values are ignored.
- No test asserts the Return ownership rule, double/foreign return behavior,
  concurrent Rent/Return, buffer reuse policy, size bucketing, or whether
  clear applies only to a retained buffer as in the source contract.
- `std::vector<T>` return-by-value keeps full caller ownership after Return;
  the header does not explain that a caller can continue to mutate its vector,
  unlike .NET where post-Return use is forbidden. This is a C++ ownership
  adaptation that needs an explicit policy before consumers infer pooling.
- Huge Rent allocation and native `length_error`/`bad_alloc` error taxonomy,
  move-only/non-default-constructible elements, and exception safety of clear
  are untested.

## Final assessment

The simple allocate-and-clear path is serviceable, but the public configurable
factory is a nonfunctional stub that accepts invalid input without a
diagnostic. No source or test was modified during this audit.
