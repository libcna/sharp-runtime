#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Apply or verify the final audit dispositions recorded for ticket #2417.

The per-file ``*.audit.md`` reports are immutable audit-time evidence.  The
current status and concise current-HEAD evidence live in the index, while
``audit/final_dispositions.json`` is the reproducible source for every row
reviewed by the final reconciliation.  This script keeps that source, the
index, and ``docs/AuditFindingsReconciliation.md`` in lockstep.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_PATH = REPO_ROOT / "audit/final_dispositions.json"
INDEX_PATH = REPO_ROOT / "audit/AUDIT_FINDINGS_INDEX.md"
DOCUMENT_PATH = REPO_ROOT / "docs/AuditFindingsReconciliation.md"
ROW_ID_RE = re.compile(r"SR-AUD-(\d{3})")
OPEN_STATUSES = {"confirmed", "confirmed (design-complete)"}
BASELINE_COMMIT = "115ce1b0"
EXPECTED_PREVIOUSLY_OPEN_IDS = {
    "001", "002", "003", "004", "005", "013", "014", "042", "081", "087", "088",
    "092", "098", "124", "125", "139", "140", "141", "142", "148", "149", "152",
    "153", "154", "158", "159", "160", "161", "163", "164", "165", "166", "167",
    "168", "173", "174", "176", "182", "186", "191", "193", "194", "196", "201",
    "203", "208", "209", "215", "219", "220", "228", "235", "239", "246", "259",
    "273", "279", "281", "282", "283", "284", "285", "294", "308", "317", "324",
    "325", "327", "333", "336", "362",
}
STATUS_ORDER = (
    "remediated",
    "accepted-deviation",
    "false-positive",
    "external-blocked",
    "confirmed",
    "confirmed (design-complete)",
)


def load_dispositions() -> list[dict[str, str]]:
    payload = json.loads(DATA_PATH.read_text(encoding="utf-8"))
    if payload.get("schema_version") != 1:
        raise ValueError(f"{DATA_PATH}: unsupported schema_version")
    if payload.get("baseline_commit") != BASELINE_COMMIT:
        raise ValueError(
            f"{DATA_PATH}: baseline_commit must remain {BASELINE_COMMIT} for this reconciliation"
        )
    findings = payload.get("findings")
    if not isinstance(findings, list):
        raise ValueError(f"{DATA_PATH}: findings must be an array")
    seen: set[str] = set()
    for position, item in enumerate(findings):
        if not isinstance(item, dict):
            raise ValueError(f"{DATA_PATH}: finding {position} must be an object")
        missing = {"id", "group", "status", "evidence"} - item.keys()
        if missing:
            raise ValueError(
                f"{DATA_PATH}: finding {position} misses {', '.join(sorted(missing))}"
            )
        finding_id = item["id"]
        if not isinstance(finding_id, str) or not re.fullmatch(r"\d{3}", finding_id):
            raise ValueError(f"{DATA_PATH}: invalid finding id {finding_id!r}")
        if finding_id in seen:
            raise ValueError(f"{DATA_PATH}: duplicate SR-AUD-{finding_id}")
        seen.add(finding_id)
        if item["status"] in OPEN_STATUSES:
            raise ValueError(
                f"{DATA_PATH}: final disposition for SR-AUD-{finding_id} is still open"
            )
        if item["status"] not in STATUS_ORDER:
            raise ValueError(
                f"{DATA_PATH}: unsupported status {item['status']!r} for SR-AUD-{finding_id}"
            )
        if not isinstance(item["evidence"], str) or not item["evidence"].strip():
            raise ValueError(f"{DATA_PATH}: SR-AUD-{finding_id} has no evidence")
    group_counts = Counter(item["group"] for item in findings)
    if group_counts != Counter(
        {
            "previously-open": 71,
            "special-consistency": 1,
            "metadata-correction": 1,
            "stale-summary-correction": 4,
        }
    ):
        raise ValueError(
            f"{DATA_PATH}: expected 71 reviewed + 6 correction records, got {dict(group_counts)}"
        )
    reviewed_ids = {
        item["id"] for item in findings if item["group"] == "previously-open"
    }
    if reviewed_ids != EXPECTED_PREVIOUSLY_OPEN_IDS:
        missing = sorted(EXPECTED_PREVIOUSLY_OPEN_IDS - reviewed_ids)
        unexpected = sorted(reviewed_ids - EXPECTED_PREVIOUSLY_OPEN_IDS)
        raise ValueError(
            f"{DATA_PATH}: baseline open-ID set changed; missing={missing}, unexpected={unexpected}"
        )
    return findings


def rewrite_index(text: str, dispositions: list[dict[str, str]]) -> str:
    by_id = {item["id"]: item for item in dispositions}
    found: set[str] = set()
    output: list[str] = []
    for line in text.splitlines(keepends=True):
        match = ROW_ID_RE.search(line) if line.startswith("| [SR-AUD-") else None
        if match is None or match.group(1) not in by_id:
            output.append(line)
            continue
        finding_id = match.group(1)
        parts = line.rstrip("\n").split(" | ", 4)
        if len(parts) != 5 or not parts[4].endswith(" |"):
            raise ValueError(f"cannot parse index row SR-AUD-{finding_id}")
        item = by_id[finding_id]
        parts[2] = item["status"]
        parts[4] = item["evidence"].strip() + " |"
        output.append(" | ".join(parts) + "\n")
        found.add(finding_id)
    missing = sorted(by_id.keys() - found)
    if missing:
        raise ValueError("index misses dispositions: " + ", ".join(missing))
    return "".join(output)


def parse_index_rows(text: str) -> list[tuple[str, str]]:
    rows: list[tuple[str, str]] = []
    for line in text.splitlines():
        if not line.startswith("| [SR-AUD-"):
            continue
        match = ROW_ID_RE.search(line)
        parts = line.split(" | ", 4)
        if match is None or len(parts) != 5:
            raise ValueError(f"cannot parse audit index row: {line[:120]}")
        rows.append((match.group(1), parts[2]))
    return rows


def render_document(
    dispositions: list[dict[str, str]], index_text: str
) -> str:
    rows = parse_index_rows(index_text)
    counts = Counter(status for _, status in rows)
    open_rows = [finding_id for finding_id, status in rows if status in OPEN_STATUSES]
    if open_rows:
        raise ValueError(
            "final index still has open findings: "
            + ", ".join(f"SR-AUD-{finding_id}" for finding_id in open_rows)
        )
    marker_values = [f"{status}={counts.get(status, 0)}" for status in sorted(STATUS_ORDER)]
    marker_values.append(f"total={len(rows)}")

    lines = [
        "<!-- SPDX-License-Identifier: MIT -->",
        "<!-- Copyright (c) Robert Vokac and contributors -->",
        "",
        "# Final audit findings reconciliation against `next`",
        "",
        "*2026-08-22, ticket #2417.* Every one of the 71 findings that was still",
        "`confirmed` or `confirmed (design-complete)` after commit `115ce1b0` was",
        "checked individually against the current implementation, tests, public contract,",
        "and discoverable ticket/commit history. The result below is generated from",
        "`audit/final_dispositions.json` and checked against the authoritative index.",
        "",
        "<!-- audit-status-counts: " + "; ".join(marker_values) + " -->",
        "",
        "## Final distribution",
        "",
        "| Disposition | Count |",
        "| --- | ---: |",
    ]
    for status in STATUS_ORDER:
        lines.append(f"| `{status}` | {counts.get(status, 0)} |")
    lines.extend(
        [
            f"| **total** | **{len(rows)}** |",
            "",
            "`remediated` means the reported contradiction is fixed and covered.",
            "`accepted-deviation` means the remaining behavior is an explicit, tested limit of",
            "the practical C++ subset rather than forgotten implementation work.",
            "`false-positive` means the audit premise was factually wrong. No finding is hidden",
            "behind a closed status: the historical per-file reports remain intact as audit-time",
            "evidence, while this document and the index describe current HEAD.",
            "",
            "## Individually reviewed findings",
            "",
            "| Finding | Final status | Current-HEAD evidence |",
            "| --- | --- | --- |",
        ]
    )
    for item in dispositions:
        if item["group"] != "previously-open":
            continue
        lines.append(
            f"| SR-AUD-{item['id']} | `{item['status']}` | {item['evidence']} |"
        )

    special = next(item for item in dispositions if item["group"] == "special-consistency")
    correction = next(
        item for item in dispositions if item["group"] == "metadata-correction"
    )
    stale_summaries = [
        item for item in dispositions if item["group"] == "stale-summary-correction"
    ]
    lines.extend(
        [
            "",
            "## SR-AUD-071 / 071b",
            "",
            special["evidence"],
            "",
            "The prior row was internally inconsistent: it was marked `remediated` while saying",
            "071b remained open. Its final status is therefore `accepted-deviation`, not",
            "`remediated`; the owner getter repair remains credited, and the retained-view rule",
            "is now stated consistently in IMemoryOwner, MemoryPool, its migration note, and tests.",
            "",
            "## Metadata correction",
            "",
            f"**SR-AUD-{correction['id']}:** {correction['evidence']}",
            "",
            "## Other stale index summaries corrected",
            "",
        ]
    )
    for item in stale_summaries:
        lines.append(f"- **SR-AUD-{item['id']}:** {item['evidence']}")
    lines.extend(
        [
            "",
            "## Repository policy and external work",
            "",
            "The close-out sanitizer pass after the index reconciliation found two additional in-scope UB",
            "classes before ticket #2417 was closed. Half and BFloat16 integral conversions now avoid native",
            "out-of-range floating-to-integral casts while matching the current .NET runtime's unchecked",
            "lowering, and ClientWebSocket no longer passes null `vector::data()` pointers to zero-byte",
            "`memcpy`. Permanent regressions cover both. The same pass corrected a member-initialization-order",
            "bug in a diagnostics test helper, an owned-enumerator leak in a runtime test, and two socket tests",
            "whose immediate `Task::IsCompleted` assertion raced terminal-state publication even though the",
            "raw-`this` lifetime boundary had already been crossed; the Socket production boundary itself did",
            "not need widening.",
            "",
            "Final verification is reproducible from the repository gates: a cache-disabled two-job build was",
            "warning-free; all **17,781 tests across 38 executables** passed with no failure or skip; all ten",
            "selective components passed; and module, catalogue, audit, planning, inventory, Unicode, temporary",
            "path, seam, and negative-fixture checks passed. ASan+UBSan+LSan covered **12,793 relevant tests",
            "across 22 executables**, strict `float-cast-overflow` UBSan covered all **6,156 Core.Base tests**,",
            "and three repeated targeted TSan groups passed **57/57** without a report. The audit distribution",
            "remains 343/19/2 because these late defects were fixed before closure rather than hidden in a new",
            "open status.",
            "",
            "The checked Doxygen ceiling remains 2,675 warnings and runs in both GitHub CI and",
            "`scripts/local_ci_check.sh`; this reconciliation does not re-baseline historical",
            "warnings upward. Audit/index consistency is now a local gate as well.",
            "",
            "The planning tickets #1773, #1962, and #2381 are external/environment blockers,",
            "not unresolved audit findings. They remain blocked in `plan.sqlite3` and are not",
            "reclassified by this source-tree reconciliation.",
            "",
        ]
    )
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--write",
        action="store_true",
        help="update the index and generated reconciliation document",
    )
    args = parser.parse_args(argv)

    try:
        dispositions = load_dispositions()
        current_index = INDEX_PATH.read_text(encoding="utf-8")
        expected_index = rewrite_index(current_index, dispositions)
        expected_document = render_document(dispositions, expected_index)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1

    if args.write:
        INDEX_PATH.write_text(expected_index, encoding="utf-8")
        DOCUMENT_PATH.write_text(expected_document, encoding="utf-8")
        print("OK: wrote final audit dispositions and reconciliation")
        return 0

    problems: list[str] = []
    if current_index != expected_index:
        problems.append(
            "audit index differs from audit/final_dispositions.json; run "
            "scripts/reconcile_audit_findings.py --write"
        )
    try:
        current_document = DOCUMENT_PATH.read_text(encoding="utf-8")
    except OSError as error:
        problems.append(str(error))
    else:
        if current_document != expected_document:
            problems.append(
                "reconciliation document is stale; run "
                "scripts/reconcile_audit_findings.py --write"
            )
    if problems:
        for problem in problems:
            print(f"FAIL: {problem}", file=sys.stderr)
        return 1
    print("OK: final audit dispositions, index, and reconciliation agree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
