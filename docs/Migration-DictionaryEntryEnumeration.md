<!-- SPDX-License-Identifier: MIT -->
# SAMPLE-104: Dictionary entry enumeration and Framework diagnostics

Dictionary now enumerates live .NET entry slots rather than unordered hash buckets. Fresh entries
append, removals make slots available in LIFO order, rehashing preserves order, and TrimExcess
compacts live slots. Original .NET Framework 4/x86 probes are retained under
`/rv/tmp/samples/SAMPLE-104-PerformanceUtility_4_0/evidence/implementation-20260928/`.
The measured seven-command and two-removal sequences are pinned in general collection tests.
No application-specific name, registration sequence, comparator or sort was introduced.

SA-3 layout change on this LP64 libstdc++ checkout: Dictionary<int,int> **64 → 176 bytes**,
alignment **8 → 8**. Published iterator **24 → 24**, alignment **8 → 8**. No public signature,
namespace, mangled symbol, noexcept or vtable changes. Rebuild **every consumer**; mixing old/new
headers violates the ODR. The prior 64-byte pin is superseded by the new before/after record and
176-byte layout regression. Existing fail-fast rules remain (including removal/clear); arrow now
checks the same version as dereference. Copy/move/assignment own their entry keys and indices.

Mutable ToMap remains the same STL compatibility API/type. Raw external insertions cannot supply
an insertion history: preserve known slots and reconcile unknown keys in the backing map order.
Normal Dictionary APIs retain average O(1) updates/lookups; reconciliation runs only after mutable
ToMap has been exposed. Raw key-set changes are detected before enumerator access; erase/reinsert with the same final
key set cannot reveal its insertion history through this C++ escape hatch. Existing raw
MapType consumers retain their source spelling.

System.String.Substring now validates startIndex before length, reports the correct parameter,
uses the .NET diagnostic resource, and subtracts after checking bounds to avoid sum overflow.
Sharp Runtime's default ArgumentException format remains modern .NET. The opt-in AppContext switch
`SharpRuntime.UseNetFrameworkArgumentExceptionMessages` reproduces .NET Framework's separate
`Parameter name:` line and its negative Substring diagnostics, with no validation/type change.
CNA's XNA 4 host selects this compatibility mode; explicit caller switch values are respected.

Full component gate: **18,120/18,120 across 41 executables**, zero failed/skipped, with the
unchanged original Yacht SOAP fixture. Eleven new collection and five Framework tests pass.
The module-boundary validator retains exactly two inherited findings on both HEAD and working
source (Xml.Serialization/Core.Base and ServiceModel/Net.Http visibility), zero new findings;
that separate check is not green. See SAMPLE-104 evidence for baseline comparison and build logs.

Validation precedence was also cross-checked against Microsoft’s [Framework reference source](https://github.com/microsoft/referencesource/blob/main/mscorlib/system/string.cs#L1163).
