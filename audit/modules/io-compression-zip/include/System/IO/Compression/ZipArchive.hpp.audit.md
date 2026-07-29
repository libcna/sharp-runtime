# Audit: `modules/io-compression-zip/include/System/IO/Compression/ZipArchive.hpp`

## Metadata

- AUDITED: archive/entry API, modes, raw Stream ownership contract, read/create/update
  semantics, and disposal documentation.
- Validation: direct native subprocess probe plus current .NET comparison;
  focused ZIP integration fixture passed 38/38.

## SR-AUD-242 — high — a null Stream crashes read/update construction and is silently accepted for Create

The public `Stream*` constructor has no null boundary check.  The native
`ZipArchive(nullptr, Read)` probe dies with SIGSEGV and exit 139 at the first
`stream->Read`; Create mode stores a null pointer and can finalize only an
internal buffer, silently producing no caller-visible output.  Current .NET
performs `ArgumentNullException.ThrowIfNull(stream)` at constructor entry; its
matching probe prints `null_stream=exception:System.ArgumentNullException`.

## Assessment

The documented non-owning stream lifetime is a visible C++ adaptation, and
the integration fixture covers ordinary creation/update/readback and write
failure propagation.  Null input has no safe contract, however, and one public
mode turns it into a process crash rather than a required diagnostic.

## Other missing assertions and diagnostics

- Add null Stream coverage for Read, Create, and Update (SR-AUD-242), invalid
  mode values, read/write/seek capability combinations, disposed streams, and
  short/negative/throwing Read results.
- Add duplicate/empty/invalid entry names, large entry limits, concurrent entry
  streams, open-after-dispose, corrupt existing-entry update, and failed
  finalization recovery tests under ASan/UBSan.

## Final assessment

SR-AUD-242 is directly reproduced. No source or test was changed during this audit.

## Post-audit remediation for SR-AUD-242 (ticket #1812, 2026-07-29): REMEDIATED

The audit evidence above is retained unchanged.

Ticket #1812 (`REMED-IO-ZIP-NULL-STREAM`, P1, size S) rejects a null
`System::IO::Stream*` at `ZipArchive`'s stream constructor with
`ArgumentNullException("stream")`, in **every** mode, matching .NET, whose
`Stream`-taking `ZipArchive` constructors all funnel into
`ZipArchive(Stream, ZipArchiveMode, bool, Encoding?)` and open with
`ArgumentNullException.ThrowIfNull(stream)`.

Reproduced before any production change, one process per case
(`build-probe/1812_prefix_defects.cpp`, logs `build-probe/1812_prefix_defects.log`
and `build-probe/1812_postfix_defects.log`):

| Case | Input | Pre-fix | Post-fix |
|---|---|---|---|
| 1 | `ZipArchive(nullptr, Read)` | **ASan SEGV on 0x0**, exit 1 | `ArgumentNullException` |
| 2 | `ZipArchive(nullptr, Update)` | **ASan SEGV on 0x0**, exit 1 | same |
| 3 | `ZipArchive(nullptr, Create)` | constructed | same |
| 4 | `ZipArchive(nullptr, Create)` + `CreateEntry` + write + `Dispose()` | completed — **and delivered nothing** | same |
| 5 | `ZipArchive(nullptr, Create)` destruction alone | completed | same |
| 6 | `ZipArchive(nullptr, (ZipArchiveMode)42)` | constructed | now rejected on the null, before the mode |
| 7 | the valid Create path over a real `MemoryStream` | `length=146` | **byte-identical** |
| 8 | the valid Read path over case 7's archive | `entries=1 name=payload.txt length=5` | **byte-identical** |

**The two halves of this finding are not symmetric, and the asymmetry is the
reason the guard is unconditional.** Read and Update dereference the pointer
inside the constructor itself, so they fail loudly. Create does not crash at all:
it stores the null pointer, and then every call the caller makes succeeds —
`CreateEntry`, the entry write stream, `Dispose()`. The finalized archive lands in
`state_->memBuf`, and `Dispose()`'s write-back is gated on
`state_->stream != nullptr`, so the archive is silently discarded with no
diagnostic of any kind. Case 4 wrote a complete one-entry archive and delivered it
nowhere. Silent data loss is the worse failure mode of the two, so the check is
not restricted to the modes that crash.

**One separate defect was found while reproducing this one and was deliberately
not folded in.** Case 9, added to the probe after the fix landed (a null stream no
longer reaches the mode at all), constructs `ZipArchive(&realStream,
(ZipArchiveMode)42)` successfully — .NET's `ValidateMode` throws
`ArgumentOutOfRangeException(nameof(mode))` for any value outside
Read/Create/Update. That is a different public contract, it carries **no
`SR-AUD-*` identifier** (the audit recorded it only as a missing-test note above,
not as a finding), and it is tracked as inactive ticket **#1813** rather than
widening this one. The audit numbering stays frozen at 364.

Closure evidence: **8 new permanent regressions** in the ZIP integration fixture
(`tests/integration/System/IO/Compression/CompressionTests.cpp`) — all three
modes, the defaulted-mode overload, the parameter name, repeatability of the
rejected construction, an unaffected valid Create/Read round-trip, and the
path-based constructor overload proving it still produces
`InvalidDataException` rather than the new guard. The ZIP fixture is **44/44**
(was 36), and the same 44 under AddressSanitizer + UndefinedBehaviorSanitizer +
LeakSanitizer with **zero reports** (`build-asan/1812_zip_asan.log`). Repository
gate: 0 warnings, 0 errors, **13,987 tests across 37 executables** (was 13,979).

Source and ABI consequences: none. No public signature, object layout, vtable or
exported symbol changed. Behavioural note for consumers: constructing a
`ZipArchive` over a null stream now throws instead of crashing (Read/Update) or
silently discarding output (Create). No in-repository caller did so.
