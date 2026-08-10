# Audit: `modules/runtime/include/System/Runtime/InteropServices/PosixSignal.hpp`

## Metadata

- AUDITED: 52-line public enum declaration, fully read.
- Validation: the dedicated POSIX fixture passed 9/9 on 2026-07-27.
- Reference basis: local current-.NET `PosixSignal.cs` and the Unix
  `PosixSignalRegistration` implementation.

## Assessment

The eleven represented values exactly preserve the managed negative numeric
values.  The `Sighup`/`Sigint` spelling change is necessary and explicitly
explained: literal `SIGHUP`-style enumerators are unusable in a translation
unit that has included the POSIX macro names.  This is a documented native
adaptation, not a finding.

The enum itself intentionally remains the public carrier for raw native
integer casts; whether those casts are accepted is an implementation issue in
`PosixSignalRegistration.cpp` (SR-AUD-170), not a numeric declaration error.

## Other missing assertions and diagnostics

- Tests cover only `Sigwinch`, `Sigterm`, `Sigkill`, `Sighup`, and `Sigint`.
  They do not assert the remaining declared numeric values or that the
  documented macro-safe names stay available after including `<signal.h>`.
- No compile probe documents the current-.NET Unix contract that a positive
  raw native signal number cast to this enum is accepted when the OS supports
  it; the registration probe exposes the implementation gap instead.
- The managed Windows-platform annotations have no equivalent C++ availability
  metadata.  The platform fallback is reviewed with the registration source.

## Final assessment

The declared named values and its forced, documented C++ spelling adaptation
are coherent.  No source or test was modified.
