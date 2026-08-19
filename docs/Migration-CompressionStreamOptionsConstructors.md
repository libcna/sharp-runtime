<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the three compression streams gained their options constructors (ticket #2150)

*2026-08-19.* `System::IO::Compression::DeflateStream`, `GZipStream` and `ZLibStream` each gained

```cpp
Stream(System::IO::Stream* stream, const ZLibCompressionOptions& options, bool leaveOpen = false);
```

matching .NET's `(Stream, ZLibCompressionOptions, bool)` constructor on all three types.

**Purely additive.** Nothing that compiled before compiles differently, no existing call changes
meaning, no symbol changed, and no object grew. Landed under `docs/StandingApprovals.md` **SA-5**.

---

## 1. The ticket's classification was wrong, and the repository's own record already said so

#2150 was recorded as *"blocked — public surface addition, approval required"*, on the grounds
that *"new overloads change source overload resolution and add mangled symbols"*.

**The design record already contradicted that.** `docs/SystemIOCompressionNamespaceReviewPlan.md`
§7 lists #2150 as: signature *new ctors*, vtable —, layout —, `noexcept` —, accepted input —,
observable result **additive**.

**And the overload-resolution claim is false here, measurably.** A new overload can change the
meaning of an existing call only if some argument can bind to both. Two facts prevent that:

| Fact | Consequence |
|---|---|
| `CompressionMode` is a **scoped** enumeration | no integer, `bool` or other enum converts to it, and it converts to nothing |
| `ZLibCompressionOptions` has **no converting constructor** — only a defaulted default one | nothing implicitly converts *to* it |

With no type convertible to both parameter types, no existing call can rebind and no new call can
bind to the old overload. This is asserted, not argued, by
`Decl2150_TheAdditionCannotRebindAnExistingCall` — and if either fact stops holding, that test
fails and the additive claim must be re-made.

*Adding* mangled symbols breaks nothing: no existing symbol was removed or changed, so an
already-linked consumer is unaffected and a recompiling one sees only new declarations.

This is the same shape as #1980's G-1 and #1997's A-1 earlier in this programme: a sub-item whose
own record calls it additive is ordinary SA-5 work.

## 2. What the constructor does

It implies **`CompressionMode::Compress`** — .NET's options overload carries no mode, for the
same reason its `CompressionLevel` overloads are commented *"Implies mode = Compress"*: every
option the type holds describes compression.

Every option is honoured:

| Option | Becomes |
|---|---|
| `CompressionLevel` | zlib's `level` |
| `CompressionStrategy` | zlib's `strategy`, via the shared `Detail::ResolveZLibStrategy` |
| `WindowLog` | zlib's `windowBits`, via the shared `Detail::ResolveWindowBits` |
| *(derived from the level)* | zlib's `memLevel` — **7** at quality 0, otherwise **8**, as .NET does |

## 3. One definition of the format arithmetic, not four

`Detail::ResolveWindowBits(windowLog, CompressionFormat)` and `Detail::ResolveDeflateMemLevel`
are new, and they are transcribed from .NET's `CompressionFormatHelper.ResolveWindowBits`
(`CompressionFormat.cs:30-48`) and `DeflateEncoder` (`DeflateEncoder.cs:75-77, 95-97`).

Before this ticket, each of the three **encoders** carried its own copy of the format arithmetic
in an anonymous namespace, and the three **streams** had none — because they had no options
constructor to need one. Rather than add three more copies, the three encoders now delegate to
the shared resolver too. That is a net removal, and it is proven behaviour-preserving: all 103
pre-existing `SharpRuntimeTests_IO_Compression` cases pass unchanged.

**Two asymmetries are transcribed rather than smoothed away**, and both are pinned:

1. **Deflate and GZip clamp the window log to a minimum of 9; ZLib does not.** .NET's own comment
   gives the reason — *"zlib-ng rejects windowBits 8 for raw deflate and gzip; classic zlib
   silently upgrades to 9"* — so the clamp makes the two zlib implementations agree. A resolver
   that clamped uniformly would change what a `WindowLog` of 8 means for `ZLibStream`.
2. **The sign and offset are the container**: raw deflate is negative, zlib positive, gzip
   positive plus 16.

## 4. Evidence

Six mutations, **all caught**:

| Mutation | Caught by |
|---|---|
| M1 — the stream constructor drops the strategy | `Fix2150_TheStrategyReachesZlibInAllThreeStreams` |
| M2 — the window log is ignored and the default used | `Fix2150_TheWindowLogReachesZlibInAllThreeStreams` |
| M3 — the ZLib arm clamps to 9 as well | `Fix2150_ResolveWindowBitsTranscribesBothAsymmetries` — **only after that test was added**, see below |
| M4 — the deflate window-bits sign is lost | six cases, four of them **pre-existing** |
| M5 — the gzip `+16` offset is lost | five cases, three of them **pre-existing** |
| M6 — the `memLevel` boundary moves | `Fix2150_ResolveDeflateMemLevelIsSevenOnlyAtQualityZero` |

**M3 is the one worth recording.** It went uncaught at first because every end-to-end case used a
`WindowLog` of 9 or 15, so the single input that distinguishes clamping from not clamping — a
`WindowLog` of 8 — was never exercised. The fix was to assert the **resolver** directly rather
than to hunt for the difference in emitted bytes, and that is not merely more convenient: classic
zlib silently upgrades a `windowBits` of 8 to 9 internally, so an end-to-end test could not have
discriminated this reliably at all.

M6 was invalid as first written — removing the quality-0 arm left a `constexpr` unused and
`-Werror` rejected it — and was reformulated (moving the boundary to `<= 1`) rather than counted.

Gate: **17,444 run, 17,444 passed, 0 failed, 0 skipped** across 38 executables — `+10` on 17,434,
exactly the ten new cases (`SharpRuntimeTests_IO_Compression` 103 → 113). No other executable
moved. Module graph
unchanged at 41/93.

## 5. To use it

```cpp
ZLibCompressionOptions options;
options.setCompressionLevelProperty(9);
options.setCompressionStrategyProperty(ZLibCompressionStrategy::RunLengthEncoding);

MemoryStream sink;
GZipStream out(&sink, options, /*leaveOpen=*/true);
out.Write(data.data(), 0, static_cast<intcs>(data.size()));
out.Close();
```

Nothing needs migrating. The `(Stream*, CompressionMode, bool)` constructor is untouched and
still the only way to build a **decompressing** stream.

## 6. Downstream, measured

Per SA-2 condition 5 — recorded although SA-2 is not the approval this landed under, because an
addition to a public type deserves the measurement anyway: `DeflateStream`, `GZipStream`,
`ZLibStream` and `ZLibCompressionOptions` appear in **zero** places in `cna` and **zero** in
`mobile-eggbert`. Neither repository was modified.
