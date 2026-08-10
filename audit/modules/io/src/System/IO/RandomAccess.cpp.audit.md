# Audit: `modules/io/src/System/IO/RandomAccess.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-340 — medium — RandomAccess raw overloads silently accept invalid metadata and POSIX GetLength suppresses failures

The raw pointer/count overloads do not validate pointer, count, or offset.  In particular, the Write loop is skipped for a negative count.  A direct fd probe prints `negative-write=accepted`.  The same probe calls `GetLength(-1)` and prints `invalid-fd-length=-1`: both `lseek` failures are returned as a spurious length instead of becoming `IOException`.  The Read path casts a negative count to `size_t`, and neither platform branch detects zero-byte write progress, so the public contract has inconsistent error behavior.

## Missing assertions and diagnostics

- Existing tests exercise valid regular-file offsets, lengths, and flushes only; they omit invalid descriptors, null buffers, negative counts/offsets/lengths, read-only descriptors, non-seekable handles, and write-zero-progress behavior.
- No test asserts that an OS failure maps to a System I/O exception with the operation/path/errno context needed for diagnosis.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.

## Remediation record — SR-AUD-340 (ticket #2100, 2026-08-04)

**Status: `confirmed` → `remediated`.** The original evidence above is retained verbatim; this
section is appended, not a rewrite.

**The report above is broader than the findings index's one-line summary of it, and the repair
followed the report.** The index summary names only *"negative `Write` counts"* and
*"POSIX `GetLength` returns -1 for invalid descriptors"*, and `docs/SystemIONamespaceReviewPlan.md`
§6.3 inherited that scope. The report itself already named **pointer, count and offset**
validation, **null buffers**, negative **lengths**, **read-only descriptors**, **non-seekable
handles** and **write-zero-progress** — all of which were measured
(`build-probe/2100_probe1_before.log`) and all of which are repaired.

What changed, per door, is tabulated in `docs/SystemIONamespaceReviewPlan.md` §20.3. In summary:

- every negative `count`, `fileOffset` and `length` now raises `ArgumentOutOfRangeException`
  naming the offending parameter, on **both** the read and the write side and in **both** the
  raw-pointer and `std::vector` overloads;
- a null `buffer` with a positive `count` raises `ArgumentNullException("buffer")`; a null buffer
  with `count == 0` stays accepted, because nothing is transferred;
- `GetLength` throws instead of returning `-1`, for an invalid descriptor **and** for a valid but
  non-seekable one (a pipe returned `-1` too — wider than the finding states);
- every native failure carries the operating system's own reason, so `EBADF`, `EINVAL` and
  `ENOSPC` are no longer indistinguishable;
- `EINTR` is retried rather than thrown from mid-loop, and a zero-progress write throws instead
  of spinning the loop forever. **Neither of those two is covered by a deterministic test** —
  signal delivery mid-`pwrite` and a zero-byte `pwrite` on a regular file cannot be forced in
  this environment — and both are recorded as inspection-verified, not as tested.

**Validation:** `build/SharpRuntimeTests_IO` 611 → **626**, zero warnings. Five mutations against
the final shipped source each discriminate (7 / 5 / 4 / 2 / 2 distinct tests), with a clean
unmutated control and a byte-identical restore. ASan + UBSan + LSan clean over 3,900 rejections
and 1,300 acceptances with both controls proven live, `RandomAccess.cpp` compiled *into* the
instrumented translation unit because it is an archive member rather than a header-only body.
`/proc/self/fd` delta **0**.

**The exception types and parameter names are this port's choice**, recorded as such: `/rv` is
absent, so no reference evidence pins what .NET raises at these doors.

**Missing assertions and diagnostics** listed above are now covered, with the two exceptions
named as inspection-verified.
