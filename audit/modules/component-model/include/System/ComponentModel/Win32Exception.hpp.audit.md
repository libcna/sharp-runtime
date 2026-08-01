# Audit: `modules/component-model/include/System/ComponentModel/Win32Exception.hpp`

## Metadata

- AUDITED: native error-code and message storage adapter.
- Evidence: WebSocketException consumers and current .NET constructor surface
  were reviewed.

## Assessment

This is a reduced native-error value adapter.  It lacks managed cause-bearing
constructors, so a derived public WebSocketException overload currently loses
its cause; that concrete reachable effect is tracked as SR-AUD-250.

## Other missing assertions and diagnostics

- Add direct error/message/HResult/cause tests and distinguish platform-native
  error translation from plain message storage.

## Final assessment

No separate finding is added beyond SR-AUD-250. No source or test changed.

## Post-audit correction and remediation — ticket #1875 (2026-08-01)

Current .NET makes no direct HResult assignment here because Win32Exception
inherits `E_FAIL` (`0x80004005`) from `ExternalException`. This reduced port
derives from `Exception`, so the same omission produced `COR_E_EXCEPTION`.
Both represented constructors now assign the inherited reference value
directly, without changing the public base or any declaration. The permanent
matrix also pins NetworkInformation, Socket and WebSocket derived controls.
SR-AUD-250 remains confirmed and unchanged.
