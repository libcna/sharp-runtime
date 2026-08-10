# Audit: `modules/console/CMakeLists.txt`

## Metadata

- AUDITED: static Console component registration and Core.Base dependency.
- Validation: `SharpRuntimeTests_Console` built and passed 123/123.

## Assessment

The registration matches the public headers and platform-probe source.  The
fixture is present and exercises both inline API and compiled redirection/window
helpers.

## Other missing assertions and diagnostics

- Add captured stdout/stderr tests, redirected and TTY-platform matrix runs,
  invalid console property inputs, and signal/cancel-key behavior tests.

## Final assessment

Build registration is coherent. No source or test was changed during this audit.
