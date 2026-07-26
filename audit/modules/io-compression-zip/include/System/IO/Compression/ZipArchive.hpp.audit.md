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
