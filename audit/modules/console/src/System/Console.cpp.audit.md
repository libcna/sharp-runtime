# Audit: `modules/console/src/System/Console.cpp`

## Metadata

- AUDITED: input/output/error redirection detection and terminal-size platform
  queries/fallbacks.
- Validation: complete Console fixture passed 123/123 in a redirected sandbox.

## Assessment

The platform branches use isatty/_isatty and native terminal-size queries with
documented redirected/Emscripten fallback values.  The current environment
validates only fallback behavior; a real TTY and Windows/Emscripten matrix is
still required for the platform branches.

## Other missing assertions and diagnostics

- Add pseudo-TTY/pipe/file descriptor fixtures, descriptor-closure errors,
  resize changes, Windows console-handle failure coverage, and Emscripten
  redirection behavior.

## Final assessment

No implementation defect was demonstrated. No source or test was changed.
