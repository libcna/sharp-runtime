# Audit: `modules/console/include/System/ConsoleKey.hpp`

## Metadata

- AUDITED: console-key numeric enum surface.
- Validation: representative control, alphanumeric, function, navigation, and
  numpad values passed in Console 123/123.

## Assessment

The native enum exposes the expected key-code vocabulary.  ReadKey is explicitly
partial and returns a character with `ConsoleKey::None`, so this declaration
does not imply terminal key decoding is implemented.

## Other missing assertions and diagnostics

- Add exhaustive enum parity, underlying-type, unknown-cast, actual raw key,
  modifier, Unicode, escape-sequence, and redirected-input coverage.

## Final assessment

No enum declaration defect was demonstrated. No source or test was changed.
