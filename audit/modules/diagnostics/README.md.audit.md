# Audit: `modules/diagnostics/README.md`

## Metadata

- AUDITED: component description and ownership documentation.
- Evidence: README, module CMake declaration, and public include inventory.

## Assessment

The README identifies the component but omits its POSIX Process restriction and
the deliberately partial debugger/metadata adaptation. The public Process
header carries the material scope instead; this is a documentation gap, not an
independent behavioral finding.

## Other missing assertions and diagnostics

- Document POSIX-only process support, process lifetime rules, and passive C++
  metadata attributes next to the component summary.

## Final assessment

No standalone finding. No source or test changed.
