#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Negative and positive fixtures for check_version_seam_odr.py.

Every case builds a miniature repository on disk -- a production header that
declares a seam, plus test sources that define it -- and asserts what the
checker says about it. The first negative case is the exact shape ticket #1800
found in the real tree: two suites, two bodies, one program.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import textwrap
import unittest


REPO_ROOT = Path(__file__).resolve().parent.parent
CHECKER_PATH = REPO_ROOT / "scripts/check_version_seam_odr.py"
SPEC = importlib.util.spec_from_file_location("version_seam_checker", CHECKER_PATH)
assert SPEC is not None and SPEC.loader is not None
CHECKER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CHECKER
SPEC.loader.exec_module(CHECKER)


SEAM_DECLARATION = """\
#pragma once
namespace SharpRuntime::Testing
{
    template <typename TOwner>
    struct CollectionVersionAccess;
}
namespace System::Collections
{
    class Widget
    {
        int version_ = 0;
        template <typename> friend struct SharpRuntime::Testing::CollectionVersionAccess;
    };
}
"""

RICH_BODY = """\
namespace SharpRuntime::Testing {
template <>
struct CollectionVersionAccess<System::Collections::Widget> {
    static int version(const System::Collections::Widget& w) { return w.version_; }
    static void positionVersion(System::Collections::Widget& w, int v) { w.version_ = v; }
};
}
"""

LEAN_BODY = """\
namespace SharpRuntime::Testing {
template <>
struct CollectionVersionAccess<System::Collections::Widget> {
    static int version(const System::Collections::Widget& w) { return w.version_; }
};
}
"""


class FixtureRepository:
    def __init__(self, root: Path) -> None:
        self.root = root

    def write(self, relative: str, content: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(content), encoding="utf-8")

    def declare_seam(self, content: str = SEAM_DECLARATION) -> None:
        self.write("modules/collections/include/System/Collections/Widget.hpp", content)


class VersionSeamCheckerTests(unittest.TestCase):
    def setUp(self) -> None:
        self._directory = tempfile.TemporaryDirectory()
        self.addCleanup(self._directory.cleanup)
        self.fixture = FixtureRepository(Path(self._directory.name))

    def check(self):
        return CHECKER.check_repository(self.fixture.root)

    # -- positive cases ---------------------------------------------------

    def test_one_defining_header_included_by_two_suites_is_accepted(self) -> None:
        self.fixture.declare_seam()
        self.fixture.write(
            "modules/collections/tests/support/Seam.hpp",
            "#pragma once\n" + RICH_BODY,
        )
        for name in ("AlphaTests.cpp", "BetaTests.cpp"):
            self.fixture.write(
                f"modules/collections/tests/System/Collections/{name}",
                '#include "../../support/Seam.hpp"\nint alpha() { return 0; }\n',
            )
        report = self.check()
        self.assertTrue(report.ok, report.problems)
        self.assertEqual(len(report.definitions), 1)
        self.assertIn("CollectionVersionAccess", report.seams)

    def test_a_use_or_a_friend_declaration_is_not_a_definition(self) -> None:
        self.fixture.declare_seam()
        self.fixture.write(
            "modules/collections/tests/support/Seam.hpp",
            "#pragma once\n" + RICH_BODY,
        )
        self.fixture.write(
            "modules/collections/tests/System/Collections/UseTests.cpp",
            """\
            #include "../../support/Seam.hpp"
            int read(const System::Collections::Widget& w) {
                return SharpRuntime::Testing::CollectionVersionAccess<
                    System::Collections::Widget>::version(w);
            }
            """,
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)
        self.assertEqual(len(report.definitions), 1)

    def test_a_commented_out_second_body_is_ignored(self) -> None:
        self.fixture.declare_seam()
        self.fixture.write(
            "modules/collections/tests/support/Seam.hpp",
            "#pragma once\n" + RICH_BODY,
        )
        self.fixture.write(
            "modules/collections/tests/System/Collections/CommentTests.cpp",
            '#include "../../support/Seam.hpp"\n/*\n' + LEAN_BODY + "*/\n",
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)

    # -- negative cases ---------------------------------------------------

    def test_two_suites_with_different_bodies_are_rejected(self) -> None:
        """The exact shape ticket #1800 found: SR1787 body against SR1794 body."""
        self.fixture.declare_seam()
        self.fixture.write(
            "modules/collections/tests/System/Collections/AlphaTests.cpp", RICH_BODY
        )
        self.fixture.write(
            "modules/collections/tests/System/Collections/BetaTests.cpp", LEAN_BODY
        )
        report = self.check()
        self.assertFalse(report.ok)
        self.assertTrue(
            any("2 different token bodies" in problem for problem in report.problems),
            report.problems,
        )
        self.assertTrue(
            any("is defined in 2 files" in problem for problem in report.problems),
            report.problems,
        )

    def test_two_suites_with_identical_bodies_are_still_rejected(self) -> None:
        """Identical copies are legal C++ but are one edit away from not being."""
        self.fixture.declare_seam()
        for name in ("AlphaTests.cpp", "BetaTests.cpp"):
            self.fixture.write(
                f"modules/collections/tests/System/Collections/{name}", RICH_BODY
            )
        report = self.check()
        self.assertFalse(report.ok)
        self.assertTrue(
            any("is defined in 2 files" in problem for problem in report.problems),
            report.problems,
        )
        self.assertFalse(
            any("different token bodies" in problem for problem in report.problems),
            report.problems,
        )

    def test_a_definition_in_a_production_header_is_rejected(self) -> None:
        self.fixture.declare_seam(SEAM_DECLARATION + RICH_BODY)
        report = self.check()
        self.assertFalse(report.ok)
        self.assertTrue(
            any("in a production tree" in problem for problem in report.problems),
            report.problems,
        )

    def test_a_definition_in_a_production_source_is_rejected(self) -> None:
        self.fixture.declare_seam()
        self.fixture.write(
            "modules/collections/src/System/Collections/Widget.cpp",
            '#include "System/Collections/Widget.hpp"\n' + RICH_BODY,
        )
        report = self.check()
        self.assertFalse(report.ok)
        self.assertTrue(
            any("in a production tree" in problem for problem in report.problems),
            report.problems,
        )

    def test_two_seam_macros_in_one_file_are_rejected(self) -> None:
        self.fixture.declare_seam(
            SEAM_DECLARATION.replace(
                "class Widget",
                "class Gadget { int version_ = 0; "
                "template <typename> friend struct "
                "SharpRuntime::Testing::CollectionVersionAccess; };\n    class Widget",
            )
        )
        self.fixture.write(
            "modules/collections/tests/support/Seam.hpp",
            """\
            #pragma once
            namespace SharpRuntime::Testing {
            #define RICH_SEAM_BODY(T) \\
                static int version(const T& c) { return c.version_; } \\
                static void positionVersion(T& c, int v) { c.version_ = v; }
            #define LEAN_SEAM_BODY(T) \\
                static int version(const T& c) { return c.version_; }
            template <>
            struct CollectionVersionAccess<System::Collections::Widget> {
                RICH_SEAM_BODY(System::Collections::Widget)
            };
            template <>
            struct CollectionVersionAccess<System::Collections::Gadget> {
                LEAN_SEAM_BODY(System::Collections::Gadget)
            };
            }
            """,
        )
        report = self.check()
        self.assertFalse(report.ok)
        self.assertTrue(
            any("different macros" in problem for problem in report.problems),
            report.problems,
        )

    def test_a_repository_with_no_declared_seam_is_reported(self) -> None:
        self.fixture.write(
            "modules/collections/include/System/Collections/Widget.hpp",
            "#pragma once\nnamespace System::Collections { class Widget {}; }\n",
        )
        report = self.check()
        self.assertFalse(report.ok)
        self.assertTrue(
            any("no test-only access seam" in problem for problem in report.problems),
            report.problems,
        )

    def test_build_and_vendor_trees_are_not_scanned(self) -> None:
        self.fixture.declare_seam()
        self.fixture.write(
            "modules/collections/tests/support/Seam.hpp",
            "#pragma once\n" + RICH_BODY,
        )
        self.fixture.write("build-probe/1800_odr_a.cpp", LEAN_BODY)
        self.fixture.write("vendor/googletest/fake.cpp", LEAN_BODY)
        report = self.check()
        self.assertTrue(report.ok, report.problems)


class TokenizerTests(unittest.TestCase):
    def test_nested_template_close_is_two_tokens(self) -> None:
        tokens = CHECKER.tokenize("A<B<C>>")
        self.assertEqual(tokens, ["A", "<", "B", "<", "C", ">", ">"])

    def test_comments_and_literals_are_neutralised(self) -> None:
        stripped = CHECKER.strip_comments_and_literals(
            'int a; // note\nconst char* s = "x"; /* block */ int b;'
        )
        self.assertNotIn("note", stripped)
        self.assertNotIn("block", stripped)
        self.assertNotIn('"x"', stripped)
        self.assertIn("int b", stripped)


if __name__ == "__main__":
    unittest.main()
