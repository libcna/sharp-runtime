# plan.md — sharp-runtime planning index
*Last updated: 2026-06-13 (session 70) — 3939 tests passing*

sharp-runtime is a C++23 static library reimplementing a practical subset of .NET `System.*` for **CNA** (C++ XNA port) and **mobile-eggbert** (ported Windows Phone game).

Reference source: `/rv/tmp/runtime/src/libraries/` (dotnet/runtime, MIT License)

---

## Planning documents

| File | Contents |
|------|----------|
| [plan_namespaces.md](plan_namespaces.md) | All 311 .NET namespaces with status (todo / ignore / ported / in_progress) |
| [plan_files.md](plan_files.md) | Individual .NET reference `.cs` files with porting status |

---

## Legend

| Status | Meaning |
|--------|---------|
| `ported` | Implemented in sharp-runtime — good coverage |
| `in_progress` | Partially implemented, work ongoing |
| `todo` | Needs to be ported/implemented |
| `ignore` | Out of scope for sharp-runtime |
