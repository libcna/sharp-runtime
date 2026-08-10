# Audit: `modules/buffers/tests/System/Buffers/Batch16BuffersTests.cpp`

## Metadata

- Audit status: AUDITED (315 lines, 37 tests in nine suites, fully read).
- Validation: `DecimalOACurrencyTests.*:StringInternTests.*:ArrayPoolCreateTests.*:MemoryHandleTests.*:IPinnableTests.*:MemoryManagerTests.*:SearchValuesTests.*:SearchValuesFactoryTests.*:SequenceReaderExtensionsTests.*`
  passed 37/37 in `SharpRuntimeTests_Buffers` on 2026-07-26.
- Scope note: this is a mixed batch. It is complete as a test-source review;
  implementation reports for SearchValues and SequenceReaderExtensions remain
  separate pending work.

## Assessment

The fixture gives useful smoke evidence for several independently implemented
surfaces and includes expected failure output for a short SequenceReader
extension input. It is intentionally shallow: production semantics such as
currency rounding, string interning identity, configured pooling, pin lifetime,
manager-backed memory, and delimiter state are mostly represented by a single
happy-path value.

## Finding references

- **SR-AUD-037:** Decimal OLE currency tests use only exact four-decimal
  values, so they do not expose the confirmed truncation-versus-rounding
  behavior.
- **SR-AUD-070/SR-AUD-076:** ArrayPool creation tests use only valid limits,
  do not inspect clear content, and permit the factory to ignore its public
  configuration/default-construction constraints.
- **SR-AUD-075 (context):** SequenceReaderExtensions checks short-input output
  correctly, but does not exercise its reader's own failed TryRead/TryPeek
  output-default contract.
- **SR-AUD-077:** SearchValues tests instantiate only hashable built-ins. They
  leave the advertised equality-only generic contract untested; an equality-
  only C++ type fails because the implementation additionally requires
  `std::hash<T>`.

## Other missing assertions and diagnostics

- `String::Intern`/`IsInterned` tests compare string values only; they cannot
  establish managed intern-table identity, null behavior, or the documented
  C++ adaptation.
- `ArrayPoolCreateTests.Create_ClearOnReturn` uses `SUCCEED` after clear rather
  than inspecting contents, reuse, configuration, or Return ownership.
- `MemoryHandle`/`IPinnable` omit RAII destruction, repeated Dispose, pointer
  offset, actual pin lifecycle, and no-reallocation guarantees.
- `MemoryManagerTests.GetMemoryProperty_Throws` locks in the documented
  unsupported manager-backed Memory adaptation. No test offers a real manager
  or distinguishes a deliberate architecture limit from an accidental native
  exception.
- SearchValues cases omit empty/duplicate/copy-mutation, Unicode, and
  equality-only/custom-hash, comparison-contract behavior. SequenceReader
  extension cases omit unsigned, 64-bit big-endian, all short-widths, reader
  position after false, and multi-segment sequences.

## Final assessment

All 37 tests pass. The batch is valuable feature smoke evidence but must not
be treated as conformance coverage for its broad public surface. No test source
was modified during this audit.
