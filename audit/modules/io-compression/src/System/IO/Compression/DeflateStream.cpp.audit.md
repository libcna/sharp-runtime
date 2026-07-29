# Audit: `modules/io-compression/src/System/IO/Compression/DeflateStream.cpp`

## Metadata

- AUDITED: raw Deflate stream lifecycle, zlib I/O, and inner-stream ownership.
- Validation: ASan/UBSan native probe and current-.NET constructor/lifecycle
  probe.

## Assessment

Nominal round trips and pre-zlib buffer validation pass. Constructor and closed
state guards are absent, so invalid public states are stored and later ignored
or dereferenced.

## SR-AUD-257 — high — compression streams accept a null inner stream and later dereference it

`DeflateStream(nullptr, Compress, true)` constructs successfully; a sufficiently
large incompressible Write reaches `inner_->Write` and UBSan reports member
access through null (the isolated native process exits 139). Current .NET
prints `nullStream=ArgumentNullException`. GZipStream and ZLibStream share the
same unguarded constructor/Write shape.

## SR-AUD-258 — medium — compression streams silently accept invalid modes and post-close operations

A native `(CompressionMode)42` constructor succeeds, creates a deflater, and
reports both CanRead and CanWrite false; current .NET prints
`invalidMode=ArgumentException`. After `Close`, native Write returns silently,
where .NET prints `afterClose=ObjectDisposedException`. The three stream
implementations duplicate this mode/closed-state pattern.

## Other missing assertions and diagnostics

- Reject null/invalid mode at construction; enforce disposed state on all
  properties/I/O/Flush; cover leave-open, mode misuse, corrupted frames, and
  throwing inner close/write paths.

## Final assessment

SR-AUD-257 and SR-AUD-258 are confirmed. No source or test changed.

## Post-audit remediation for SR-AUD-257 (ticket #1811, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged. **SR-AUD-258 is untouched and
stays `confirmed`** — this ticket repaired the null inner stream only, not the
invalid-`CompressionMode` and post-close-operation contracts that share these
files.

Ticket #1811 (`REMED-IO-COMPRESSION-NULL-INNER-STREAM`, P1, size S) rejects a
null `Stream*` in `DeflateStream`, `GZipStream` and `ZLibStream` with
`ArgumentNullException("stream")`, matching .NET, whose every `Stream`-taking
constructor of all three types opens with
`ArgumentNullException.ThrowIfNull(stream)`.

Reproduced before any production change, one process per case
(`build-probe/1811_prefix_defects.cpp`, log `build-probe/1811_prefix_defects.log`),
covering all three types symmetrically rather than only the one the finding named:

| Cases | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1–3 | `T(nullptr, Compress, true)` then a 256 KiB incompressible `Write` | **ASan SEGV on 0x0**, exit 1, for all three types | `ArgumentNullException` at construction |
| 4–6 | `T(nullptr, Decompress, true)` then `Read` | **ASan SEGV on 0x0**, exit 1, for all three types | same |
| 7–9 | `T(nullptr, Compress, false)` then `Close()` | completed | same |
| 10–12 | `T(nullptr, Compress, false)` destruction alone | completed | same |
| 13 | the valid path over a real `MemoryStream` | `length=6` | **byte-identical** |

The finding named `Write`; the `Decompress`-mode `Read` path crashes identically
and is recorded here as the second half of the same defect.

**The payload shape matters and is now pinned by a test.** A small compressible
write does **not** reproduce this: zlib absorbs it into the 64 KiB deflate buffer
and never touches the inner stream. That is why the finding specifies "a
sufficiently large incompressible Write", and it is why validating at
*construction* rather than at the first write is what actually closes the hole —
a check on the write path would still leave a constructed object whose inner
stream does not exist.

**The check is placed before zlib initialisation, deliberately.**
`deflateInit2`/`inflateInit2` allocate state that only `deflateEnd`/`inflateEnd`
release, so throwing after them would leak it. `state_` is a `std::unique_ptr` and
unwinds on its own.

**Cases 7–12 did not crash, and the reason must not be "tidied away".** `Close()`
tests `if (produced > 0 && inner_)` and the non-`leaveOpen` path tests
`if (!leaveOpen_ && inner_)`. Those guards look like the ones ticket #1806 removed
from `StreamReader` as unreachable, but they are **not** unreachable here:
`Close()` itself assigns `inner_ = nullptr` after closing a non-`leaveOpen` inner
stream, so a null `inner_` is a genuine post-close state in these classes. They
stay, and a comment in each constructor says so.

Closure evidence: 9 new permanent regressions in `CompressionTests.cpp` — both
compression modes rejected for each of the three types, the owning
(`leaveOpen = false`) shape whose destructor and `Close()` also touch the inner
stream, the parameter name, and a large incompressible round-trip that exercises
the exact flush path the crash came from. `SharpRuntimeTests_IO_Compression`
**31/31** (was 22), and the same 31 under AddressSanitizer +
UndefinedBehaviorSanitizer + LeakSanitizer with zero reports
(`build-asan/1811_io_compression_asan.log`). Repository gate: 0 warnings, 0
errors, **13,979 tests across 37 executables** (was 13,970); the `IO.Compression`
and `IO.Compression.Zip` selective consumer fixtures both passed. Doxygen 1,941 of
the 1,942 ceiling, unchanged.

Source and ABI consequences: none. No public signature, object layout, vtable or
exported symbol changed. Behavioural note for consumers: constructing any of the
three over a null stream now throws instead of deferring a crash to first use. No
in-repository caller did so.
