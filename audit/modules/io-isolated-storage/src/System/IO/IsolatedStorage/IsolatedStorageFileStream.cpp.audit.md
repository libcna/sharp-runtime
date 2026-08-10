# Audit: `modules/io-isolated-storage/src/System/IO/IsolatedStorage/IsolatedStorageFileStream.cpp`

## Metadata

- AUDITED: parent directory preparation, FileStream construction, and
  Emscripten close synchronization hook.
- Evidence: public stream and IsolatedStorageFile call path were inspected.

## Assessment

The implementation creates missing parent directories then delegates normal
stream behavior to FileStream.  On Emscripten it asynchronously requests IDBFS
sync and logs failures to the browser console.  It trusts its fullPath caller,
so the IsolatedStorageFile containment failure propagates here (SR-AUD-241).

## Other missing assertions and diagnostics

- Add parent creation/error and contained-path tests (SR-AUD-241), file-mode
  coverage, close flushing, Emscripten sync failure reporting, and a no-write
  guarantee after stream/store close.

## Final assessment

No independent implementation defect was demonstrated. No source or test was
changed during this audit.


---

## Correction appended 2026-08-10 (#2204)

*The original text above is unchanged.*

The propagated containment failure this report names is closed at its source:
`IsolatedStorageFile::OpenFile`/`CreateFile` now validate the caller's relative path before
constructing a stream, so the parent-directory preparation here can no longer run outside the
store. Measured: a rejected open creates no parent directories at all.

`IsolatedStorageFileStream`'s **own public constructor remains unconfined by design** -- it takes
a full path and opens it anywhere. That is now stated as a `@warning` in its header rather than
left implicit, and a test pins the behaviour so the day #2208 (a public signature change taking
the owning store) ships, the pin inverts instead of passing silently.
