# Audit: `modules/runtime/include/System/Runtime/InteropServices/PosixSignalContext.hpp`

## Metadata

- AUDITED: 41-line inline event-data declaration, fully read.
- Validation: `PosixSignalContextTests.*` passed 2/2 on 2026-07-27.
- Reference basis: local current-.NET `PosixSignalContext.cs`.

## Assessment

Construction establishes the represented signal and `Cancel` defaults to
false, matching the managed data contract.  C++ needs a public
`setSignalProperty` for the external registration implementation to stamp the
shared context before each reverse-ordered callback; the header expressly
documents that the corresponding managed setter is internal.  This exposes a
native mutability route that .NET callers lack, but it is a visible,
implementation-required adaptation rather than evidence of an accidental
state error in this small data object.

## Other missing assertions and diagnostics

- The direct tests omit `setSignalProperty`, returning Cancel to false,
  repeated mutation, and construction from every enum/raw signal value.
- No registration-level assertion verifies that one shared context preserves a
  prior handler's `Cancel=true` while changing its `Signal` to each handler's
  registered value, as the implementation claims.
- Neither test is a cross-thread lifetime diagnostic for a context passed by
  reference from the watcher thread; source-level registration evidence is
  required for that boundary.

## Final assessment

The represented constructor/getter/Cancel behavior is coherent.  No source or
test was modified.
