# Audit: `modules/diagnostics/tests/System/Diagnostics/ProcessTests.cpp`

## Metadata

- AUDITED: primary Process lifecycle fixture.
- Evidence: target run, 11/11 tests passed; direct native Process probes.

## Assessment

The fixture covers uncomplicated start, wait, kill, redirection, and directory
paths. It misses every confirmed adverse lifecycle boundary: negative timeout
validation (SR-AUD-268), zombie/blocking destruction (SR-AUD-269), redirected
restart termination (SR-AUD-270), concurrent reader state (SR-AUD-271), EINTR
(SR-AUD-272), detached descendants (SR-AUD-273), and fork safety (SR-AUD-274).

## Other missing assertions and diagnostics

- Promote the seven direct probes to bounded subprocess tests with timeouts and
  stable native error assertions.

## Final assessment

SR-AUD-268 through SR-AUD-274 apply. No source or test changed.
