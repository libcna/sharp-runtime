# Audit: `modules/io-hashing/src/System/IO/Hashing/NonCryptographicHashAlgorithm.cpp`

## Metadata

- AUDITED: common lifecycle, vector/stream append, and destination handling.
- Evidence: source review and current .NET base-class source.

## Assessment

Short destinations are handled, but a claimed sufficiently long null
destination proceeds to `GetCurrentHashCore` without validation, extending
SR-AUD-260 to every derived hasher. The stream loop correctly stops at zero;
asynchronous managed append remains intentionally unavailable.

## Other missing assertions and diagnostics

- Add null destination and stream exception tests, rejection-state tests, and
  diagnostics that distinguish a bad pointer contract from a short buffer.

## Final assessment

SR-AUD-260 applies. No source or test changed.
