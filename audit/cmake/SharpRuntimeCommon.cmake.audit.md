# Audit: `cmake/SharpRuntimeCommon.cmake`

## Metadata

- Audit status: AUDITED (28 lines, full read).
- Subsystem: common target configuration.
- Evidence: root/component CMake flow and the warning-free GCC 14.2 build.

## Purpose

Creates the vendor include-only `SharpRuntime::Headers` target, requires C++23,
and applies the repository warning policy to compiled targets.

## Assessment

The vendor path is intentionally `SYSTEM`, preventing third-party warnings from
being treated as project warnings.  First-party targets receive `-Wall`,
`-Wextra`, and `-Werror` on non-MSVC toolchains, while the GNU-only
`-Wno-format-truncation` suppression is correctly not passed to Clang.  MSVC
uses the corresponding `/W4 /WX` policy.

## Findings

None.

## Final assessment

Small, correctly factored policy helper.  The successful zero-warning build is
direct evidence that the policy is currently applied to the configured graph.
