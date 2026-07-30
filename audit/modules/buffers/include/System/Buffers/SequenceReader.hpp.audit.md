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

---

## SR-AUD-075 — REMEDIATED (ticket #1872, 2026-07-30, CCF-014)

The original evidence above is retained unchanged.

`SequenceReader<T>::TryRead(T&)` and `TryPeek(T&)` now assign `value = T{}` on
their end-of-sequence branch, matching .NET's `value = default`
(`SequenceReader.cs:192-198` and `:114-126`). The reader's position is unchanged
by a failing call, before and after.

**Root cause, stated once for the family.** A C# `out T` parameter is *definitely
assigned on every returning path* — the reference's `value = default` lines are
what the compiler requires, not defensive style. Porting `out T` to a C++ `T&`
dropped that guarantee, and with it the assignment. The finding's own remark that
"C++ references cannot obtain the exact C# `out` language guarantee
automatically, so the public implementation must assign `T{}` before returning
false" is exactly right and is what was implemented.

**Measured before** (`build-probe/1871_prefix.log`, caller sentinels 42 / 99 /
`"stale"`): `seqreader.tryread.atend value=42`,
`seqreader.tryread.exhausted value=42`, `seqreader.trypeek.atend value=99`,
`seqreader.tryread.string value=stale`. **Measured after**
(`build-probe/1872_postfix_asan.log`): all four report the default, and
`seqreader.tryread.exhausted` still reports `counter=1`, so a failed read does
not rewind.

**Two facts established beyond the original evidence.**

1. **`TryReadTo` was already correct** and is not part of this finding: it clears
   `result` at entry *and* on the not-found path and restores `consumed_`,
   matching `SequenceReader.Search.cs:33-43`. It is now pinned by a permanent
   test so it cannot silently regress.
2. **The wrapper already compensated for the core.**
   `SequenceReaderExtensions::TryReadLittleEndian` assigns `value = 0` on false
   and reaches `SequenceReader::TryRead` through `detail::tryReadBytes`, which
   *also* assigns `value = T{}` — two layers implemented the contract that the
   layer between them did not. Together with `BinaryPrimitives::TryRead*`,
   `Utf8Formatter::TryFormat`, `StandardFormat::TryParse` and
   `ReadOnlySequence::TryGet`, five same-module surfaces already had it, which is
   why this is recorded as a localised omission rather than a module-wide design
   choice.

**One requirement recorded rather than assumed away.** `value = T{}` requires `T`
to be value-initialisable, which these two members did not previously require.
This is **not** a narrowing of the reference contract: .NET declares
`SequenceReader<T> where T : unmanaged`, and every `unmanaged` type has a
`default`, so value-initialisability is strictly weaker. Member functions of a
class template instantiate only when used, so any instantiation that does not
call `TryRead`/`TryPeek` is unaffected. A permanent test instantiates
`SequenceReader<std::string>` — far outside `unmanaged` — to demonstrate the
requirement is genuinely weak.

Closure evidence: 7 new permanent regressions in `Batch6BuffersTests.cpp`
(at-end and post-exhaustion `TryRead`; at-end and post-exhaustion `TryPeek` with
the position asserted in both; a non-trivial `std::string` element; the
already-correct `TryReadTo`; and one test pinning three sibling surfaces).
`SequenceReaderTests` + `Utf8ParserTest` 52/52, `SharpRuntimeTests_Buffers`
536/536, whole-repository build clean with zero errors and zero warnings.
**Mutation-checked:** reverting `TryRead`'s assignment to the pre-#1872 body
fails three permanent tests. The direct probe compiled **with**
`-fsanitize=address,undefined` — this is a header-only template, so instrumenting
the probe recompiles the changed code and no stale archive is involved — exits 0
with zero AddressSanitizer, UndefinedBehaviorSanitizer and LeakSanitizer reports.

Source, ABI and layout consequences: none beyond the recorded `T` requirement. No
signature, `noexcept` specification, virtual function or data member changed.

The plan for this family is `docs/TryOutputFailureContractPlan.md` (ticket #1871).
