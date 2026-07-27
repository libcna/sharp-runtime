# Sharp Runtime audit final report

## Outcome

The evidence-only repository audit is complete. Every one of the 1,748
eligible tracked first-party text-like files has exactly one mirrored report
under `audit/<source-path>.audit.md`; the 1,699 runtime-module files are also
fully covered. No production source or test was changed during this phase.

At audit closure, the findings index recorded 364 confirmed issues. It now
retains all 364 entries while marking 361 `confirmed` and three `remediated`:

| Severity | Count |
|---|---:|
| High | 91 |
| Medium | 262 |
| Low | 11 |
| **Total** | **364** |

The index is the remediation inventory, while per-file reports contain the
source-level evidence, missing assertions/diagnostics, and focused target for
each finding. `AUDIT_CROSS_CUTTING_FINDINGS.md` identifies causes that require
coordinated repairs rather than isolated symptom patches.

## Closure evidence

- Mirror reconciliation: 1,748 reports for 1,748 eligible files; no missing
  source-path mirror and every newly completed report has an `AUDITED` status.
- Focused audit validations: `SharpRuntimeTests_Collections_Core` 1,422/1,422;
  `SharpRuntimeTests_Xml` 377/377; `SharpRuntimeTests_IO` 527/527;
  `SharpRuntimeTests_Xml_Linq` 92/92; and
  `SharpRuntimeTests_Security_Cryptography` 80/80 at their respective
  checkpoints.
- Full configured build: `gmake -C build -j4` completed all registered
  backend/runtime libraries and test executables during final reconciliation.
- Audit controls: `python3 scripts/db_consistency_check.py --db plan.sqlite3`,
  `python3 scripts/validate_module_boundaries.py --root .`, and
  `git diff --check` passed during final reconciliation and are required again
  for each remediation change.
- Direct ASan/UBSan probes establish memory-safety findings including
  SR-AUD-338, SR-AUD-341, SR-AUD-356, SR-AUD-357, and SR-AUD-358. Functional
  probes establish the associated public-contract findings cited by the
  per-file reports.

During the audit phase, the broad local CI gate was environment-limited: six
`Net.Http` local-server tests fail at socket creation in this sandbox
(`Socket::Socket: socket() failed`). The tests remained enabled. The first
post-audit remediation batch later satisfied this prerequisite in a
network-permitted run; see the status below.

## Repair handoff

Do not repair from this report en masse. Create small, independently validated
tickets that preserve public compatibility and retain the audit evidence. Start
with high-severity memory safety and lifecycle boundaries, then repair grouped
root causes from the cross-cutting report. In particular, Collections findings
SR-AUD-356 through SR-AUD-358 need a design review because they affect shared
enumerator and raw-polymorphic APIs; patching one concrete collection would
leave sibling public paths unsafe.

The audit phase is closed. The next phase is user-approved post-audit
remediation planning, not further source changes under ticket #1766.

## Post-audit remediation status

Ticket #1767 completed the first bounded remediation batch on 2026-07-27.
SR-AUD-356 and SR-AUD-364 / CCF-018 are marked `remediated`; their original
audit evidence remains in place. A shared lifecycle guard now prevents invalid
`Current` access across the ten affected collection enumerators, and
`BitArray` uses mutation-version checks.

Closure evidence is 13/13 permanent focused regressions, 1,435/1,435
Collections.Core tests, a clean direct ASan/UBSan probe, and
`scripts/local_ci_check.sh build` in a network-permitted environment:
12,694/12,694 tests across 37 executables with zero build warnings/errors.
Boundary validation remains 41 physical modules and 90 dependency edges;
catalogue, database consistency, and diff checks pass. Doxygen 1.9.8 reports
1,941 warnings against the 1,942-warning ceiling. LeakSanitizer alone could
not initialize under the sandbox's `ptrace` policy, so its probe was rerun
with leak detection disabled while AddressSanitizer and UndefinedBehaviorSanitizer
remained active.

Design ticket #1768 and implementation ticket #1769 completed the second
bounded batch on 2026-07-27. SR-AUD-357 is marked `remediated` and CCF-019 is
partially remediated; the original audit evidence remains in place.
`LinkedListNode<T>` now refers to an independently allocated, reference-counted
node with an explicit null/detached/attached state, so removal, `Clear`, and
destruction of the owning `LinkedList<T>` detach the node and retain its value
instead of leaving a dangling `std::list` iterator. The selected contract is
recorded in `docs/LinkedListNodeLifetime.md`.

Closure evidence is 49 permanent regressions in
`LinkedListNodeLifetimeTests.cpp`, 1,484/1,484 Collections.Core tests, a
standalone `Collections.Core` public-header consumer fixture compiled with
`-Werror`, a direct ASan/UBSan/LeakSanitizer probe reporting `failures=0` with
no diagnostic, and a network-permitted `scripts/local_ci_check.sh build` run of
12,743/12,743 tests across 37 executables with zero build warnings/errors.
Boundary validation is unchanged at 41 modules and 90 edges; validator-test,
catalogue, database, selective-component, and diff checks pass, and Doxygen
1.9.8 remains at 1,941 warnings with no new warning from the touched headers.
The JsonNode (SR-AUD-327) and XML LINQ (SR-AUD-333) members of CCF-019 remain
open by design.

Design ticket #1770 completed the third bounded batch on 2026-07-27 and made no
production change. SR-AUD-358 / CCF-020 is **design-complete but still
`confirmed`**: the findings index therefore still records 361 open findings and
three `remediated`. The selected contract is recorded in
`docs/ICollectionCopyToDesign.md` — a length-aware, statically typed
`System::Span<std::any>` destination behind a non-virtual `ICollection`, so the
destination's capacity and element type are validated exactly once before any
implementation writes, with `CopyTo(void*, intcs)` leaving the virtual interface
and remaining briefly as a deprecated, never-writing shim. Seven repository-local
compile/sanitizer probes back the design: virtual templates are ill-formed;
removal cannot silently misbind; the full prototype is clean under
ASan/UBSan/LeakSanitizer with `-Werror`; derived-class name hiding requires
`using ICollection::CopyTo;`; the current boundary still reproduces three
sanitizer aborts plus one silent LeakSanitizer-only element-type corruption; the
affected public headers compile standalone against `Collections.Core` +
`Core.Base`; and a retained deprecated overload is a compile error under the
repository's own `-Werror` policy. Implementation is proposed as inactive ticket
#1771, gated on explicit user approval of the narrow public-API break.

Implementation ticket #1771 closed that batch on 2026-07-27, after the user
explicitly approved the public source- and ABI-breaking change. SR-AUD-358 /
CCF-020 is now `remediated`, so the findings index records **360 open findings
and four `remediated`**; the original audit evidence remains in place.
`virtual void CopyTo(void*, intcs) = 0` is removed from `ICollection` and
replaced by non-virtual, validating `CopyTo(ObjectSpan, intcs)` and
`CopyTo(std::vector<std::any>&, intcs)` over a single protected pure virtual
`copyToCore(ObjectSpan, intcs)` hook, with checked typed
`std::vector<void*>` / `std::vector<DictionaryEntry>` overloads on the concrete
collections. One departure from the design record is recorded in section 21 of
`docs/ICollectionCopyToDesign.md`: the deprecated, never-writing shim was **not**
retained, so a stale call site fails to compile rather than throwing at run time.
Because a pure virtual member was removed, every `ICollection`/`IList`/
`IDictionary` vtable changes and all consumers must be rebuilt; the consumer
guidance is in `docs/Migration-ICollectionCopyTo.md`, and the CNA /
mobile-eggbert sweep is inactive ticket #1773, as neither repository is in this
checkout.

Closure evidence is 128 permanent regressions in `CopyToBoundaryTests.cpp`
parameterised over every `ICollection` implementation (also 128/128 under
ASan + UBSan + LeakSanitizer), 1,612/1,612 Collections.Core tests, a standalone
`Collections.Core` public-header consumer fixture compiled with `-Werror` and
executed successfully, a replacement ASan/UBSan/LeakSanitizer probe reporting
`failures=0` with no diagnostic and no leak on the four scenarios that previously
crashed or leaked, a captured compile log showing that the old raw calls now
produce four `no matching function` errors naming the replacements, and a
`scripts/local_ci_check.sh`-equivalent run of 12,871/12,871 tests across 37
executables with zero build warnings/errors. Boundary validation is unchanged at
41 modules and 90 edges; validator-test, catalogue, database, selective-component,
and diff checks pass, and Doxygen 1.9.8 reports 1,941 warnings against the
1,942 ceiling.
