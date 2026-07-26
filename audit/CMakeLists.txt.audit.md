# Audit: `CMakeLists.txt`

## Metadata

- Audit status: AUDITED (129 lines, full read).
- Subsystem: root build configuration.
- Evidence: root configuration, `cmake/SharpRuntimeModules.cmake`, and the
  initial `scripts/local_ci_check.sh build` run.

## Purpose

Defines the C++23 project, selects requested components, enables component
tests and the integration target, and exposes the aggregate `SharpRuntimeTests`
target.

## Assessment

The source-root default to `All`, the explicit component list for embedding,
and the selective-test construction are coherent with the component registry.
Test binaries are intentionally component-scoped, with integration tests only
when `All` is requested.  The root file delegates ownership/dependency logic to
the CMake helpers rather than duplicating it.

The initial audit gate configured successfully and built every target with zero
warnings.  It did not finish testing because the sandbox cannot create sockets;
that environment limitation is recorded in `AUDIT_PROGRESS.md`, not attributed
to this file.

## Findings

None in this file.

## Final assessment

No source-level defect found.  Revalidate the `All` and a selective
configuration during final audit reconciliation.
