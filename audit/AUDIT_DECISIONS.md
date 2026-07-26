# Audit decisions

| ID | Decision | Rationale |
|---|---|---|
| SR-AD-001 | Audit all tracked first-party text-like files, including relevant project/planning documentation. | This matches the requested repository-wide scope and makes documentation claims independently reviewable. |
| SR-AD-002 | Exclude `vendor/**`, legal text, VCS placeholders, local/generated artifacts, and the audit output itself. | These are not authored runtime behavior; the deterministic counts are recorded in `AUDIT_SCOPE.md`. |
| SR-AD-003 | Keep this phase evidence-only. | The user requested a complete, reproducible audit before remediation; defects, missing assertions, and missing diagnostics become indexed follow-up work rather than inline changes. |
| SR-AD-004 | Compare .NET-shaped public APIs with the local dotnet/runtime source where available. | The repository's own contributor rules designate it as the authoritative parity source. |
