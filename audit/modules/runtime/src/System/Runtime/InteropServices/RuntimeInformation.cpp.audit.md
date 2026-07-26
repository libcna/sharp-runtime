# Audit: `modules/runtime/src/System/Runtime/InteropServices/RuntimeInformation.cpp`

## Metadata

- AUDITED: 94-line platform implementation, fully read.
- Validation: shared Architecture/OSPlatform/RuntimeInformation filter passed
  11/11 on 2026-07-27.
- Reference basis: local `RuntimeInformation.Windows.cs` and Unix sources.

## SR-AUD-154 — medium — Windows OSArchitecture aliases ProcessArchitecture instead of querying the native operating system

The Windows branch returns `getProcessArchitectureProperty()` immediately.
Current .NET uses IsWow64Process2 or GetNativeSystemInfo to report native OS
architecture, which can differ for a 32-bit/WOW64 process. C++ therefore
reports X86 under a 32-bit process on an X64/Arm64 OS rather than the required
OS architecture. Linux tests cannot exercise this compile-time Windows branch.

## Other missing assertions and diagnostics

- The unknown compile-target fallback fabricates X64 instead of refusing an
  unsupported target as current .NET's build-time mapping does.
- Existing tests only run on x86_64 Linux and assert process/OS equality; no
  cross-bitness, Windows, Emscripten, or uname failure test exists.
- The Unix uname mapping is appropriate for the exercised x86_64 sandbox, but
  architecture aliases outside the listed machine strings remain untested.

## Final assessment

Linux normal-path behavior is green, but Windows native architecture reporting
is observably wrong for supported mixed-bitness execution. No source or test
was modified.
