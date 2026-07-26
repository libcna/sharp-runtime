# Audit: `modules/core/include/System/Version.hpp`

## Metadata

- Audit status: AUDITED (227 lines, full read; header-only implementation).
- Public API: `System.Version` construction, parse, comparison, formatting,
  and revision properties.
- Validation: 54 Version tests passed in the focused Core.Base run.

## Assessment

The parser correctly distinguishes malformed, overflow, negative, and
trailing-separator cases; comparisons avoid subtraction overflow.  The
field-count formatting overload emits unspecified fields instead of rejecting
them.

## Finding

### SR-AUD-011 — medium — `Version::ToString(fieldCount)` serializes unspecified components as `-1`

`ToString(intcs fieldCount)` only validates the numeric interval 0–4.  When a
two-component version is asked for three fields, it appends `Build`, whose
sentinel value is `-1`; a three-component version asked for four fields does
the same for `Revision`.

**Reproduction (observed in the audit probe):**

```cpp
Version(1, 2).ToString(3); // current result: "1.2.-1"
```

.NET `Version.ToString(int)` throws `ArgumentException` when `fieldCount`
requests a component that the instance does not define.  The result above is
not a valid version representation and contradicts the API's specified
components.

**Required post-audit verification:** add throw assertions for
`Version(1,2).ToString(3/4)` and `Version(1,2,3).ToString(4)`, alongside valid
2- and 3-field cases.  Repair must reject field counts beyond the defined
component count.

## Final assessment

Parsing and comparison are robust; field-count formatting has a confirmed
observable parity defect (SR-AUD-011).
