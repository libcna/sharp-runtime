# Audit: `modules/buffers/include/System/Buffers/SequenceReader.hpp`

## Metadata

- Audit status: AUDITED (229-line public header-only implementation, fully
  read).
- Validation: `SequenceReaderTests.*` passed 13/13, as part of the complete
  63/63 `Batch6BuffersTests.cpp` focused filter in
  `SharpRuntimeTests_Buffers` on 2026-07-26.
- Reproducer: `/tmp/sharp-runtimervc-sequencereader-audit-probe.cpp` compiles
  with C++20 and prints `0,42,0,99` for failed `TryRead`/`TryPeek` with
  preinitialized `int` outputs.
- Reference: local .NET `SequenceReader.cs` and `SequenceReader.Search.cs`,
  including the false-output paths, were reviewed.

## Assessment

For a valid single-segment sequence, basic consumed/remaining state, relative
rewind, advancing, and delimiter scan behavior are coherent. The adaptation
uses output references rather than C# `out` values, but fails to explicitly
write their required default result on the normal false path.

## SR-AUD-075 — medium — failed SequenceReader TryRead and TryPeek retain stale output instead of returning default

When the reader is at end, `TryRead(T&)` and `TryPeek(T&)` immediately return
false without assigning their output reference. The probe initializes outputs
to `42` and `99`; both values remain unchanged after the false calls. Current
.NET explicitly writes `default` to the `out T` value in each false branch.

This is observable whenever a caller reuses output storage: a failed operation
can be mistaken for a newly read stale value despite the false boolean. C++
references cannot obtain the exact C# `out` language guarantee automatically,
so the public implementation must assign `T{}` before returning false or
document a deliberately different contract.

## Finding references

- **SR-AUD-072/SR-AUD-073 (context):** this reader depends on the current
  ReadOnlySequence adaptation. It does not itself accept caller-supplied
  positions, but a sequence constructed from invalid raw metadata or unsafe
  `TryGet` view still compromises any reader built over it.

## Other missing assertions and diagnostics

- The 13 direct cases do not inspect output values after failed `TryRead`,
  `TryPeek`, `IsNext`, or `TryReadTo`, nor state preservation after each false
  path.
- No test covers `TryPeek` at an offset, unread span/sequence/current span,
  `TryReadExact`, binary helpers, delimiter escapes, any-of delimiters, or
  multi-segment traversal; most current .NET SequenceReader surface is absent
  or intentionally unsupported.
- `total()` and `consumed_` narrow the source `long` state to C++ `int`; no
  diagnostic establishes behavior around the 32-bit ceiling.
- `TryReadTo` copies to a `std::vector`, so allocation/copy failure can occur
  while it advances state. Its rollback/exception guarantee and nontrivial-T
  semantics are untested.
- The reader stores a reference to its sequence and a raw Memory view; the
  header says the sequence must outlive it but does not diagnose moved,
  destroyed, or externally invalidated storage.

## Final assessment

Single-segment happy paths pass, but false read/peek operations violate the
source output contract and can leak stale caller state. No source or test was
modified during this audit.
