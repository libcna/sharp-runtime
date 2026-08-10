# Audit: `test/consumer/CMakeLists.txt`

## Metadata

- Audit status: AUDITED (127 lines, full read).
- Subsystem: external-consumer fixture project.
- Evidence: fixture CMake, injection helper, and selective-check script.

## Purpose

Builds either a normal direct consumer or a compile-only object fixture whose
include/compile usage requirements are recursively collected from the selected
component's public interface.

## Assessment

The compile-only path intentionally ignores non-target generator expressions,
including static-library `LINK_ONLY` private edges, so it models consumer-
visible requirements rather than accidentally inheriting private internals.
Visited-target tracking avoids recursive traversal loops.  Strict warning flags
ensure consumer fixtures do not pass merely because the runtime has looser
diagnostics.

## Findings

None.

## Final assessment

The fixture project correctly tests both linkable consumers and the narrower
header/usage surface required by negative include-leakage checks.
