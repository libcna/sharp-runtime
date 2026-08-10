# Audit: `modules/globalization/include/System/Globalization/IdnMapping.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [.NET AllowUnassigned](https://learn.microsoft.com/en-us/dotnet/api/system.globalization.idnmapping.allowunassigned?view=net-10.0)
  specifies that this setting affects mapping operations.

## Assessment

`allowUnassigned_` is stored, exposed through equality/hash, and never read by
mapping code.  A standalone probe maps the known unassigned U+0378 identically
with the setting false and true (`xn--zva.example`), proving it has no behavior.

### SR-AUD-282 — medium — IdnMapping.AllowUnassigned is a semantically inert public setting

Default false must reject unassigned Unicode according to its contract, while
true permits the mapping operation.  The local API accepts the same unassigned
input in both modes and changes no validation policy, so callers cannot enforce
their declared IDN input rule.

## Finding references

- SR-AUD-282 — medium — public IDN validation policy is not applied.

## Other missing assertions and diagnostics

- Test assigned and unassigned Unicode in both modes, code-point/version
  boundaries, STD3 interactions, and offset ranges that do not split UTF-8.

## Final assessment

SR-AUD-282 applies.
