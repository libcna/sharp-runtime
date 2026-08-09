<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::IO::Compression` namespace review and remediation plan

Ticket **#2147**, written 2026-08-09 on branch
`claude/remediation-batch-1804-namespace-b1yjh5`.

The seventeenth namespace review in the post-audit remediation programme, after `System::Threading`
(#1950), `Threading::Tasks`/`Channels` (#1964), `System::Runtime` (#1972), `System::Uri` (#1987),
`System::Text` (#2006), `System::Diagnostics` (#2023), `System::Net` (#2034), `Buffers` (#2054),
`Net::Http` (#2062), `System::Xml` (#2074), `Net::WebSockets` (#2087), `System::IO` (#2098),
`Text::Json` (#2109), `Net::Http::Headers` (#2124), `Net::Sockets` (#2133) and
`IO::Hashing` (#2140).

Same contract as its sixteen predecessors: **every confirmed finding in the module gets exactly one
disposition, no finding disappears between the audit index and this plan, and every premise is
re-measured against the shipped library before it is relied upon.**

**Nothing in §§1–14 is implemented by writing them.** The measured before-matrix is
`build-probe/2146_probe1_before.log` (source `2146_probe1_compression_lengths.cpp`) and
`build-probe/2146_probe2.log` (source `2146_probe2_masking.cpp`).

**No `SR-AUD-*` identifier is issued by this review.** Audit numbering stays frozen at **364**.

---

## 1. Why `modules/io-compression` is next — the selection, re-derived

Re-derived by parsing `audit/AUDIT_FINDINGS_INDEX.md` on 2026-08-09, **not** inherited from any
earlier handoff. Every module with ≥ 2 open confirmed findings:

| Module | Open | high | med | low | high % | Reviewed? | `/rv`-bound? | Verdict |
|---|---|---|---|---|---|---|---|---|
| `core` | 72 | 9 | 59 | 4 | 12 % | partly, by family | mixed | **poor cohesion** — not a namespace; already carved by seven `CCF-*` family plans |
| `globalization` | 7 | 1 | 6 | 0 | 14 % | no | **heavily** | grapheme clusters, culture collation/casing, IDN, unknown-culture metadata — needs `/rv` **and** ICU data, neither present |
| `time-zone` | 7 | 0 | 7 | 0 | **0 %** | no | **mostly** | 3 of 7 need real tz-database + .NET parity; zero high |
| `numerics` | 4 | 0 | 4 | 0 | 0 % | no | partly | zero high; one finding is a missing-definition link error |
| `xml-linq` | 4 | 1 | 3 | 0 | 25 % | no | no | its **only** high (SR-AUD-333) is **CCF-019**, which is open and blocked (#1899) |
| **`io-compression`** | **3** | **1** | **2** | 0 | **33 %** | **no** | **none** | **winner** — see below |
| `net-network-information` | 3 | 0 | 3 | 0 | 0 % | no | no | zero high; its executable cannot pass here (#1962 capability gap) |
| `console` | 2 | 0 | 2 | 0 | 0 % | no | no | zero high; two argument-validation items |
| `security-cryptography` | 2 | 2 | 0 | 0 | **100 %** | no | no | strong severity, but only 2 findings and both are `Dispose`-clearing questions in a module CLAUDE.md partly excludes |
| `timers` | 2 | 1 | 1 | 0 | 50 % | no | no | strong, tiny; the natural **next** unit after this one |

`io-compression` wins on the rule the previous sixteen reviews used, and on the `/rv` penalty this
batch was told to apply explicitly:

- **Severity's character.** Its one high, SR-AUD-256, is **memory unsafety with ASan evidence** —
  a negative public length cast into an unbounded native zlib read. That is the highest-consequence
  actionable finding in any unreviewed module whose high is not already blocked. `xml-linq`'s high
  *is* blocked (CCF-019); `globalization`'s is a process-global-state race needing an
  architectural approval.
- **Zero `/rv` dependence.** All three findings are locally decidable: a buffer over-read is a
  measurement, an out-of-domain enum cast is a domain question, and the strategy finding is
  observable by comparing this library's own output between two option values. `globalization` and
  `time-zone` — the only larger unreviewed candidates — are the two whose useful work is
  *predominantly* unverifiable reference parity with `/rv` absent.
- **Public/remote input exposure.** This module decompresses **attacker-supplied bytes**. It is the
  only unreviewed candidate whose ordinary job is parsing hostile input.
- **An established idiom already exists.** The `IO::Hashing` batch (#2141/#2142) just landed the
  exact repair shape this module needs — a module-local `Detail::ValidateLength` choke point
  throwing `ArgumentOutOfRangeException("length", "Non-negative number required.")` — so the
  compatible ratio is high and the design question is already settled.
- **Cohesion and size.** One module, one namespace, 14 headers / 9 bodies / 1 test file, three
  findings. A completable unit.

---

## 2. Scope and file inventory

Component **`IO.Compression`** → target `sharp_runtime_io_compression`,
`PUBLIC_DEPENDENCIES Buffers Core.Base IO`, plus a private `ZLIB::ZLIB` link.

| Kind | Count | Notes |
|---|---|---|
| Public headers | 14 | 6 codec types, 3 stream types, 3 option/enum types, 1 exception, 1 mode enum |
| Implementation bodies | 9 | the 3 option/enum types and the exception are header-only |
| Test translation units | **1** | `tests/CompressionTests.cpp` — the whole module's coverage |

### 2.1 Public raw-pointer surface (the SR-AUD-256 door set)

| Type | Doors taking `(ptr, intcs)` |
|---|---|
| `DeflateEncoder` | `Compress`, `Flush`, `TryCompress` ×3 overloads |
| `GZipEncoder` | `Compress`, `Flush`, `TryCompress` ×3 — **all delegate to `DeflateEncoder`** |
| `ZLibEncoder` | `Compress`, `Flush`, `TryCompress` ×3 — **all delegate to `DeflateEncoder`** |
| `DeflateDecoder` | `Decompress` |
| `GZipDecoder` | `Decompress` |
| `ZLibDecoder` | `Decompress` |

---

## 3. Confirmed finding inventory — all 3, with measured current behaviour

| ID | Sev | Owner | Defect | Cause |
|---|---|---|---|---|
| **SR-AUD-256** | **high** | `DeflateEncoder.cpp`, `DeflateDecoder.cpp`, GZip/ZLib wrappers | public `intcs` lengths are cast to `uInt` with no validation; a negative length becomes a huge count and zlib reads/writes past the caller's allocation | **C-A** |
| **SR-AUD-258** | med | the three stream types, `CompressionMode.hpp` | a cast `CompressionMode` 42 constructs a deflater while **both** capability properties report false; `Write` after `Close` silently returns | **C-B** |
| **SR-AUD-259** | med | the three encoders, `ZLibCompressionOptions.hpp` | every options constructor discards `getCompressionStrategyProperty()` and hard-codes `Z_DEFAULT_STRATEGY`; the three stream types have no options constructor at all | **C-C** |

---

## 4. Corrections to the audit record — measured, not inferred

Four, and the first two change how many doors the repair must cover.

### 4.1 The source-side defect is in **all three** encoders, not only `DeflateEncoder`

`build-probe/2146_probe1_before.log` appears to show `GZipEncoder` and `ZLibEncoder` returning
**normal** for `sourceLength = -1` while only `DeflateEncoder` crashes. **That is an artefact of the
probe's own 1-byte destination**, and taking it at face value would have under-scoped the repair to
one type.

`build-probe/2146_probe2.log` isolates it. A gzip or zlib stream must emit a **header** before any
input is read; with a 1-byte destination `deflate()` fills that byte and returns before it ever
touches the source. Raw deflate has no header, so it reads immediately. Give the identical call a
destination large enough to absorb the header and the masking disappears:

| Encoder | `srcLen=-1`, dst = 1 byte | `srcLen=-1`, dst = 4096 bytes |
|---|---|---|
| `DeflateEncoder` | **CRASHED** | **CRASHED** |
| `GZipEncoder` | normal | **CRASHED** |
| `ZLibEncoder` | normal | **CRASHED** |

All three carry the identical unguarded cast, because `GZipEncoder::Compress` and
`ZLibEncoder::Compress` are two-line forwarders to `DeflateEncoder::Compress`. **The destination
size decides which of the two over-runs fires first, not whether the defect is present.**

### 4.2 The three decoders do **not** crash — and that is luck, not a guard

The audit says "DeflateDecoder and the GZip/ZLib encoder/decoder wrappers expose the same unguarded
signed pointer-length boundary". The boundary is indeed identical in the source — `Decompress` casts
both lengths the same way — but **all 21 decoder cases returned `normal`**, including
`srcLen=INTCS_MIN`, `dstLen=INTCS_MIN`, and both null-buffer cases. `inflate()` rejects the
one-byte garbage input with `Z_DATA_ERROR` before it consumes the impossible `avail_in`.

That is not safety. Two consequences survive and must be repaired on their own terms:

- the same call on **valid compressed input** has no such early exit;
- `bytesConsumed = sourceLength - static_cast<intcs>(zs.avail_in)` with a negative `sourceLength`
  computes a meaningless value and reports it to the caller as a byte count.

### 4.3 Null-buffer handling already differs between encoders and decoders

Unrecorded by the audit. Every **encoder** null-buffer case **threw** (6 of 6). Every **decoder**
null-buffer case returned **normal** (6 of 6), handing `inflate()` a null `next_in`/`next_out`.
The repair must therefore *add* null validation to the decoders and *preserve* the encoders'
existing behaviour rather than assume a uniform starting point.

### 4.4 The measured matrix

63 cases, **15 crashes**, 15 throws, 33 normal — all with a 1-byte destination. §4.1 shows the
crash count is a **lower bound**: widening the destination converts two more encoder families from
"normal" to "crashed".

| Group | Cases | Crashed | Threw | Normal |
|---|---|---|---|---|
| `DeflateEncoder` | 14 | **7** | 4 | 3 |
| `GZipEncoder` | 14 | **4** | 4 | 6 |
| `ZLibEncoder` | 14 | **4** | 4 | 6 |
| `DeflateDecoder` | 7 | 0 | 0 | 7 |
| `GZipDecoder` | 7 | 0 | 0 | 7 |
| `ZLibDecoder` | 7 | 0 | 0 | 7 |

Source-side crashes: **3** (all `DeflateEncoder`, before §4.1's unmasking). Destination-side
crashes: **12**. The controls — valid 1→256 compression and `srcLen=0` — are `normal` in all six
types and must stay so.

---

## 5. Root causes

### C-A — a signed public length is cast to an unsigned native count without validation (SR-AUD-256)

The single line `zs.avail_in = static_cast<uInt>(sourceLength)` and its destination twin. Identical
in shape to **SR-AUD-261** (`IO::Hashing`, closed by #2142) and to **SR-AUD-264**
(`Net::Sockets`, closed by #2135): a negative `intcs` reaching an unsigned native API. This is a
**repeat of an already-repaired repository idiom**, which is why it is compatible and needs no new
design.

Not minted as a `CCF-*`: the cross-cutting numbering is closed, and #2131 (CCF-021) and #2109
(CCF-022) remain the only unminted candidates. This is recorded as a **third site of the negative-
length idiom** in the audit cross-cutting file instead.

### C-B — an out-of-domain enum reaches native construction (SR-AUD-258)

`CompressionMode` is not validated by any of the three stream constructors. Same cause as
`System::Threading`'s **T-C** (#1954), `System::Runtime`'s **R-F** (#1976) and `System::Uri`'s
**U-E** (#1992); the policy those three settled applies verbatim.

### C-C — a stored option never reaches the native encoder (SR-AUD-259)

`ZLibCompressionOptions::getCompressionStrategyProperty()` is validated on the way **in** and then
dropped on the way **out**. Adding stream options constructors is a **public surface addition** and
is separated from the strategy plumbing for that reason.

---

## 6. Compatible / blocked / deferred matrix

| Cause | Ticket | Classification | Approval |
|---|---|---|---|
| C-A | **#2146** | **compatible — implement now** | none |
| C-B | **#2148** | compatible | none |
| C-C strategy plumbing | **#2149** | compatible | none |
| C-C stream options constructors | **#2150** | **blocked — public surface addition** | required |
| disclosure + pins | **#2151** | compatible, mandatory closing ticket | none |

---

## 7. Source / ABI / layout consequences

| Ticket | Signature | vtable | layout | `noexcept` | Accepted input | Observable result |
|---|---|---|---|---|---|---|
| #2146 | — | — | — | — | negative and null now rejected | `ArgumentOutOfRangeException` / `ArgumentNullException` replaces UB |
| #2148 | — | — | — | — | out-of-domain mode rejected | `ArgumentOutOfRangeException` at construction |
| #2149 | — | — | — | — | — | RLE/Fixed strategies now change the emitted bytes |
| #2150 | **new ctors** | — | — | — | — | additive |

#2149 is the only one that changes **produced bytes**, and only for a caller that explicitly set a
non-default strategy — today that caller silently gets default output.

---

## 8. Test and sanitizer matrix

| Cause | Tests required | ASan | UBSan | LSan | TSan |
|---|---|---|---|---|---|
| C-A | the full 63-case matrix, both destination sizes, plus exact-size / one-short / zero-length / valid controls, on all six types | **required** — the finding *is* an ASan report | required — the cast itself | — | — |
| C-B | all three streams × cast mode × both capability properties; `Write` after `Close` | — | — | — | — |
| C-C | each strategy value produces its own output; round-trip still decompresses | — | — | — | — |

**Round-trip correctness is mandatory for every ticket**: compress → decompress must return the
original bytes for empty, 1-byte, short-ASCII, NUL-bearing, block-boundary and multi-block inputs.
**No repair may change the compressed bytes for default options** — that is the discriminating
control separating a bounds fix from an output-corrupting one.

---

## 9. Execution order

**#2146 → #2148 → #2149 → #2151**, with #2150 blocked throughout. #2146 first: it is the only
memory-safety item, and the other two touch the same files.

---

## 10. Exclusions

1. **`modules/io-compression-zip`** is a separate component with its own findings; not in scope.
2. **zlib itself** (`vendor/`) is third-party and is not modified.
3. **Compression ratio / performance parity** with .NET is not a correctness question and no
   finding claims it.
4. **#2150's stream options constructors** — public surface addition, blocked.

---

## 11. Completion criteria

1. All three findings `remediated`, or `confirmed` with a completed design and a named blocked
   ticket.
2. No public raw-pointer door in the module reaches zlib with an unvalidated length or buffer.
3. The 63-case matrix reports **zero** crashes at both destination sizes, with every control
   unchanged.
4. Round-trip and default-output byte-identity pinned.
5. `SharpRuntimeTests_IO_Compression` grows monotonically.

---

## 12. Status

Written 2026-08-09. §§1–11 changed no production source. **#2146 was implemented in the same
batch**; its record is §13. #2148's record is §14.

Superseded by §14: the sentence "#2148/#2149/#2151 are `todo`" describes the state at the moment
this plan was written, not the current one.

---

## 13. Implementation record — #2146 (cause C-A, SR-AUD-256)

**Repair.** One module-local choke point, `Detail::ValidateSource` / `Detail::ValidateDestination`
(`CompressionArgumentValidation.hpp`), applied at `DeflateEncoder::Compress`,
`DeflateEncoder::Flush` and all three decoders' `Decompress`. That covers the whole module: the
GZip/ZLib `Compress`/`Flush` are two-line forwarders and all nine `TryCompress` overloads funnel
through `DeflateEncoder::Compress`. Validation runs **before** the output parameters are zeroed, so
a rejected call leaves the caller's `bytesConsumed`/`bytesWritten` untouched rather than looking
like a successful empty operation.

The rule, exception types and messages are **identical to `System::IO::Hashing::Detail`**
(#2141/#2142), deliberately: two sibling components answering "what does a negative length mean"
two different ways is how a repository ends up with two contracts.

**Before / after.**

| Matrix | Cases | Crashed | Threw | Normal |
|---|---|---|---|---|
| before | 63 | **15** | 15 | 33 |
| after | 63 | **0** | 54 | 9 |

All nine controls (`valid 1→big`, `srcLen=0`) are `normal` in both columns. The six
large-destination cases of `build-probe/2146_probe2.log` — the ones that unmasked GZip and ZLib —
crashed before and throw after.

**Mutation testing.** Three mutations, of which **two count**:

| # | Mutation | Result | Counts? |
|---|---|---|---|
| 1 | remove `ValidateSource` from `DeflateEncoder::Compress` | **SIGSEGV, exit 139** | **No** — an abort/UB-only outcome, which this batch's own rules exclude. It does confirm the defect is real, but it is not a clean pin discrimination |
| 2 | `length < 0` → `length <= 0` (over-rejection) | **7 clean failures**, all of them the legal-zero-length and round-trip pins; every rejection test stayed green | **Yes** |
| 3 | XOR one produced byte after `deflate()` | **exactly 2 clean failures** — the round-trip and byte-stability pins — with all 73 bounds tests green | **Yes** |

Mutation 3 is the important one: it shows the output pins detect output corruption *specifically*,
and that the bounds tests are not accidentally coupled to the produced bytes. Both mutations were
reverted and the tree rebuilt to 75/75 green.

**Tests: +35** (`CompressionBoundsTests.cpp`), typed over all three encoders and all three decoders.
`SharpRuntimeTests_IO_Compression` **40 → 75**.

**Consequences.** No public signature, `noexcept`, virtual, vtable, data member or object-layout
change. The component graph is unchanged at **41 / 92**; the only catalogue movement is the
representative-header column, because `CompressionArgumentValidation.hpp` now sorts first among the
module's public headers.

**SR-AUD-256 is `remediated`.** SR-AUD-258 and SR-AUD-259 remain `confirmed`, owned by #2148 and
#2149/#2150.

---

## 14. Implementation record — #2148 (cause C-B, SR-AUD-258)

**Two premise corrections, both measured before anything was edited**
(`build-probe/2148_probe1_before.log`, `build-probe/2148_probe2_before_after.log`).

### 14.1 The out-of-domain mode is a guaranteed leak, not only a capability inconsistency

The audit records the consequence as "creates a deflater, and reports both CanRead and CanWrite
false". Measured, it is worse, and the mechanism is a split the two halves of each type make on
**different** enumerators:

| Body | Test |
|---|---|
| constructor | `mode == Decompress ? inflateInit2(…) : deflateInit2(…)` |
| `Close()` | `mode == Compress ? …Z_FINISH…; deflateEnd(…) : inflateEnd(…)` |

An out-of-domain value takes the **deflate** arm on the way in and the **inflate** arm on the way
out, so `inflateEnd` is handed a `deflateInit2`-initialised `z_stream`. zlib rejects that with
`Z_STREAM_ERROR` and frees nothing. LeakSanitizer, with the three stream bodies compiled from
source and instrumented:

| Arm | Before | After |
|---|---|---|
| `mode = 42`, one cycle per type | **leaks: 3 × 5,952 direct + 8 × 65,536 indirect bytes**, exit 1 | three rejections at construction, **no leaks**, exit 0 |
| `mode = Compress` (control) | clean, exit 0 | clean, exit 0 |

The caller's payload is discarded too: a 4 KiB write through a mode-42 stream left **0** inner
bytes for `DeflateStream`, **10** for `GZipStream` (the gzip header alone) and **2** for
`ZLibStream` (the zlib header alone) — a header with no payload and no trailer, because `Close()`'s
`Z_FINISH` loop is guarded by `mode == Compress` and never ran.

### 14.2 The closed-state defect is on three doors, not one

The finding names `Write`. Measured on all three types × both `leaveOpen` values, `Read` answered
**0** and `Flush` returned **silently** after `Close()` as well — the identical
`!state_->initialized` early return. A repair covering only `Write` would have left a closed stream
still reporting "the compressed stream is exhausted" from `Read`, which is the more dangerous of the
two because it is a silent *wrong answer* rather than silently dropped work.

### 14.3 The repair

Two more entries in the module's single validation choke point
(`CompressionArgumentValidation.hpp`, established by #2146):

- `Detail::ValidateCompressionMode(mode)` — throws the **base** `System::ArgumentException`
  (`"Enum value was out of legal range."`, param `"mode"`). Base, not the derived
  `ArgumentOutOfRangeException` that #2148's acceptance criterion named: the audit's own managed
  probe for SR-AUD-258 recorded .NET's category for this exact call as `ArgumentException`
  (per-file report, "current .NET prints `invalidMode=ArgumentException`"), `/rv` is absent to
  narrow it further, and the same reasoning already settled `System::Threading`'s `EventWaitHandle`
  (#1954) and `System::Uri`'s `Uri(string, UriKind)` (#1992). **The acceptance criterion is
  corrected rather than satisfied literally.**
- `Detail::ThrowIfStreamClosed(open, typeName)` — throws
  `ObjectDisposedException(typeName, "Cannot access a closed Stream.")`, the spelling
  `MemoryStream` and `UnmanagedMemoryStream` already use.

**Placement is load-bearing in three places**, and each is pinned by its own test:

1. the mode check sits **after** the null-stream check and **before** `deflateInit2`/`inflateInit2`
   — .NET's constructor order, and the only placement at which a rejected construction has nothing
   to leak;
2. the closed check sits **after** the buffer-argument validation in `Read`/`Write` — .NET runs
   `ValidateBufferArguments` before `EnsureNotDisposed`;
3. the closed check sits **before** the `count == 0` early return — otherwise "nothing to write"
   launders a use-after-close into a success.

### 14.4 Mutation testing — three mutations, all three count

| # | Mutation | Result | Counts? |
|---|---|---|---|
| 1 | delete `ValidateCompressionMode` from `DeflateStream`'s constructor | **2 clean failures** — both mode-domain tests; the other 87 green, including GZip's and ZLib's | **Yes** |
| 2 | move the closed check **ahead** of `Read`'s buffer validation | **1 clean failure** — the order pin alone | **Yes** |
| 3 | move `Write`'s closed check **after** the `count == 0` return | **1 clean failure** — the zero-count pin alone | **Yes** |

No mutation crashed, hung or failed to build; all three were reverted and the tree rebuilt to
89/89 green. UBSan over the whole 51-case matrix: **0 reports** (the enum is scoped, so its
underlying type is `int` and a cast of 42 is well-defined — the defect was never UB, which is
exactly why nothing detected it before).

### 14.5 Two doors deliberately NOT changed → new ticket #2152

Measured and **pinned**, not repaired, because SR-AUD-258 does not name them and `/rv` is absent to
confirm the exception type .NET uses:

| Door | This port, measured | .NET (recollected, unverifiable here) |
|---|---|---|
| `Read` on a **Compress**-mode open stream | returns **0** | `InvalidOperationException` |
| `Write` on a **Decompress**-mode open stream | `IOException("… deflate error -2")` | `InvalidOperationException` |

Both are pinned by `CompressionStreamStatePinTests`, which name #2152 in the test name.

### 14.6 Consequences

**Tests: +14** (`CompressionStreamStateTests.cpp`); `SharpRuntimeTests_IO_Compression`
**75 → 89**. No public signature, `noexcept`, virtual, vtable, data member or object-layout
change — the mode and the open flag were both already stored. The component graph is unchanged at
**41 / 92**: `CompressionArgumentValidation.hpp` now includes `CompressionMode.hpp`, which is a
header in the same component.

**Narrowing, deliberately.** Two calls that used to succeed now throw: constructing with a cast
mode, and touching a closed stream. A repository-wide search for `static_cast<CompressionMode>`
and `(CompressionMode)` over `modules/`, `test/`, `tests/` and `bench/` returns hits only in this
ticket's own tests and probes, so no existing caller is reached.

**SR-AUD-258 is `remediated`.**

---

## 15. Implementation record — #2149 (cause C-C, SR-AUD-259 strategy half)

### 15.1 The finding is confirmed, and its evidence widened from one pair to 45 cases

The audit's evidence is a single pair — "native RLE and default option probes produce identical
60-byte output" — on one encoder. Measured across the full grid of **3 encoders × 5 strategies ×
3 payload shapes** (`build-probe/2149_probe1_before.log`):

| | Cases | identical to `Default` | port ≠ zlib given the same parameters |
|---|---|---|---|
| before | 45 | **45** | **24** |
| after | 45 | 21 | **0** |

The 21 that remain identical to `Default` after the repair are not a residual defect: `Filtered`
genuinely produces `Default`'s bytes at level 6 on all three payloads (zlib agrees, in the same 45
cases), and raw deflate over incompressible input emits stored blocks whose bytes no strategy
changes. The port now matches a straight zlib encode with the same
level/windowBits/memLevel/strategy in **all 45**.

The payload shape is load-bearing and is why the test file builds its own inputs: a test that
discriminated on `Filtered` alone would have passed against the broken code.

### 15.2 The repair

`Detail::ResolveZLibStrategy` — one mapping, declared in `CompressionArgumentValidation.hpp` and
defined once in the matching `.cpp`, returning `intcs` so no zlib header leaks into a public
header. Five `static_assert`s pin the enum's members to zlib's constants, so a renumbering on
either side stops the build instead of silently compressing with the wrong strategy.

`DeflateEncoder(options)` no longer delegates to `DeflateEncoder(quality, windowLog)` — that
constructor's `Z_DEFAULT_STRATEGY` is its own documented contract — and instead resolves the same
window bits and memLevel through two private static helpers and passes the stored strategy to the
four-argument constructor. `GZipEncoder`/`ZLibEncoder`'s file-local `MakeDeflateEncoder` gains a
`strategy` parameter; their three **non**-options constructors pass `Z_DEFAULT_STRATEGY`
explicitly, unchanged.

Both argument validations live in one helper, deliberately: the evaluation order of a
constructor's arguments is unspecified, and quality-before-windowLog is the order
`DeflateEncoder(quality, windowLog)` already established.

### 15.3 Mutation testing — two count, one is a proven non-defect

| # | Mutation | Result | Counts? |
|---|---|---|---|
| 4 | options path passes `Z_DEFAULT_STRATEGY` again (the pre-#2149 behaviour) | **2 clean failures** | **Yes** |
| 5 | `RunLengthEncoding` mis-mapped to `Z_FIXED` | **SURVIVED** the suite as first written — the wrong strategy is still *a* strategy, so the output still differed from `Default`, still round-tripped and still landed in the right size order. Adding a **pairwise-distinctness** assertion killed it: **3 clean failures** | **Yes — after the test was strengthened.** This is the mutation that earned its keep |
| 6 | options path always resolves memLevel 8, never the quality-0 value 7 | **SURVIVED, and correctly so.** The port selects memLevel 7 only for quality 0, and at level 0 zlib emits **stored blocks**, where memLevel has no effect: memLevel 7 and 8 produce identical bytes at level 0 for every input measured, and differ only at levels 1/6/9 where both paths already agree on 8 (`build-probe/2149_probe2_memlevel.log`). The mutation is **observationally equivalent**, not an undetected defect | **No — recorded as a non-result** |

ASan + UBSan + LSan over the whole 45-case matrix with the three encoder bodies instrumented:
**0 reports**.

### 15.4 Consequences

**Tests: +9** (`CompressionStrategyTests.cpp`); `SharpRuntimeTests_IO_Compression` **89 → 98**.

**The default-option output is byte-identical to before** — the discriminating control this ticket
exists to protect. Verified by diffing the `Default` rows of the before and after probe logs (all
nine identical) and pinned by an exact-byte-count test.

**Behaviour change, deliberate and narrow.** A caller who set a non-`Default` strategy previously
got `Default`'s bytes and now gets that strategy's. That is the finding. Every strategy still
round-trips through the matching decoder.

No public signature, `noexcept`, virtual, vtable, data member or object-layout change — the two new
`ResolveOptions*` members are **private static** functions, which add no vtable slot and no storage.
Component graph unchanged at **41 / 92**.

**SR-AUD-259 is only half closed by this ticket.** Its remaining half — the three stream types'
absent `ZLibCompressionOptions` constructors — is a public surface addition and stays with the
**blocked** #2150. The finding therefore stays `confirmed`, with its strategy half recorded as
landed.
