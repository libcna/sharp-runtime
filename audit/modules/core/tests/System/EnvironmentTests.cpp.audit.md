# Audit: `modules/core/tests/System/EnvironmentTests.cpp`

## Metadata

- Audit status: AUDITED (677 lines, 99 tests, fully read).
- Validation: `EnvironmentTests.*` passed 99/99 in
  `SharpRuntimeTests_Core_Base` on 2026-07-26.
- Reference basis: local .NET Environment Unix behavior and the audited C++
  public surface.

## Assessment

The fixture has unusually broad normal-path coverage: variable-name validation,
current-directory success/failure, expansion, process data, selected folders,
command-line storage, CPU/OS values, and enum constants.  It nevertheless
uses self-consistency and current-machine assumptions for the paths where .NET
has configuration, error, or round-trip semantics.

The 99 green cases do not contradict SR-AUD-105 through SR-AUD-108.  In
particular, the source expects empty SetEnvironmentVariable to remove a key,
expects every folder option to equal the no-option path, and expects only a
raw whitespace-free command-line join.  Those assertions protect the C++
adaptation instead of the referenced public behavior.

## Other missing assertions and diagnostics

- Add controlled absolute `XDG_CONFIG_HOME`/`XDG_DATA_HOME` cases, invalid
  `SpecialFolder`/`SpecialFolderOption` cases, and `None`/`Create` behavior for
  a nonexistent temporary directory.
- Distinguish an empty-value entry from an absent entry through
  `GetEnvironmentVariables`, and add an explicitly representable deletion API
  once the public model supports it.
- Exercise a >4 KiB cwd, long process path, quote/whitespace/backslash command
  arguments, and a parse-back argument round trip.
- Assertions such as `Is64BitProcess_ReturnsBool`,
  `IsPrivilegedProcess_ReturnsBool`, `StackTrace_DoesNotThrow`, and broad
  nonempty/name checks provide little behavioral diagnostic when the platform
  implementation changes.

## Final assessment

The fixture is a valuable smoke suite but locks several known compatibility
gaps and leaves all reported boundary cases unasserted.  No test was modified
during this audit.
