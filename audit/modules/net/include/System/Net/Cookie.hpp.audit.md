# Audit: `modules/net/include/System/Net/Cookie.hpp`

## Metadata

- Audit status: AUDITED.
- Reference: [Cookie.cs](/rv/tmp/runtime/src/libraries/System.Net.Primitives/src/System/Net/Cookie.cs).

## Assessment

Name validation and simple equality are present.  The overloads that receive
path and domain write the fields directly but leave their implicit flags set.

### SR-AUD-306 — medium — path/domain supplied to `Cookie` constructors are subsequently treated as implicit defaults

`Cookie(name, value, path, domain)` assigns `path_` and `domain_` directly,
but leaves `pathImplicit_` and `domainImplicit_` true.  `CookieContainer::Add`
therefore replaces both supplied values with the request URI's host and full
path.  The constructor arguments must establish explicit cookie identity just
as the corresponding setters do.

Required remediation: route constructor values through the setters or set the
implicit flags consistently; add tests that distinguish constructor values
from URI defaults.

## Missing assertions and diagnostics

No tests construct a cookie with the path/domain overload and then add it to a
container.  Add header-emission assertions for both constructor and setter
forms.

## Final assessment

Confirmed constructor-state mismatch.
