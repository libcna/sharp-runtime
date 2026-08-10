# Audit: `tests/integration/System/IO/Compression/CompressionTests.cpp`

## Metadata

- Audit status: AUDITED (1,371 lines, 115 tests in 20 suites, full read).
- Runtime evidence: the non-filesystem compression filter passed all 88 tests
  in 15 suites on 2026-07-25: stream modes/validation/lifetime, wrappers,
  streamless encoders/decoders, options, and enum/exception paths.
- File-backed ZIP archive/file/extension cases were deliberately not rerun in
  this session because their fixed `/tmp` targets already existed and their
  test code recursively removes or overwrites those names.  This is a test
  isolation finding, not a source-validation failure.

## Coverage observed

The stream tests have unusually valuable negative coverage for null buffers,
negative offsets/counts, thrown inner writes, destructor exception containment,
and GZip/ZLib framing.  Encoder/decoder tests cover a destination-too-small
status, disposal, validation, and the >2 GiB bound calculation.  ZIP tests
cover archive update preservation/deletion and a Zip Slip traversal regression.

## SR-AUD-014 — medium — file-backed compression tests use non-unique, destructive `/tmp` paths

The ZIP archive/file/extension tests repeatedly use hard-coded paths such as
`/tmp/sharpruntimetest.zip`, `/tmp/sharpruntimetest_update_preserve.zip`, and
`/tmp/sharp_rt_zipfile_src`.  `RemoveAll` calls `std::filesystem::remove_all`
on those exact names before and after cases, while several `ZipArchive` tests
overwrite paths without cleanup.  A read-only inventory before this focused run
found seven such `sharpruntimetest*` artifacts already present.

This makes the tests non-isolated: concurrent jobs/processes can collide, a
failed run leaves state for the next run, and a file that happens to use one of
the names can be removed or overwritten.  It also prevented safe rerunning of
the file-backed subset in this audit without first deciding ownership of the
existing artifacts.

### Required post-audit verification

Generate one unique directory with `std::filesystem::temp_directory_path()`
plus a process/random suffix, create every archive/source/destination below
it, and remove only that owned directory through a scope guard.  Run the whole
compression filter twice and in parallel, then assert no owned temp artifacts
remain.  Do not delete the pre-existing `/tmp` artifacts as part of the audit.

## Other missing assertions and diagnostics

- Most stream round trips perform one `Read`; they do not force short reads,
  chunked writes, concatenated members, trailing garbage, corrupt headers/CRC,
  truncated input, or `leaveOpen` ownership behavior.
- `ZipArchive` test fixtures rely on the bundled `miniz` writer and local
  round trips.  They should add independently produced ZIP fixtures for data
  descriptors, UTF-8/non-ASCII names, directory entries, unsupported methods,
  duplicate names, corrupt central directories, and ZIP64 limits.
- The destructive-path test proves `../` rejection but not absolute paths,
  separator variants, symlink escapes, collision policy, or cleanup after an
  extraction failure.
- `GetMaxCompressedLength` has a good large-input regression but no exact
  boundary/overflow diagnostic coverage for all encoder flavors.

## Final assessment

The non-filesystem subset passed 88/88 and provides strong safety regression
coverage.  File-backed test hygiene is materially unsafe and must be isolated
before the full compression suite is treated as reproducibly runnable.
