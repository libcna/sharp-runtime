#!/usr/bin/env python3
"""Focused tests for scripts/validate_audit_findings.py."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "validate_audit_findings.py"
SPEC = importlib.util.spec_from_file_location("validate_audit_findings", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class AuditFindingValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "audit" / "reports").mkdir(parents=True)
        (self.root / "docs").mkdir()
        (self.root / "audit" / "reports" / "one.audit.md").write_text(
            "## SR-AUD-001 — medium — example\n", encoding="utf-8"
        )
        self.index = self.root / "audit" / "AUDIT_FINDINGS_INDEX.md"
        self.reconciliation = self.root / "docs" / "AuditFindingsReconciliation.md"

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write_index(self, *, status: str = "remediated", duplicate: bool = False) -> None:
        row = (
            "| [SR-AUD-001](reports/one.audit.md#sr-aud-001) | medium | "
            f"{status} | `one.cpp` | Evidence-backed disposition. |\n"
        )
        self.index.write_text(row + (row if duplicate else ""), encoding="utf-8")

    def write_counts(self, *, status: str = "remediated", count: int = 1) -> None:
        values = {status: 0 for status in MODULE.VALID_STATUSES}
        values[status] = count
        values["total"] = count
        marker = "; ".join(f"{key}={values[key]}" for key in sorted(values))
        self.reconciliation.write_text(
            f"<!-- audit-status-counts: {marker} -->\n", encoding="utf-8"
        )

    def test_valid_index_and_published_counts_pass(self) -> None:
        self.write_index()
        self.write_counts()
        counts, problems = MODULE.validate(self.index, self.reconciliation)
        self.assertEqual(counts["remediated"], 1)
        self.assertEqual(problems, [])

    def test_each_terminal_disposition_is_part_of_the_checked_vocabulary(self) -> None:
        for status in ("remediated", "accepted-deviation", "false-positive"):
            with self.subTest(status=status):
                self.write_index(status=status)
                self.write_counts(status=status)
                counts, problems = MODULE.validate(self.index, self.reconciliation)
                self.assertEqual(counts[status], 1)
                self.assertEqual(problems, [])

    def test_unknown_status_is_rejected(self) -> None:
        self.write_index(status="closed-ish")
        self.write_counts(count=0)
        _, problems = MODULE.validate(self.index, self.reconciliation)
        self.assertTrue(any("invalid status" in problem for problem in problems))

    def test_duplicate_id_is_rejected(self) -> None:
        self.write_index(duplicate=True)
        self.write_counts(count=2)
        _, problems = MODULE.validate(self.index, self.reconciliation)
        self.assertTrue(any("duplicate finding" in problem for problem in problems))

    def test_stale_published_count_is_rejected(self) -> None:
        self.write_index()
        self.write_counts(count=0)
        _, problems = MODULE.validate(self.index, self.reconciliation)
        self.assertTrue(any("published remediated=0" in problem for problem in problems))

    def test_missing_linked_report_is_rejected(self) -> None:
        self.write_index()
        (self.root / "audit" / "reports" / "one.audit.md").unlink()
        self.write_counts()
        _, problems = MODULE.validate(self.index, self.reconciliation)
        self.assertTrue(any("linked report does not exist" in problem for problem in problems))

    def write_dispositions(self, evidence: str) -> Path:
        path = self.root / "audit" / "final_dispositions.json"
        path.write_text(
            json.dumps({"findings": [{"id": "001", "evidence": evidence}]}),
            encoding="utf-8",
        )
        return path

    def write_plan_database(self, *ticket_numbers: int) -> Path:
        path = self.root / "plan.sqlite3"
        connection = sqlite3.connect(path)
        try:
            connection.execute("CREATE TABLE ticket (ticket_no INTEGER NOT NULL UNIQUE)")
            connection.executemany(
                "INSERT INTO ticket(ticket_no) VALUES (?)",
                ((ticket_number,) for ticket_number in ticket_numbers),
            )
            connection.commit()
        finally:
            connection.close()
        return path

    def test_disposition_ticket_references_must_exist_in_the_plan_database(self) -> None:
        dispositions = self.write_dispositions(
            "Tickets #1234 and #5678 provide the implementation evidence."
        )
        database = self.write_plan_database(1234)

        references, problems = MODULE.validate_disposition_ticket_references(
            dispositions, database
        )

        self.assertEqual(references, {1234, 5678})
        self.assertTrue(any("SR-AUD-001" in problem and "#5678" in problem for problem in problems))
        self.assertFalse(any("#1234" in problem for problem in problems))

    def test_non_ticket_hash_numbers_are_not_treated_as_plan_references(self) -> None:
        dispositions = self.write_dispositions(
            "The accepted subset follows UAX #15; ticket #1234 records the decision."
        )
        database = self.write_plan_database(1234)

        references, problems = MODULE.validate_disposition_ticket_references(
            dispositions, database
        )

        self.assertEqual(references, {1234})
        self.assertEqual(problems, [])


if __name__ == "__main__":
    unittest.main()
