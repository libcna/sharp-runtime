# Audit: `modules/net-security/include/System/Net/Security/SslApplicationProtocol.hpp`

## Metadata

- AUDITED: ALPN construction, length validation, UTF-8 rendering, equality,
  hash, and HTTP protocol constants.
- Validation: complete Net.Security fixture passed 13/13. A direct valid
  255-byte native ALPN probe was compiled with UBSan; the same byte sequence
  was checked with current .NET.

## SR-AUD-240 — medium — valid ALPN protocol hashing executes signed-overflow undefined behavior

`GetHashCode()` accumulates a 32-bit signed `intcs` with
`((hash << 5) + hash) ^ byte`.  A valid 255-byte protocol of `0xff` causes
UBSan to report signed integer overflow at the addition in line 72.  A normal
native build and current .NET both currently print hash `-969519841`, but .NET
defines the operation's unchecked Int32 wrap while C++ signed overflow is
undefined and may be miscompiled or trapped.  The path is reachable through a
valid public constructor, not malformed protocol data.

## Assessment

Length bounds, raw-byte equality, strict UTF-8 detection, default value, and
hex fallback agree with the managed value semantics.  The public vector is
exposed only through a const reference, preserving immutable observation in
ordinary use.  SR-AUD-240 prevents the hash from having a portable C++
contract for legitimate long identifiers.

## Other missing assertions and diagnostics

- Add UBSan coverage for short and 255-byte hash inputs (SR-AUD-240), then
  assert the checked native bit pattern against current .NET.
- Cover the default value, all one/255-byte boundaries, string inputs with
  invalid UTF-8/unpaired-surrogate equivalents, equality/hash collisions,
  copy isolation, and every invalid UTF-8 sequence class.

## Final assessment

SR-AUD-240 is UBSan-confirmed. No source or test was changed during this audit.
