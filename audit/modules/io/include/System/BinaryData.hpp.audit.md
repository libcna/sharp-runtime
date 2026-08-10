# Audit: `modules/io/include/System/BinaryData.hpp`

## Metadata

- Audit status: AUDITED.
- AUDITED: 395-line inline BinaryData implementation, fully read.
- Validation: Core `MathFTests.*:BinaryDataTests.*` passed 33/33 and IO
  `BinaryDataTests.*` passed 15/15 on 2026-07-27; the corresponding two test
  sources are audited with this report.
- Reference/probe: local current-.NET `System.Memory.Data/BinaryData.cs` and
  standalone C++/managed UTF-8 probes.

## SR-AUD-185 — medium — BinaryData.ToString returns malformed UTF-8 bytes unchanged instead of UTF-8-decoding them

`ToString()` constructs a `std::string` directly from the byte vector.  For
the invalid single byte `FF`, the C++ probe prints
`binary_data_tostring_hex=FF`.  Current .NET's `BinaryData.ToString()` calls
`Encoding.UTF8.GetString(_bytes.Span)`; the equivalent managed UTF-8 probe
prints `utf8_decode_hex=EFBFBD` for U+FFFD replacement.

The difference affects a public conversion from arbitrary binary payload to
text.  The port's valid UTF-8/ASCII tests pass, but no declaration marks raw
byte preservation as an intentional replacement for the stated UTF-8 decode.

## SR-AUD-186 — medium — BinaryData.FromBytes(ReadOnlyMemory) snapshots bytes that current .NET deliberately wraps

The ReadOnlyMemory constructors copy `[pointer, pointer + length)` into the
owned vector, despite the factory documentation saying it wraps the supplied
memory.  The C++ probe constructs BinaryData from byte `01`, changes the
source byte to `02`, and prints `from_memory_after_source_mutation=1`.  Current
.NET stores `_bytes = data`; its constructor and FromBytes documentation call
this a wrapper, so the supplied memory's later content remains observable
through BinaryData.

The top-level C++ note that BinaryData owns a vector explains the implementation
choice but does not document this public snapshot semantic alongside the
factory contract.  It changes observation, allocation, and lifetime behavior
for every caller using the ReadOnlyMemory overload.

## Assessment

The header provides useful synchronous byte/string/stream/file conversions and
explicitly documents its content equality/hash adaptation and omitted JSON
surface.  Index validation and read-only streams are covered by direct tests.
The two conversion/ownership behaviors above are independent public contract
deviations.

## Other missing assertions and diagnostics

- Tests use only valid ASCII strings; they omit malformed/overlong UTF-8,
  replacement behavior, embedded NUL, non-ASCII round trips, and UTF-8 error
  diagnostics (SR-AUD-185).
- No ReadOnlyMemory/vector mutation-after-construction case distinguishes a
  copy from a wrapper, nor are view lifetime/reallocation/move semantics
  tested (SR-AUD-186).
- FromFile lacks race, unreadable-path, large-file, media-type, and exact
  exception taxonomy tests. FromStream has null/error/short-read/non-seekable/
  malicious-count coverage gaps; ToStream lacks position/lifetime/independent
  copy coverage.
- The `intcs` length narrowing and borrowed ToMemory/ReadOnlySpan views have
  no near-limit/lifetime diagnostics. JSON, async, type-metadata, and current
  broader BinaryData overloads are explicitly absent but have no capability
  query.

## Final assessment

SR-AUD-185 and SR-AUD-186 are confirmed by source and direct probes. No source
or test was modified.
