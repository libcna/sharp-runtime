<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# `System::Numerics::BFloat16` — final surface disposition (ticket #2340, SR-AUD-176)

The original 2026-08-19 decomposition split SR-AUD-176 into a bounded sibling-parity repair and
a policy decision. Both parts have since landed:

- **#2382** brought `BFloat16` to the practical value-type line then implemented by `Half`, adding
  classification, ordering, identity, parsing and formatting. It also removed the second formatter
  that emitted C spellings for NaN and infinity. See
  `docs/Migration-BFloat16SurfaceAndFormatter.md`.
- **#2384** made the policy decision for both 16-bit floats together and added the supported
  `MathF`-forwarding and ordinary conversion surface. Raw-bit construction was separated from
  numeric conversion by #2395. See `docs/Migration-SixteenBitFloatMathSurface.md` and
  `docs/Migration-SixteenBitFloatRawBitsIsNamed.md`.

The planning database originally stored the first row as `ticket_no=2383` even though its source,
tests and migration record consistently identify it as #2382. The final audit reconciliation
corrected that metadata; #2384 remains the distinct policy ticket. There is no #2383 planning
ticket to cite.

## What the measurement established

The initial finding compared the port with all 193 public members and 36 interfaces of current
.NET `BFloat16`. That is not the project's compatibility boundary: sharp-runtime implements a
practical C++ subset and its sibling type `System::Half` is the relevant local consistency check.
The initial header diff found the bounded value gap, and #2382 closed it. #2384 then deliberately
moved both sibling types together instead of adding one-sided convenience APIs.

The current supported surface includes:

- whole-domain classification and IEEE sign/magnitude helpers;
- value ordering, equality and hashing with `BFloat16`'s own reference semantics;
- parsing, formatting and `TryFormat` through the runtime's single-precision implementation;
- the practical rounding, root, trigonometric, exponential and related static math families;
- ordinary conversions supported by the runtime, with explicit C++ conversion rules where
  implicit managed conversions would create overload ambiguity.

`sizeof(BFloat16)` remains two bytes. Regression coverage exhausts all 65,536 bit patterns for the
classification partition and pins formatting, parsing, identity, conversions and the sibling-type
policy.

## Permanent subset boundary

Generic-math interface conformance such as `INumber<BFloat16>` and
`IFloatingPointIeee754<BFloat16>`, C# static-abstract interface machinery, `checked` conversion
operators, and managed numeric types that sharp-runtime does not otherwise expose remain outside
the practical C++ subset. They are language/framework machinery rather than missing internal
algorithms, and the same boundary is documented on `Half` and `BFloat16`.

SR-AUD-176 is therefore an **accepted deviation**, not an open implementation item: the
actionable, internally consistent surface is implemented, while the excluded generic-math layer is
named explicitly instead of masquerading as forgotten work.
