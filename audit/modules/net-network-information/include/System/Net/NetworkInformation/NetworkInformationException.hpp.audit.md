# Audit: `modules/net-network-information/include/System/Net/NetworkInformation/NetworkInformationException.hpp`

## Metadata

- AUDITED: POSIX-native-error exception adaptation.
- Validation: error-code and message fixture cases passed.

## Assessment

`errno` is an explicit practical replacement for the unavailable P/Invoke
last-error source. Its absent cause-bearing base constructor is already causal
context for SR-AUD-250, not a separate duplicate finding here.

## Other missing assertions and diagnostics

- Test current-errno capture, HResult/native-code relation, and the causal
  exception path reached by Ping failures.

## Final assessment

No separate finding is added beyond SR-AUD-250. No source or test changed.

## Post-audit remediation — ticket #1875 (2026-08-01)

The shared Win32 root now supplies current .NET's inherited `E_FAIL`
(`0x80004005`) instead of `COR_E_EXCEPTION`; a permanent derived control pins
that value separately from `NativeErrorCode`. The POSIX errno adaptation and
SR-AUD-250 context remain unchanged.
