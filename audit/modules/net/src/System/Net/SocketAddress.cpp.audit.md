# Audit: `modules/net/src/System/Net/SocketAddress.cpp`

## Metadata

- Audit status: AUDITED.
- Reference: [SocketAddress.cs](/rv/tmp/runtime/src/libraries/Common/src/System/Net/SocketAddress.cs).
- Evidence: ASan/UBSan probe `/tmp/sharp-runtime-net-audit/net_bounds.cpp`.

## Assessment

The raw-buffer constructor deliberately permits a two-byte `SocketAddress`, as
does .NET.  The C++-only public `GetIPEndPoint()` then assumes an IPv4 or IPv6
layout without checking the stored size or family.  `IPEndPoint::Create` does
perform those checks, so this is a direct-helper boundary that bypasses its
own safe path.

### SR-AUD-300 — high — `GetIPEndPoint` decodes undersized and non-IP raw buffers without validating their layout

`SocketAddress(AddressFamily::InterNetwork, 2).GetIPEndPoint()` reads
`buffer_[2]`, `buffer_[3]`, and the address bytes past the two-byte allocation.
The ASan probe reports a heap-buffer-overflow at `SocketAddress.cpp:106`.
An `AddressFamily::Unix` buffer is also treated as IPv4.  The public
`SocketAddress(const IPAddress&, intcs)` accepts an invalid port and truncates
it to 16 bits before serialization.  .NET exposes the raw buffer and lets an
endpoint's `Create` validate family and size; it does not expose this unsafe
decoder as a public shortcut.

Required remediation: validate family, minimum size, and port at every public
decode/encode boundary; reject unsupported family values with the established
`ArgumentException` contract.  Keep raw-buffer construction permissive.

## Missing assertions and diagnostics

`NetTests.cpp` covers normal `IPEndPoint::Create` layouts but has no direct
undersized `GetIPEndPoint`, unknown-family, or invalid-port assertion.  Add
those cases and run them under ASan/UBSan.

## Final assessment

Unsafe public raw-buffer decoding is confirmed.

---

## Correction and remediation record — ticket #2035, 2026-08-04

*Everything above is the original audit text and is preserved verbatim. This section is
appended, not a rewrite.* Evidence: `build-probe/2035_probe1_before_after.log`, produced from
one probe source built six ways (`{before, after} × {plain, ASan, UBSan}`) with
`SocketAddress.cpp`, `IPAddress.cpp` and `IPEndPoint.cpp` compiled **from source** into every
binary, so both the allocation and the reads are instrumented.

### The assessment is correct, at both of its lines

The `heap-buffer-overflow` reproduces at `SocketAddress.cpp:106` for a 2-byte `InterNetwork`
buffer — *0 bytes after a 2-byte region*, exactly as reported — and **also at `:110`** for an
8-byte `InterNetworkV6` buffer, which the report does not mention. The full family × size matrix
shows every non-IP family (`Unknown`, `Unspecified`, `Unix`, `Max`, and an unnamed value)
decoding through the IPv4 branch, and the values produced are whatever the heap held:
`238.85.0.0:52122` for a 2-byte buffer, `[::%553648128]:52122` for the IPv6 one.

### Extension 1 — the port defect spans the whole signed domain

The report names an "invalid port" truncated to 16 bits. Measured across the domain:

| `port` | Encoded as |
|---|---|
| `INTCS_MIN` (-2147483648) | **0** |
| -1 | 65535 |
| 65536 | **0** |
| 70000 | 4464 |
| `INTCS_MAX` (2147483647) | 65535 |

So an out-of-range port did not merely truncate — it could produce a **plausible** port a caller
cannot distinguish from a real one, including 0 for both `INTCS_MIN` and 65536.

### Extension 2 — a defect the report does not name: the *declared* size

`setSizeProperty` can shrink a well-formed 16-byte IPv4 `SocketAddress` to a declared size of 4.
Before the repair, `GetIPEndPoint()` still decoded `127.0.0.1:80` — reading offsets 4–7, which
the type's **own** `operator[]` refuses at the same offsets with `IndexOutOfRangeException`. The
allocation is intact, so **no sanitizer sees this**: ASan is silent both before and after. It is
a contradiction inside the type's own contract rather than a memory error, and it is why the
repair validates `getSizeProperty()` rather than `buffer_.size()` — `size_ <= buffer_.size()`
always holds, so bounding the declared size bounds the allocation too.

No `SR-AUD-*` identifier was issued for either extension; numbering stays frozen at 364.

### Remediation

- `GetIPEndPoint()` rejects a family that is neither `InterNetwork` nor `InterNetworkV6`, and a
  declared size below `IPv4AddressSize` (16) / `IPv6AddressSize` (28), with
  `System::ArgumentException` naming the family and, for the size case, both numbers. Those two
  minimums are **`IPEndPoint::Create`'s own**, which is the safe path this public shortcut
  bypasses, and are exactly what `SocketAddress(const IPAddress&, intcs)` produces — so nothing
  this class builds is narrowed.
- `SocketAddress(const IPAddress&, intcs)` validates the port through
  `ArgumentOutOfRangeException::ThrowIfLessThan/ThrowIfGreaterThan` against
  `IPEndPoint::MinPort`/`MaxPort`, `paramName = "port"` — `IPEndPoint::validatePort`'s own
  domain and exception identity, since the encoded result is handed straight to an `IPEndPoint`.
- **Raw-buffer construction stays permissive**, as this report's remediation note requires: only
  the decode is narrowed, and a 2-byte buffer of any family is still constructible, indexable
  and printable.

| Sanitizer | Before | After |
|---|---|---|
| ASan | **2** `heap-buffer-overflow` READs (`:106`, `:110`) | **0** |
| UBSan | none | none — **non-discriminating**, recorded as a non-result |
| LSan | — | no leak across the whole matrix |

**Tests:** `modules/net/tests/System/Net/SocketAddressDecodeTests.cpp` (17), including every
family × {below minimum, minimum, above}, the port domain `INTCS_MIN`/-70000/-1/0/1/80/32768/
65535/65536/70000/`INTCS_MAX` on both IP families, the shrunk-declared-size case, agreement
between `IPEndPoint::Create` and the shortcut, and four **byte-identical** round-trip pins whose
literals are transcribed from the pre-repair probe log.

**Consequences:** no signature, `noexcept`, virtual, vtable, data member or object-layout change;
`sizeof(SocketAddress)` is 32 before and after. The header gained only doc-comments stating the
new contract. `modules/net-sockets`'s `UnixDomainSocketEndPoint`, the only cross-module user of
`AddressFamily::Unix` `SocketAddress` buffers, never calls `GetIPEndPoint` and is unaffected —
verified, not assumed.

Status: `confirmed` → **`remediated`**.
