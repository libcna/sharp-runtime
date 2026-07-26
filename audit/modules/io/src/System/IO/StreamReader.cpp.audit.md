# Audit: `modules/io/src/System/IO/StreamReader.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-337 — medium — StreamReader/StreamWriter Close with leaveOpen keeps the wrapper usable

Both text wrappers treat `leaveOpen` as “do nothing” rather than as “do not close the base stream.”  Neither has a disposed flag or checks one on public operations.  The direct lifecycle probe closes a `StreamReader` and `StreamWriter` constructed with `leaveOpen=true`, then prints `reader-after-close=97` and `writer-after-close=accepted`; the base `MemoryStream` remains correctly open.  Current .NET leaves only the base stream open: the reader/writer itself is disposed and later operations throw `ObjectDisposedException`.

## SR-AUD-338 — high — text stream wrappers accept a null base stream and reach silent EOF or a null dereference

The raw-pointer constructors perform no null validation.  A direct ASan/UBSan probe prints `reader-null-read=-1` for `StreamReader(nullptr, true)`; `StreamWriter(nullptr, true).Write("x")` then terminates with an ASan null read in `StreamWriter::WriteRaw`.  The corresponding BinaryReader/BinaryWriter constructors already reject null with `ArgumentNullException`, which makes this inconsistency especially hazardous.

## Missing assertions and diagnostics

- The IO tests cover ordinary text reads/writes and base-stream ownership, but do not assert wrapper disposal separately from base-stream ownership for either leaveOpen mode (SR-AUD-337).
- No constructor-null test exists for StreamReader or StreamWriter, and there is no lifecycle diagnostic identifying a closed text wrapper before the raw pointer is dereferenced (SR-AUD-338).
- Existing coverage remains ASCII/byte-oriented; BOM, UTF-8 decoding, malformed input, buffer flushing failure, and exceptional base-stream paths are untested in this documented partial surface.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
