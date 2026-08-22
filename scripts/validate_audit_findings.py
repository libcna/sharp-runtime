#!/usr/bin/env python3
"""Validate the authoritative audit index and its published status counts.

The per-file ``*.audit.md`` reports preserve the evidence seen at audit time.
``audit/AUDIT_FINDINGS_INDEX.md`` is the current disposition ledger, while
``docs/AuditFindingsReconciliation.md`` publishes its aggregate counts.  This
check keeps those three layers mechanically connected.
"""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sqlite3
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INDEX = REPO_ROOT / "audit" / "AUDIT_FINDINGS_INDEX.md"
DEFAULT_RECONCILIATION = REPO_ROOT / "docs" / "AuditFindingsReconciliation.md"
DEFAULT_DISPOSITIONS = REPO_ROOT / "audit" / "final_dispositions.json"
DEFAULT_PLAN_DATABASE = REPO_ROOT / "plan.sqlite3"

VALID_SEVERITIES = {"low", "medium", "high", "critical"}
VALID_STATUSES = {
    "confirmed",
    "confirmed (design-complete)",
    "remediated",
    "accepted-deviation",
    "false-positive",
    "external-blocked",
}

ROW_RE = re.compile(
    r"^\| \[(SR-AUD-(\d{3}))\]\(([^)]+)\) \| ([^|]+) \| ([^|]+) \| (.*?) \| (.*) \|$"
)
COUNTS_RE = re.compile(r"<!-- audit-status-counts: (.*?) -->")
TICKET_REFERENCE_RE = re.compile(r"(?<!\d)#(\d{4})(?!\d)")


@dataclass(frozen=True)
class Finding:
    identifier: str
    number: int
    target: str
    severity: str
    status: str
    source: str
    summary: str


def parse_index(path: Path) -> tuple[list[Finding], list[str]]:
    findings: list[Finding] = []
    problems: list[str] = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw_line.startswith("| [SR-AUD-"):
            continue
        match = ROW_RE.match(raw_line)
        if match is None:
            problems.append(f"{path}:{line_number}: malformed audit index row")
            continue
        identifier, number, target, severity, status, source, summary = match.groups()
        finding = Finding(
            identifier=identifier,
            number=int(number),
            target=target,
            severity=severity.strip(),
            status=status.strip(),
            source=source.strip(),
            summary=summary.strip(),
        )
        findings.append(finding)
        if finding.severity not in VALID_SEVERITIES:
            problems.append(
                f"{path}:{line_number}: {identifier} has invalid severity {finding.severity!r}"
            )
        if finding.status not in VALID_STATUSES:
            problems.append(
                f"{path}:{line_number}: {identifier} has invalid status {finding.status!r}"
            )
        if not finding.source or not finding.summary:
            problems.append(f"{path}:{line_number}: {identifier} has a blank source or summary")

    numbers = [finding.number for finding in findings]
    duplicates = sorted(number for number, count in Counter(numbers).items() if count > 1)
    if duplicates:
        problems.append(f"duplicate finding number(s): {duplicates}")
    if numbers:
        expected = list(range(1, max(numbers) + 1))
        if sorted(numbers) != expected:
            missing = sorted(set(expected) - set(numbers))
            problems.append(f"finding IDs are not contiguous from 001: missing {missing}")
    else:
        problems.append("audit index contains no finding rows")

    for finding in findings:
        report_part = finding.target.split("#", 1)[0]
        report_path = (path.parent / report_part).resolve()
        if not report_path.is_file():
            problems.append(f"{finding.identifier}: linked report does not exist: {report_path}")
            continue
        report_text = report_path.read_text(encoding="utf-8", errors="replace")
        if finding.identifier not in report_text:
            problems.append(
                f"{finding.identifier}: linked report does not contain its identifier: {report_path}"
            )

    return findings, problems


def status_counts(findings: list[Finding]) -> Counter[str]:
    counts = Counter(finding.status for finding in findings)
    counts["total"] = len(findings)
    return counts


def parse_published_counts(path: Path) -> tuple[dict[str, int], list[str]]:
    text = path.read_text(encoding="utf-8")
    matches = COUNTS_RE.findall(text)
    if len(matches) != 1:
        return {}, [
            f"{path}: expected exactly one '<!-- audit-status-counts: ... -->' marker, found {len(matches)}"
        ]

    parsed: dict[str, int] = {}
    problems: list[str] = []
    for field in matches[0].split(";"):
        field = field.strip()
        if not field:
            continue
        key, separator, value = field.partition("=")
        if not separator or not key.strip() or not value.strip().isdigit():
            problems.append(f"{path}: malformed audit count field {field!r}")
            continue
        parsed[key.strip()] = int(value.strip())
    return parsed, problems


def validate_disposition_ticket_references(
    dispositions_path: Path, database_path: Path
) -> tuple[set[int], list[str]]:
    """Ensure every four-digit ticket cited by disposition evidence exists in the plan DB.

    Audit evidence also contains specification references such as ``UAX #15``.  Restricting the
    matcher to the repository's four-digit ticket spelling keeps those external section numbers
    out of the planning namespace while still checking every ``#NNNN`` claim.
    """
    problems: list[str] = []
    try:
        data = json.loads(dispositions_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return set(), [f"{dispositions_path}: cannot read disposition data: {error}"]

    findings = data.get("findings")
    if not isinstance(findings, list):
        return set(), [f"{dispositions_path}: 'findings' must be a list"]

    references: dict[int, set[str]] = {}
    for position, finding in enumerate(findings):
        if not isinstance(finding, dict):
            problems.append(
                f"{dispositions_path}: findings[{position}] must be an object"
            )
            continue
        identifier = str(finding.get("id", f"index {position}"))
        evidence = finding.get("evidence")
        if not isinstance(evidence, str):
            problems.append(
                f"{dispositions_path}: SR-AUD-{identifier} evidence must be a string"
            )
            continue
        for ticket_text in TICKET_REFERENCE_RE.findall(evidence):
            references.setdefault(int(ticket_text), set()).add(identifier)

    try:
        connection = sqlite3.connect(
            f"{database_path.resolve().as_uri()}?mode=ro", uri=True
        )
        try:
            known_tickets = {
                int(row[0]) for row in connection.execute("SELECT ticket_no FROM ticket")
            }
        finally:
            connection.close()
    except sqlite3.Error as error:
        problems.append(f"{database_path}: cannot read planning tickets: {error}")
        return set(references), problems

    for ticket in sorted(set(references) - known_tickets):
        finding_names = ", ".join(
            f"SR-AUD-{identifier}" for identifier in sorted(references[ticket])
        )
        problems.append(
            f"{dispositions_path}: {finding_names} evidence references missing planning "
            f"ticket #{ticket}"
        )
    return set(references), problems


def validate(index: Path, reconciliation: Path) -> tuple[Counter[str], list[str]]:
    findings, problems = parse_index(index)
    counts = status_counts(findings)
    published, published_problems = parse_published_counts(reconciliation)
    problems.extend(published_problems)

    expected_keys = set(VALID_STATUSES) | {"total"}
    if published:
        unknown = sorted(set(published) - expected_keys)
        missing = sorted(expected_keys - set(published))
        if unknown:
            problems.append(f"{reconciliation}: unknown published count key(s): {unknown}")
        if missing:
            problems.append(f"{reconciliation}: missing published count key(s): {missing}")
        for key in sorted(expected_keys & set(published)):
            actual = counts.get(key, 0)
            if published[key] != actual:
                problems.append(
                    f"{reconciliation}: published {key}={published[key]}, index has {actual}"
                )

    return counts, problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", type=Path, default=DEFAULT_INDEX)
    parser.add_argument("--reconciliation", type=Path, default=DEFAULT_RECONCILIATION)
    parser.add_argument("--dispositions", type=Path, default=DEFAULT_DISPOSITIONS)
    parser.add_argument("--plan-database", type=Path, default=DEFAULT_PLAN_DATABASE)
    args = parser.parse_args()

    counts, problems = validate(args.index.resolve(), args.reconciliation.resolve())
    ticket_references, ticket_problems = validate_disposition_ticket_references(
        args.dispositions.resolve(), args.plan_database.resolve()
    )
    problems.extend(ticket_problems)
    ordered = sorted((status, count) for status, count in counts.items() if status != "total")
    print("Audit status counts: " + ", ".join(f"{status}={count}" for status, count in ordered))
    print(f"Audit findings total: {counts.get('total', 0)}")
    print(f"Disposition planning-ticket references: {len(ticket_references)}")
    if problems:
        print(f"FAIL: {len(problems)} audit consistency problem(s):", file=sys.stderr)
        for problem in problems:
            print(f"  - {problem}", file=sys.stderr)
        return 1
    print("OK: audit index, reports, dispositions, planning tickets, and counts agree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
