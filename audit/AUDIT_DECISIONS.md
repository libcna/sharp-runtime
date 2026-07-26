# Audit decisions

| ID | Decision | Rationale |
|---|---|---|
| SR-AD-001 | Audit all tracked first-party text-like files, including relevant project/planning documentation. | This matches the requested repository-wide scope and makes documentation claims independently reviewable. |
| SR-AD-002 | Exclude `vendor/**`, legal text, VCS placeholders, local/generated artifacts, and the audit output itself. | These are not authored runtime behavior; the deterministic counts are recorded in `AUDIT_SCOPE.md`. |
| SR-AD-003 | Keep this phase evidence-only. | The user requested a complete, reproducible audit before remediation; defects, missing assertions, and missing diagnostics become indexed follow-up work rather than inline changes. |
| SR-AD-004 | Compare .NET-shaped public APIs with the local dotnet/runtime source where available. | The repository's own contributor rules designate it as the authoritative parity source. |
| SR-AD-005 | Close the evidence-only audit after deterministic mirror reconciliation, rather than starting remediation immediately. | All 1,748 eligible files now have reports; the user requested audit-first, repair-second workflow. |
| SR-AD-006 | Keep the sandbox-local Net.Http socket failures as an environment-limited validation requirement. | Disabling or weakening those tests would conceal a permission limitation rather than establish a source result. |
