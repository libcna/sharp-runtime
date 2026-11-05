<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# OrdinalIgnoreCase Unicode keys (TQ-078)

`StringComparer::OrdinalIgnoreCase()` and `OrdinalComparer(true)` now decode
UTF-8, apply invariant simple uppercase, and compare UTF-16 units. `Equals`,
`Compare` and `GetHashCode` share that key. They previously lowercased individual
UTF-8 bytes with the process C locale, missing non-ASCII case pairs and ordering
ASCII `a` after `[` instead of before it.

For example, U+00C4/U+00E4, Greek U+0391/U+03B1 and Cyrillic U+0410/U+0430 now
compare equal and hash equally. Dotless i U+0131 stays distinct from ASCII I/i;
dotted I U+0130 stays distinct from ASCII i. Sharp s does not expand to `SS`, and
precomposed letters do not become equivalent to combining sequences. Numeric
hashes are unspecified and must not be persisted across implementations.

The implementation uses the existing SA-4 pinned UCD16 simple-uppercase data
with the ordinal dotless-i/long-s exceptions. This implements the owner-approved
TQ-078 profile and the measured .NET 4 witnesses; it does not establish parity
with every historical Windows NLS version. .NET 4 delegates ordinal comparison
to the OS when CompareStringOrdinal is available, so version-sensitive pairs
outside those witnesses retain that qualification boundary. The source logic is
`CharUnicodeInfo.cs:294-310` and `pal_common.c:5-20` in the local .NET reference
snapshot, which is .NET 11, not .NET 4.

Malformed UTF-8 has no direct CLR string counterpart. The native implementation
preserves malformed bytes distinctly rather than merging them with U+FFFD.
Case-sensitive Ordinal and the culture-aware comparer fallbacks are unchanged.
Other string APIs and comparers that implement their own byte folding do not
automatically acquire this behavior.

No public spelling, signature, object layout, vtable or exception specification
changes. Rebuild header-inline consumers. CNA's asset cache consumes the shared
internal character rule and retains its lowercase ASCII diagnostic key spelling;
non-ASCII diagnostic keys may change bytes and length. Existing callers should
request the key size before copying it.
