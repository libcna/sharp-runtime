# Audit: `modules/console/tests/System/Batch10ConsoleTests.cpp`

## Metadata

- AUDITED: extended output, cursor/window/buffer, keyboard/cancel, and beep
  smoke tests.
- Validation: complete Console fixture passed 123/123.

## Assessment

The tests exercise stored cursor/window/buffer state and documented stubs, but
mostly assert no throw and do not capture terminal effects or invalid inputs.

## Other missing assertions and diagnostics

- Add invalid color/cursor coordinates (SR-AUD-243/244), capture generated
  ANSI sequences, terminal bounds/TTY behavior, stub diagnostics, and real
  cancel/key handling rather than only stored state.

## Final assessment

Current cases pass but do not expose SR-AUD-243/244. No source or test changed.
