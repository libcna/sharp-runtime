# Audit: `modules/security/include/System/Security/VerificationException.hpp`

## Metadata

- AUDITED: default/message construction relative to the ignored managed
  verification-exception API.
- Validation: direct probes observed native `Verification failed.` with
  `0x80131501`; current .NET reports `Operation could destabilize the runtime.`
  with `0x8013150D`.

## Assessment

The header is a minimal local exception placeholder, not a supported ported
surface: the project task matrix marks `System.Security.VerificationException`
ignored.  Its message, HResult, and missing inner-exception constructor differ
from current .NET, but that difference is not promoted to a remediation
finding while the type remains outside the supported API baseline.

## Other missing assertions and diagnostics

- The security fixture does not construct this header at all.  Add explicit
  placeholder coverage or remove the implication of supported parity.
- If the type is ever promoted from ignored, require default/message/inner
  construction and `COR_E_VERIFICATION` HResult parity before exposing it.

## Final assessment

The ignored-surface discrepancy is documented for a future scope decision. No
source or test was changed.

## Post-audit remediation — ticket #1875 (2026-08-01)

The explicitly requested 45-type sweep found current .NET's exact
`COR_E_VERIFICATION` (`0x8013150D`) assignment. Both represented local
constructors now assign it, moving the measured value from `COR_E_SYSTEM` while
leaving the ignored-surface classification, text, and missing causal overload
unchanged. No new audit identifier was issued.
