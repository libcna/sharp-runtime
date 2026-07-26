# Audit: `modules/buffers/tests/System/Buffers/BinaryPrimitivesTests.cpp`

## Metadata

- Audit status: AUDITED (308 lines, thirty-seven tests, fully read).
- Validation: `BinaryPrimitivesTest.*` passed 37/37 in the combined direct
  `SharpRuntimeTests_Buffers` filter on 2026-07-26.  The filter's three direct
  fixtures passed 54/54.
- Related implementation: `BinaryPrimitives.hpp`; the local .NET
  `BinaryPrimitives` read/write/reverse-endian sources were the comparison
  baseline used by its implementation report.

## Assessment

This is a substantially stronger companion fixture than the earlier mixed
batch: it exercises scalar and 128-bit round trips, several explicit byte
orders, short-span throwing and false paths, and signed/unsigned reverse
endianness.  The passing result supports the currently compiled little-endian
GCC/Clang surface, but mostly through round trips that can conceal matching
read/write errors.

## Finding references

No new independently reproducible implementation finding was established.
The test coverage gaps below refine the evidence limitations already listed in
`BinaryPrimitives.hpp.audit.md`.

## Other missing assertions and diagnostics

- Most scalar write paths are checked only by a matching reader.  Add
  byte-exact known-vector assertions for signed minima, unsigned maxima,
  `-0`, subnormals, infinities, and NaN payloads in both byte orders.
- False `TryWrite` tests do not assert that every destination byte remains
  unchanged.  False `TryRead` checks cover only selected types/directions and
  do not consistently assert the required zero/default output.
- Throwing short-destination coverage is limited; it omits every primitive
  width/order family, `Write*`, and most 128-bit overloads.  Neither exception
  parameter names nor oversize-source behavior are inspected.
- The 128-bit coverage has round trips and one short input only.  It lacks
  known vectors with nonzero upper/lower halves for all four read/write byte
  orders, false big-endian reads/writes, and signed-negative values.
- No test runs the big-endian helper branch, validates the MSVC exclusion of
  128-bit overloads, or checks that compiler/platform feature diagnostics are
  clear to a public consumer.

## Final assessment

All thirty-seven cases pass and exercise the implemented native path well, but
byte-exact boundary and cross-platform diagnostics remain materially weaker
than the API breadth.  No source or test was modified during this audit.
