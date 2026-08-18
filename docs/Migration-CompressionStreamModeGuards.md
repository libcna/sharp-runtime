<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the compression streams enforce their own mode (ticket #2152)

*2026-08-18.* `DeflateStream`, `GZipStream` and `ZLibStream` now reject a `Read` on a
Compress-mode stream and a `Write` on a Decompress-mode stream with
`InvalidOperationException`, matching .NET.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout or `noexcept` change.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `Read` on an open **Compress**-mode stream | returned **0** | `InvalidOperationException("Reading from the compression stream is not supported.")` |
| `Write` on an open **Decompress**-mode stream | `IOException("DeflateStream: deflate error -2")` | `InvalidOperationException("Writing to the compression stream is not supported.")` |
| `Read` on a **closed Compress**-mode stream | `ObjectDisposedException` | the **mode** error — see §3 |
| `Write` on a **closed Decompress**-mode stream | `ObjectDisposedException` | the **mode** error |
| `Flush()` in either mode | no mode guard | **no mode guard** (unchanged, deliberately — §4) |
| every call in the mode that allows it | — | **unchanged** |

**The `Read` row is the one that mattered.** Returning 0 is indistinguishable from end-of-stream,
so a read loop over a Compress-mode stream terminated normally having produced nothing, with no
diagnostic anywhere. The `Write` row already threw; it just named zlib's internal `Z_STREAM_ERROR`
for what is a caller mistake.

A caller that checks `CanRead`/`CanWrite` first is unaffected — those properties already reported
the right answer, which is exactly why the defect was invisible.

## 2. The reference

```csharp
private void EnsureDecompressionMode()
{
    if (_mode != CompressionMode.Decompress) ThrowCannotReadFromDeflateStreamException();
    static void ThrowCannotReadFromDeflateStreamException() =>
        throw new InvalidOperationException(SR.CannotReadFromDeflateStream);
}
```
*(`DeflateStream.cs:387-395`; `EnsureCompressionMode` is the mirror at `:396-403`.)*

The two messages are `Strings.resx:122,125` verbatim. They name *"the compression stream"* rather
than a concrete type, so all three wrappers share one string — which is what .NET does too, since
`GZipStream` and `ZLibStream` delegate to `DeflateStream`.

## 3. The order is transcribed, not chosen

```csharp
public override int Read(byte[] buffer, int offset, int count)
{
    ValidateBufferArguments(buffer, offset, count);      // 1. arguments
    return ReadCore(new Span<byte>(buffer, offset, count));
}

internal int ReadCore(Span<byte> buffer)
{
    EnsureDecompressionMode();                            // 2. mode
    EnsureNotDisposed();                                  // 3. disposed
```
*(`DeflateStream.cs:284-309`; `Write`/`WriteCore` is the same shape at `:531-570`.)*

So a stream that is **both** disposed and in the wrong mode reports the **mode**. That is a
behaviour change for one existing test, which is updated rather than worked around.

**A mutation proved the type alone cannot express this.** `ObjectDisposedException` derives from
`InvalidOperationException` — here as in .NET — so `EXPECT_THROW(…, InvalidOperationException)` is
satisfied by *both* orders and the swapped-order mutation went uncaught. The message is the only
discriminator, so the message is what the test asserts.

## 4. `Flush()` deliberately has no guard

.NET's `Flush()` opens with `EnsureNotDisposed()` alone and then no-ops for a Decompress-mode
stream (`DeflateStream.cs:210-215`). Adding a mode guard there would be a plausible-looking
symmetry the reference does not have, so a test pins its absence.

## 5. A correction made on the way past

`ThrowInvalidCompressionMode`'s doc-comment recorded that #2148 chose the **base**
`ArgumentException` from the audit's managed probe and that *"`/rv` is absent here to narrow it
further"*. `/rv` is present now and confirms the choice exactly: `DeflateStream.cs:99` is
`throw new ArgumentException(SR.ArgumentOutOfRange_Enum, nameof(mode))`, and
`ArgumentOutOfRange_Enum` is *"Enum value was out of legal range."* — same type, same message,
same parameter name. The caveat is replaced with the measurement.

## 6. To migrate

```cpp
// A stream opened to compress can only be written; one opened to decompress can only be read.
if (stream.getCanReadProperty()) { /* … Read … */ }
```

`CanRead` and `CanWrite` have always reported this correctly. Code that branched on them needs no
change; code that did not now gets a diagnostic instead of silence.

## 7. Evidence

| Mutation | Caught |
|---|---|
| Drop the read-mode guard (one stream type) | ✅ |
| Drop the write-mode guard (one stream type) | ✅ |
| Run the mode check **after** the disposed check | ✅ — **only after** the test asserted the message; the type alone could not see it |
| Add a mode guard to `Flush()` | ✅ |
| Swap the two messages | ✅ (2 tests) |

## 8. Downstream, measured

Per SA-2 condition 5: neither `cna` nor `mobile-eggbert` references `DeflateStream`, `GZipStream`
or `ZLibStream` — **zero sites in both**. Neither repository was modified.
