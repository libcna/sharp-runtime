# Audit: `modules/io-compression/src/System/IO/Compression/DeflateEncoder.cpp`

## Metadata

- AUDITED: raw Deflate zlib setup, compression, flushing, bounds, and options.
- Validation: ASan/UBSan probe plus strategy comparison against current .NET
  source.

## Assessment

Normal compression works, but public `intcs` source/destination lengths are
blindly cast to `uInt`; the exact `sourceLength=-1` probe gives zlib a huge
input and ASan reports a heap-buffer-overflow. Options construction also drops
its stored strategy, hard-coding `Z_DEFAULT_STRATEGY`.

## SR-AUD-256 — high — raw buffer compression APIs cast negative public lengths into unbounded native zlib input

`DeflateEncoder::Compress` accepts a one-byte source and `sourceLength=-1`,
casts it to `uInt`, and zlib reads 65,536 bytes past the allocation under ASan.
DeflateDecoder and the GZip/ZLib encoder/decoder wrappers expose the same
unguarded signed pointer-length boundary. The managed span APIs cannot express
negative lengths and validate their managed boundaries first.

## SR-AUD-259 — medium — CompressionStrategy is stored but never reaches any encoder and stream option constructors are absent

`DeflateEncoder(options)`, `GZipEncoder(options)`, and `ZLibEncoder(options)`
discard `options.getCompressionStrategyProperty()` and initialise
`Z_DEFAULT_STRATEGY`. Native RLE and default option probes produce identical
60-byte output. Current .NET passes `options.CompressionStrategy` to its native
encoder; its three stream types also provide options constructors that this
module omits.

## Other missing assertions and diagnostics

- Validate null/negative/oversized metadata before zlib; test destination
  boundaries, all strategies, flush/final states, and error diagnostics.

## Final assessment

SR-AUD-256 and SR-AUD-259 are confirmed. No source or test changed.

---

## Correction and remediation record — ticket #2146, 2026-08-09

**SR-AUD-256 is `remediated`.** The original evidence above is retained unchanged.

Every public raw-pointer door now validates before zlib sees a length, through one module-local
choke point, `System::IO::Compression::Detail::ValidateSource` / `ValidateDestination`
(`CompressionArgumentValidation.hpp`). The rule, the exception types and the messages are
deliberately identical to `System::IO::Hashing::Detail` (#2141/#2142), which repaired the same
idiom in the sibling module.

**Three corrections to the finding's extent, all measured** (`build-probe/2146_probe1_before.log`,
`build-probe/2146_probe2.log`):

1. **The source-side defect is in all three encoders, not only `DeflateEncoder`.** The 63-case
   matrix appears to show `GZipEncoder` and `ZLibEncoder` returning normally for
   `sourceLength = -1`. That is an artefact of the probe's **1-byte destination**: a gzip or zlib
   stream must emit a header first, so `deflate()` fills that byte and returns before it reads the
   source. Raw deflate has no header. With a 4096-byte destination **all three crash**. The
   destination size decides which over-run fires first, not whether the defect is present.
2. **The three decoders never crashed** — 21 of 21 cases returned normally, because `inflate()`
   rejects one byte of garbage with `Z_DATA_ERROR` before consuming the impossible `avail_in`.
   That is luck, not a guard: valid compressed input has no such early exit, and
   `bytesConsumed = sourceLength - avail_in` reports a meaningless count either way. They are
   repaired on those terms.
3. **Null handling already differed between the two halves**, which the report does not mention:
   every encoder null case threw (6/6), every decoder null case returned normally (6/6).

**Evidence.** 63 cases: **15 crashes → 0**, with all nine controls (`valid 1→big`, `srcLen=0`)
unchanged, and the six previously-masked large-destination cases now throwing. ASan reported a
65,536-byte READ past a one-byte source and a WRITE past a one-byte destination before; none after.

**Tests: +35** (`CompressionBoundsTests.cpp`), typed over all three encoders and all three
decoders, covering both destination sizes, both axes, null-with-positive-length, the legal
null-with-zero-length, exact-size destinations, no-partial-modification of out-parameters, and —
as the discriminating controls — round-trip correctness across eight representative inputs and
default-option byte stability. `SharpRuntimeTests_IO_Compression` **40 → 75**.

**Consequences.** No public signature, `noexcept`, virtual, vtable, data member or object-layout
change. SR-AUD-258 and SR-AUD-259 remain `confirmed`, owned by #2148 and #2149/#2150.

---

## Partial remediation for SR-AUD-259 — the strategy half (ticket #2149, 2026-08-09)

The audit evidence above is retained unchanged. **SR-AUD-259 stays `confirmed`**: this ticket
lands the strategy plumbing only. The finding's second half — the three stream types' absent
`ZLibCompressionOptions` constructors — is a **public surface addition** and stays with the
**blocked** ticket #2150. Cause **C-C** of
`docs/SystemIOCompressionNamespaceReviewPlan.md`; the full record is that plan's §15.

**Evidence widened from one pair to 45 cases.** The report's evidence is a single default-vs-RLE
pair on one encoder. Measured over 3 encoders × 5 strategies × 3 payload shapes
(`build-probe/2149_probe1_before.log`): **45 of 45** outputs were byte-identical to `Default`
before, and in **24** of them a straight zlib encode with the same level, window bits, memLevel and
strategy produced different bytes. After: 0 of 45 diverge from zlib, and the 21 still equal to
`Default` are genuine agreements — `Filtered` really does produce `Default`'s bytes at level 6 on
these payloads, and raw deflate over incompressible input emits stored blocks no strategy changes.

**Repair.** One mapping, `Detail::ResolveZLibStrategy`, declared in the module's existing
validation header and defined once, with five `static_assert`s pinning the enum's members to zlib's
constants. `DeflateEncoder(options)` stops delegating to `DeflateEncoder(quality, windowLog)` —
whose `Z_DEFAULT_STRATEGY` is its own contract — and resolves the same window bits and memLevel
itself; `GZipEncoder`/`ZLibEncoder`'s `MakeDeflateEncoder` gains a strategy parameter, and their
non-options constructors pass `Z_DEFAULT_STRATEGY` explicitly.

**The default-option output is byte-identical to before**, verified by diffing the `Default` rows
of the before/after probe logs and pinned by an exact-byte-count test. That is the control
separating "plumb an existing option through" from "change what every caller gets".

**Mutation testing, including one honest non-result.** Restoring `Z_DEFAULT_STRATEGY` gives 2 clean
failures. Mis-mapping `RunLengthEncoding` to `Z_FIXED` **survived the suite as first written** — the
wrong strategy is still a strategy — and a pairwise-distinctness assertion was added to kill it
(3 clean failures). Always resolving memLevel 8 also survived, and **correctly**: the port picks
memLevel 7 only at quality 0, where zlib emits stored blocks and memLevel has no observable effect
(`build-probe/2149_probe2_memlevel.log`), so that mutation is observationally equivalent rather
than an undetected defect.

**Evidence.** +9 tests (`CompressionStrategyTests.cpp`); `SharpRuntimeTests_IO_Compression`
**89 → 98**. ASan + UBSan + LSan over the 45-case matrix with the encoder bodies instrumented:
0 reports. No public signature, `noexcept`, virtual, vtable, data member or object-layout change —
the two new `ResolveOptions*` members are private static functions. Component graph unchanged at
41 / 92.
