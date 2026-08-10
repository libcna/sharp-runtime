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

## Post-audit remediation for SR-AUD-338 (ticket #1806, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged, and **SR-AUD-337 is untouched and
stays `confirmed`** — this ticket repaired the null base stream only, not the
`leaveOpen` disposal contract that shares these two files.

`StreamReader(Stream*, bool)` now throws `ArgumentNullException("stream")`, and
the `stream_ == nullptr` tests in `Peek()`, `Read()`, `Close()` and the destructor
are **removed**, because the constructor check makes `stream_` a non-null
invariant for the lifetime of every `StreamReader` and unreachable guards would
imply a state that can no longer exist. This report's own paired probe result —
`reader-null-read=-1` — was the defect: `-1` and `""` are exactly what an empty
document returns, so a missing stream was indistinguishable from empty content.

The full thirteen-case pre-fix/post-fix measurement, including the five distinct
`StreamWriter` dereferences of which the finding named one, is in the paired
`StreamWriter.cpp` report under the same heading, with logs
`build-probe/1806_prefix_defects.log` and `build-probe/1806_postfix_defects.log`.

## Follow-up #1808 closed for the reader half (2026-07-29)

**SR-AUD-337 stays `confirmed` and SR-AUD-338 stays `remediated`.** No new
`SR-AUD-*` identifier was issued; the numbering stays frozen at 364.

`StreamReader(Stream*, bool)` now also rejects a stream that **exists but declares
itself unreadable**, with `ArgumentException("Stream was not readable.")` — message
only, no `paramName`, matching `StreamReader.cs:145-148` /
`Argument_StreamNotReadable` and byte-identical to `BinaryReader.cpp:25`, which
already did this. The check follows the null check, which is .NET's order and is
required here: testing `getCanReadProperty()` first would dereference the pointer
the null check exists to reject.

**This is the same defect this report already named, one level further out.** The
`reader-null-read=-1` result recorded above was the null case; cases 6 and 7 of
`build-probe/1823_prefix_defects.log` are the unreadable case, measured on a
`FileStream(path, FileMode::Append)` — `FileAccess::Write` only, so `CanRead` is
false. `Read()` returned `-1` and `ReadToEnd()` returned `""`: a stream that can
never be read at all was indistinguishable from an empty document, exactly the
laundering that justified removing the null guards under #1806.

**Only the reader half landed, and the reason is measured rather than assumed.**
#1808 was opened covering both directions, requiring an inventory first so that
"the check cannot reject a stream that is in fact usable". Design ticket #1823
(`docs/TextWrapperInputContractPlan.md` §5) did that inventory and found the two
directions have **opposite** compatibility, from one line:

```cpp
// modules/io/include/System/IO/Stream.hpp:62,65
[[nodiscard]] virtual bool getCanWriteProperty() const { return false; }
[[nodiscard]] virtual bool getCanReadProperty()  const { return true; }
```

.NET's `Stream.CanRead`/`CanWrite` are **abstract**; this port gave them defaults,
and the default for writing is "no". A `CanRead` guard therefore rejects only a
stream that positively declares itself unreadable, but a `CanWrite` guard would also
reject every custom stream that implements `Write()` and never overrode the property
— case 8 of the same probe writes `"hello"` successfully through exactly such a
stream today. The writer half is consequently blocked ticket **#1824**, awaiting the
explicit approval that a mandatory downstream migration needs.

Closure evidence: 10 permanent regressions in `IOStreamTests.cpp` — the unreadable
stream, its exact message, the absence of a `(Parameter '…')` suffix, the
null-before-unreadable ordering, a custom stream that never overrides
`getCanReadProperty()` and must keep working, the concrete write-only `FileStream`,
the valid path in both `leaveOpen` modes, the path constructor, an inertness check
that a rejected construction leaves a neighbouring live stream alone, and a
cross-type assertion against `BinaryReader`. `SharpRuntimeTests_IO` **572/572** (was
562), clean under ASan + UBSan + LSan (`build-asan/1808_io_asan.log`). Repository
gate: 0 warnings, 0 errors, **14,070 tests across 37 executables**.

Source and ABI consequences: none. One behavioural note for consumers: code that
constructed a `StreamReader` over a self-declared-unreadable stream and relied on the
silent empty document now receives `ArgumentException` at construction. No
in-repository caller did so, and no test asserted the old behaviour.

Two further defects this measurement exposed are recorded as their own tickets rather
than folded in: **#1825** (`FileStream::Write` silently discards data written to a
read-only handle — the write never reaches the file and nothing is thrown) and
**#1826** (`MemoryStream::getCanReadProperty()` ignores `isOpen_`, where
`MemoryStream.cs:99` is `CanRead => _isOpen`).
