# Audit: `modules/io-compression-zip/src/System/IO/Compression/ZipArchive.cpp`

## Metadata

- AUDITED: miniz state, entry streams, reader/writer conversion, update carry
  forward/delete, path and stream constructors, and Dispose/destructor.
- Validation: ZIP integration focus passed 38/38; native null-stream process
  probe and current .NET comparison were run.

## SR-AUD-242 — high — a null Stream crashes read/update construction and is silently accepted for Create

The stream constructor unconditionally evaluates `stream->Read` for Read and
Update.  The native null Read-mode subprocess terminates SIGSEGV (exit 139).
For Create it stores null and later skips write-back because `state_->stream`
is null, without an entry-boundary diagnostic.  Current .NET validates stream
at construction and throws ArgumentNullException.  See the public ZipArchive
report for the exact probe output.

## Assessment

The implementation contains substantive previous fixes: guarded write offsets,
safe zero-byte miniz buffers, update preservation/deletion, stream write-back,
and non-terminating destructor cleanup.  It nevertheless lacks fundamental
input validation before dereferencing/storing its public raw stream pointer.

## Other missing assertions and diagnostics

- Add SR-AUD-242 regressions in every mode and process-isolated crash handling.
- Test malformed archives/entries, extraction failure while Update must retain
  all prior entries, memory/Int32 size bounds, duplicate names, null write
  buffers, finalization failures/retry, and all stream capabilities/lifetimes.
- Provide a diagnostic before skipping an unreadable existing update entry;
  current best-effort omission needs an explicit parity decision and test.

## Final assessment

SR-AUD-242 is directly reproduced. No source or test was changed during this audit.
