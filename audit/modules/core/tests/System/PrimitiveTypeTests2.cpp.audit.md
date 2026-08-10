# Audit: `modules/core/tests/System/PrimitiveTypeTests2.cpp`

## Metadata

- AUDITED: 191 Byte/SByte/Int16/UInt16/Boolean/Char/Single/Double cases.
- Validation: the complete Core.Base fixture passed 4,946/4,946; owning type
  reports and prior native/current-.NET probes were reviewed.

## Assessment

The fixture checks many ordinary constants, parsing paths, formatting samples,
and floating classifications.  It does not cover the full invalid-input and
boundary matrix needed to detect the already confirmed small-integral format
and Clamp validation gaps, Char malformed-UTF-8 acceptance, or the floating
rounding/NaN/subnormal differences in their owning reports.

## Other missing assertions and diagnostics

- Add malformed/overlong UTF-8, surrogate, and Unicode-category cases for
  Char; do not treat a few valid UTF-8 samples as decoder validation.
- Add inverted Clamp bounds, unknown format strings, binary `B` parity for
  every supported wrapper, extrema arithmetic, and `IsPositive(+NaN)`.
- Add invalid rounding precision/mode, subnormal power, NaN payload/sign, and
  culture-sensitive parse/format checks for Single and Double.

## Final assessment

The fixture does not contradict the prior primitive findings but leaves their
edge-case regression coverage incomplete. No source or test was changed.
