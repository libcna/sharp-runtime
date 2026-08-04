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

## Post-audit remediation for SR-AUD-076 (ticket #2053, 2026-08-04): REMEDIATED

The audit evidence above is retained unchanged. The owning review is
[`docs/BuffersNamespaceReviewPlan.md`](../../../../../../docs/BuffersNamespaceReviewPlan.md)
(ticket #2048); **no `SR-AUD-*` identifier was issued.**

Reproduced first: `Create(0, 1)`, `Create(1, 0)` and `Create(-5, -7)` all returned a usable
pool, and `Create(1024, 10)->Rent(4000)` returned **4000** elements — the declared maximum was
not applied.

`ArrayPool<T>::Create(intcs, intcs)` now throws `ArgumentOutOfRangeException` for a
non-positive `maxArrayLength` or `maxArraysPerBucket`, exactly as .NET's
`ConfigurableArrayPool` constructor does.

**Only the validation half is remediated, and the header now says so plainly.** Honouring the
limits needs a configured pool type this port does not have: `SharedArrayPool` allocates a
fresh vector on every `Rent` and has no buckets to size, so a configured pool would be a new
public class with new state — a public-surface addition outside this batch's envelope. That
is recorded as an explicit exclusion in the review plan's §21 rather than smuggled in.

**A second contract on this type was false and is now corrected** (#2061, doc-only):
`Return` is **not** .NET's ownership transfer. `Rent` hands back a `std::vector<T>` by value,
so the caller keeps owning the storage afterwards and continued use — forbidden in .NET — is
legal here; nothing is retained for reuse; and `clearArray` zeroes *the caller's own vector*
rather than scrubbing a buffer the pool is about to hand to someone else, so it is not a
guarantee that sensitive data has left the process. Pinned by
`ArrayPoolOwnershipPinTests.ReturnDoesNotTakeOwnershipAndDoesNotPool`.

Closure evidence: **8 permanent regressions** covering all five invalid configurations, the
smallest valid one, an ordinary one, and the unchanged parameterless `Create()`. Both
pre-existing call sites pass positive values and stayed green unmodified. Source and ABI
consequences: none — no signature, layout or exported-symbol change; only the accepted input
set narrowed.
