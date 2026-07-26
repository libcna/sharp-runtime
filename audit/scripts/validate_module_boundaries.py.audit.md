# Audit: `scripts/validate_module_boundaries.py`

## Metadata

- Audit status: AUDITED (665 lines, full read).
- Subsystem: component ownership/dependency validator.
- Evidence: implementation, its unittest fixtures, and successful invocation
  reporting 41 modules and 90 production edges.

## Purpose

Parses module registrations and project-local includes without configuring a
build; validates ownership, duplicate public include paths, declared
visibility, stale dependencies, allow-list consistency, cycles, and
test-only dependency use.

## Assessment

The validator checks both directions of the contract: actual includes must be
declared at the right visibility, and declared dependencies must have actual
use unless a reasoned allow-list entry exists.  It separately analyses public
headers, implementation sources, and test sources.  The current repository
passes it cleanly.

## Findings

### SR-AUD-002 — medium — validator unit fixtures cover only a narrow subset of enforced invariants

`test/validate_module_boundaries_test.py` has seven fixture tests.  They cover a
valid repository, unregistered module, duplicate header, undeclared public
include, stale dependency, cycle, and public/private visibility.  The
validator additionally enforces unresolved project includes, missing/unknown
module registrations, private-source dependency visibility, unused
test-only dependencies, unknown test dependencies, and all allow-list parsing
and visibility paths; none of those behaviors has an isolated negative
fixture.

**Impact:** the real-tree invocation detects current violations, but a future
change in an unfixture-tested validation branch can silently weaken an
architecture guarantee while the small test suite remains green.

**Follow-up evidence needed:** add minimal temporary-repository fixtures for
each untested error family, especially unresolved `System/` includes,
test-only closure/staleness, and malformed or visibility-mismatched allow-list
entries.

## Positive findings

The parser handles balanced CMake calls and quoting rather than relying on
line-oriented regexes, and it reports a concrete first source location for
undeclared include edges.

## Final assessment

The production validator is strong and currently passes; its isolated
negative-test coverage needs expansion (SR-AUD-002).
