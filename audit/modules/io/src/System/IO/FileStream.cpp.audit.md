# Audit: `modules/io/src/System/IO/FileStream.cpp`

## Metadata

- Audit status: AUDITED.
- Component: `IO`.
- Validation: `gmake -C build -j4 SharpRuntimeTests_IO && build/SharpRuntimeTests_IO --gtest_color=no` passed 527/527 on 2026-07-27.
- Scope: source, immediate call sites, and the focused test surface were reviewed; this is audit-only evidence with no production or test-source modification.

## SR-AUD-342 — medium — FileStream does not enforce requested access or disposed state consistently

Read/Write capability flags are not checked by operations or property access.  A direct probe opens an existing file read-only, calls Write, and prints `write-to-readonly=accepted`; after Close it prints `can-read-after-close=1` and `length-after-close=1`.  A second probe uses `FileMode::OpenOrCreate` with `FileAccess::Read`; the implementation opens the missing file with output flags and its unchecked Write creates content, printing `created-with-read-only=1` and `read-only-write-content=y`.  This permits actual writes through a read-only public FileStream and leaves closed properties usable instead of producing the expected NotSupported/ObjectDisposed failures.

## Missing assertions and diagnostics

- Tests cover normal modes and some SetLength validation but omit Read-on-write-only, Write/WriteByte-on-read-only, OpenOrCreate+Read, Flush/Length/Position/CanRead/CanWrite after Close, and write-error propagation.
- No test verifies that the requested FileAccess rather than incidental `std::fstream` flags governs all operations.

## Final assessment

AUDITED. The confirmed finding(s) above have reproducible evidence and a focused remediation target.

## Remediation record — ticket #2099 (2026-08-12)

**Status: `confirmed` → `remediated`.** The original evidence above is retained unchanged;
this section records what was measured and what was repaired.

### The finding was three claims with three different fates

1. **`write-to-readonly=accepted` — already repaired, before this ticket.** Ticket #1825 added
   the `canRead_`/`canWrite_` tests to `Read`, `Write` and `WriteByte`. Re-measured for #2099:
   `Write` on a `FileAccess::Read` stream throws `NotSupportedException("Stream does not support
   writing.")`. Not reproducible.
2. **`created-with-read-only=1` — deliberately NOT ticketed.** `FileMode::OpenOrCreate` +
   `FileAccess::Read` still *creates* a missing file. Whether that is wrong is a .NET behaviour
   question, `/rv/tmp/runtime/` is absent, and no repository-contained evidence settles it, so it
   is recorded as an open question (plan §6.2) rather than guessed at. **This is the one part of
   SR-AUD-342 that is closed by exclusion, not by repair.**
3. **`length-after-close=1` / `can-read-after-close=1` — repaired here.** `getCanReadProperty()`
   was already fixed by #1842; the surviving half is the metadata, and it is **six** members, not
   the three the review plan §6.2 predicted.

### Measured before-state (`build-probe/2099_probe1_before.log`)

On a closed stream over a live 5-byte file:

| Member | Before | After |
|---|---|---|
| `getLengthProperty()` | returned `5` — re-`stat`s `path_`, so a *closed* stream still reported the file's length | `ObjectDisposedException` |
| `getPositionProperty()` | returned **`-1`**, a sentinel | `ObjectDisposedException` |
| `setPositionProperty(0)` | succeeded silently | `ObjectDisposedException` |
| `Seek(0, Begin)` | returned `0` | `ObjectDisposedException` |
| `Seek(0, End)` | returned `5` | `ObjectDisposedException` |
| `Seek(0, Current)` | **`IOException`** — see below | `ObjectDisposedException` |
| `Flush()` | succeeded silently | `ObjectDisposedException` |
| `setPositionProperty(-1)`, `SetLength(-1)` | `ArgumentOutOfRangeException` — argument checked **before** the closed state | `ObjectDisposedException` |
| `Read`, `Write`, `WriteByte`, `SetLength(0)` | already `ObjectDisposedException` | unchanged |

### Two premise corrections

- **Plan §6.2 says "`Seek()` succeeds outright".** True for `Begin` and `End` only. For
  `SeekOrigin::Current` it *threw* — `IOException("An attempt was made to move the position
  before the beginning of the stream.")` — because `getPositionProperty()`'s `-1` does not stay
  local: `Stream::Seek` adds it to the offset and rejects the negative sum. The closed stream
  therefore reported a complaint **about the seek target**. A bare `EXPECT_THROW` would have
  passed against the old code; only the exception *type* discriminates.
- **Two members checked their argument before the closed state**, the same reversal #2108 found
  in `UnmanagedMemoryStream::SetLength`. The plan named neither.

### The repair, and why it needed no approval

One private `EnsureNotClosed()`, routed through by **every** dependent member including the four
that were already correct, so they cannot drift apart again. `FileStream` needs no `closed_`
flag: `file_.is_open()` already **is** one, `Close()` already clears it, and the three capability
properties already consulted it. **`sizeof(FileStream)` is 576 before and after and `alignof` 8**,
so this is layout-neutral and did **not** depend on #2098's Approval IO-1 — the same split
#2108 made for `UnmanagedMemoryStream`.

### One residue, pinned rather than removed

`FileStream` deliberately does **not** override `Seek`. `Stream::Seek` tests a negative *resulting*
position before delegating, so on a closed stream `Seek(-1, Begin)` still reports `IOException`.
Measured (`build-probe/2099_probe2_seek_residue.log`), `UnmanagedMemoryStream` — repaired by
#2108 — behaves **identically**. Overriding `Seek` in `FileStream` alone would create a divergence
between two siblings that agree today, so the residue is pinned by test. Changing the shared
`Stream::Seek` ordering is a separate question and is not this ticket.

### Gates

+9 permanent regressions (`SharpRuntimeTests_IO` 635 → 644), add-only. Five implementation
mutations discriminate 4/1/4/3/1 distinct tests with a clean control and a byte-identical restore.
ASan+UBSan+LSan clean over the changed code compiled from source. Descriptor delta **0** across
100 closed-stream rejection cycles (reported, per the ticket's acceptance criteria). No signature,
object-layout, vtable, mangled-symbol or `noexcept` change.
