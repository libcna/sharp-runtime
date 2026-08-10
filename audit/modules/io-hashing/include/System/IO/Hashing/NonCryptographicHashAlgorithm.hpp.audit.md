# Audit: `modules/io-hashing/include/System/IO/Hashing/NonCryptographicHashAlgorithm.hpp`

## Metadata

- AUDITED: common hash lifecycle, vector/stream append, and raw output API.
- Evidence: declaration/source review against current .NET base contract.

## Assessment

The raw pointer forms replace `Span<byte>` without a nullable representation
or stated invalid-pointer contract. A positive null destination is passed to
derived writers, extending SR-AUD-260. Async append is explicitly unported and
is recorded as an adaptation, not a separate defect.

## Other missing assertions and diagnostics

- Cover null output, all short/negative destination lengths, stream read
  exceptions, reset-on-success only, and empty-vector input.

## Final assessment

SR-AUD-260 applies. No source or test changed.
