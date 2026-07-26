# Audit: `modules/io/src/System/IO/FileSystemWatcher.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-339 — medium — changing Path while a FileSystemWatcher is enabled retains the old inotify watch and reports the new path

`setPathProperty` only assigns `directory_`; it does not stop and recreate the inotify watch.  The watch loop later uses the mutable new `directory_` to construct callback arguments.  A controlled two-directory probe enables a watch on `first`, changes Path to `second`, creates `first/old-watch.txt`, and prints `event-fired=1` with `reported-full-path=/tmp/sharp-runtime-io-audit/fsw-path-change/second/old-watch.txt`.  It thus watches one directory and reports another.

## SR-AUD-346 — medium — FileSystemWatcher stores NotifyFilter but never applies it to inotify registration or dispatch

The Linux backend always requests a fixed mask and dispatches every supported event type without consulting `notifyFilter_`.  A direct probe selects only `NotifyFilters::Size`, creates a file, and prints `size-only-created-event=1`; a creation/name event is delivered despite FileName not being requested.

## Missing assertions and diagnostics

- Existing Linux end-to-end tests cover create/modify/delete/rename/filter and shutdown, but omit Path, NotifyFilter, Filters, IncludeSubdirectories, and InternalBufferSize changes while enabled (SR-AUD-339/346).
- There are no overflow/error-scope tests, no callback registration/removal concurrency tests, and no exact event ordering/coalescing diagnostics.
- The public mutable handler/filter vectors and live configuration are accessed by the watcher thread without synchronization; this audit records the confirmed functional errors above, while race stress remains a follow-up validation gap.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.
