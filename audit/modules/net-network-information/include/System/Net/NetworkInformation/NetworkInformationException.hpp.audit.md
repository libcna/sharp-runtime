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
