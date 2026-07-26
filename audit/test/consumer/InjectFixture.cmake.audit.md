# Audit: `test/consumer/InjectFixture.cmake`

## Metadata

- Audit status: AUDITED (45 lines, full read).
- Subsystem: injected direct-consumer fixture.
- Evidence: CMake source and `scripts/check_selective_components.sh` flow.

## Purpose

Uses `CMAKE_PROJECT_INCLUDE` and a deferred callback to add one consumer
executable after the selected runtime target has been configured.

## Assessment

The global scheduling guard prevents duplicate fixtures, and the deferred call
is scoped to the source directory that owns the runtime configuration.  The
fixture links only the requested component and inherits no test-only target.

## Findings

None.

## Final assessment

Focused and appropriate for proving that a selective runtime configuration can
serve a real direct consumer.
