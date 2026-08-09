<!-- SPDX-License-Identifier: MIT -->

# SharpRuntime::IO.Hashing

Compiled non-cryptographic hashing component. Public dependencies:
`Core.Base` and `IO`.

See the [generated component catalogue](../../docs/ComponentCatalog.md) for
authoritative dependency metadata.

## The raw-pointer contract

.NET's `System.IO.Hashing` surface is expressed in `ReadOnlySpan<byte>` and
`Span<byte>`. This port has no span type, so **every** span was replaced by a
raw pointer plus a signed `SharpRuntime::intcs` length. That substitution makes
two states representable that a span cannot represent, and every raw-pointer
overload in this component — on `Adler32`, `Crc32`, `Crc64`, `XxHash32`,
`XxHash64`, `XxHash3`, `XxHash128`, `NonCryptographicHashAlgorithm` and both CRC
parameter sets — obeys the same four rules.

### Input buffers (`source`, `length`)

| Argument | Behaviour |
|---|---|
| `length < 0` | `ArgumentOutOfRangeException`, `paramName` `"length"`, message *"Non-negative number required."* |
| `length > 0` and `source == nullptr` | `ArgumentNullException`, `paramName` `"source"` |
| `length == 0`, **any** `source` including `nullptr` | **accepted** — a legal empty operation |

A null pointer with a zero length is accepted deliberately: `default(ReadOnlySpan<byte>)`
in .NET is an empty span whose reference is null, and appending it is legal.
`std::vector<bytecs>{}.data()` is likewise null. Only a null pointer with a
*positive* length is an error.

A rejected `Append` **does not change the accumulated hash**: the validation
precedes the state update, so a hasher is still usable after one.

### Output buffers (`destination`, `destinationLength`)

| Condition | Behaviour |
|---|---|
| `destinationLength < getHashLengthInBytesProperty()` | `Hash`/`GetCurrentHash`/`GetHashAndReset`: `ArgumentException`, `paramName` `"destination"`, message *"Destination is too short."* — `TryHash`/`TryGetCurrentHash`/`TryGetHashAndReset` return `false` and set `bytesWritten` to 0 |
| capacity suffices and `destination == nullptr` | `ArgumentNullException`, `paramName` `"destination"` — including from the `Try…` forms, because a caller told "too small" about a buffer that does not exist is being misinformed |
| capacity suffices and `destination != nullptr` | the hash is written; an oversized buffer is fine and only the leading bytes are touched |

**The capacity claim is decided first.** A null destination whose claimed length
is *insufficient* is reported as *"Destination is too short."*, not as a null
argument. A rejected call leaves the destination buffer byte-for-byte unchanged.

`Crc32ParameterSet::WriteCrcToSpan` and `Crc64ParameterSet::WriteCrcToSpan` are
the exception to the table: they carry **no capacity argument**, because the
size is implied by the algorithm (4 and 8 bytes). They reject a null destination
and can do nothing about a short one — the caller must supply a buffer of at
least that size.

### Byte order

The **numeric** result and the **emitted bytes** are two separate halves of each
algorithm's published contract, and both are fixed:

| Algorithm | Destination byte order |
|---|---|
| `Adler32` | big-endian, per RFC 1950 |
| `Crc32`, `Crc64` | little-endian when the parameter set reflects its values, big-endian when it does not |
| `XxHash32`, `XxHash64`, `XxHash3` | big-endian |
| `XxHash128` | big-endian, high 64 bits first |

Internally, xxHash lane loads are **little-endian by construction** (see
`HashingByteOrder.hpp`) rather than by native-order `memcpy`, so the algorithm's
byte order does not depend on the host's. Note the limit: this is verified by
host-independent helper tests and by the published check values on a
little-endian host. **Execution on a big-endian machine is not exercised
anywhere in this repository**, because there is no such host here and no
cross-run in its CI.

### Exceptions

Every rejection at every public door is a `System::` exception. No `std::`
exception escapes this component's public API.

## Deliberate deviations from .NET

- **No `AppendAsync`.** There is no `Task`-returning stream append in this port;
  `Append(System::IO::Stream&)` is synchronous.
- **`XxHash128` returns a `Hash128` struct**, not `System::UInt128`, which would
  require the GCC/Clang `unsigned __int128` extension. `Hash128` works on every
  toolchain this runtime targets.
- **The raw-pointer surface itself.** The rules above have no .NET counterpart
  to match, because the overloads they govern do not exist there; they are this
  port's chosen contract, not a reproduction of one.
