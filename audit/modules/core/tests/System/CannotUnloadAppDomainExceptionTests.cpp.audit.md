# Audit: `modules/core/tests/System/CannotUnloadAppDomainExceptionTests.cpp`

## Metadata

- AUDITED: 28-line dedicated fixture, fully read.
- Validation: the selected six-suite filter passed 31/31 on 2026-07-26; this
  source owns four named cases, while three same-suite duplicate cases come
  from another test source.

## Findings

The owned cases cover normal text, SystemException catchability, and outer
message construction. They do not check the distinct CannotUnloadAppDomain
HResult, leaving the already confirmed SR-AUD-094 defect green.

## Missing assertions and diagnostics

- Missing `COR_E_CANNOTUNLOADAPPDOMAIN` checks for every constructor.
- Missing default exact text, C-string/null/UTF-8, inner identity/rethrow,
  std::exception, copy/move, and AppDomain-unload consumer paths.
- The inner test checks only outer text, not retained cause diagnostics.

## Final assessment

Useful ordinary constructor smoke coverage; SR-AUD-094 remains unguarded. No
source or test was modified during this audit.
