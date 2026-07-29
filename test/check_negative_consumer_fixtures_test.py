#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Fixtures for check_negative_consumer_fixtures.py.

Every case builds a miniature repository on disk -- a CMake module registration,
a public header, and one or more ``test/consumer/*_negative.cpp`` files -- and
asserts what the checker says about it. The compiles are real; the sources are
two or three lines each, so the whole suite costs a few seconds.

The temporary repositories are written under the repository-local
``build-tmp/negative-fixture-selftest/``, never under ``/tmp``: CLAUDE.md's
build-resource policy forbids a build tree outside the repository, and a
deterministic location is also what makes a failed run inspectable.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import sys
import tempfile
import textwrap
import unittest


REPO_ROOT = Path(__file__).resolve().parent.parent
CHECKER_PATH = REPO_ROOT / "scripts/check_negative_consumer_fixtures.py"
TEMPORARY_ROOT = REPO_ROOT / "build-tmp" / "negative-fixture-selftest"

SPEC = importlib.util.spec_from_file_location("negative_fixture_checker", CHECKER_PATH)
assert SPEC is not None and SPEC.loader is not None
CHECKER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = CHECKER
SPEC.loader.exec_module(CHECKER)


MODULE_LIST = """\
include_guard(GLOBAL)
set(sharp_runtime_module_directories
    widget
    gadget
)
"""

WIDGET_MODULE = """\
sharp_runtime_register_module(
    NAME Widget
    TARGET sharp_runtime_widget
    TYPE INTERFACE
    PUBLIC_DEPENDENCIES Gadget
)
"""

GADGET_MODULE = """\
sharp_runtime_register_module(
    NAME Gadget
    TARGET sharp_runtime_gadget
    TYPE INTERFACE
)
"""

WIDGET_HEADER = """\
#pragma once
namespace Widget {
struct Box {
    int value = 0;
};
}
"""

GADGET_HEADER = """\
#pragma once
namespace Gadget {
inline int identity(int value) { return value; }
}
"""

PRELUDE = """\
#ifndef SHARP_RUNTIME_NEGATIVE_SITE
#define SHARP_RUNTIME_NEGATIVE_SITE 0
#endif
"""


class FixtureRepository:
    """A miniature sharp-runtime tree that the checker can be pointed at."""

    def __init__(self, root: Path) -> None:
        self.root = root
        self.write("cmake/SharpRuntimeModules.cmake", MODULE_LIST)
        self.write("modules/widget/CMakeLists.txt", WIDGET_MODULE)
        self.write("modules/gadget/CMakeLists.txt", GADGET_MODULE)
        self.write("modules/widget/include/Widget.hpp", WIDGET_HEADER)
        self.write("modules/gadget/include/Gadget.hpp", GADGET_HEADER)
        (self.root / "test/consumer").mkdir(parents=True, exist_ok=True)

    def write(self, relative: str, content: str) -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(textwrap.dedent(content), encoding="utf-8")
        return path

    def fixture(self, name: str, body: str, *, directive: str | None = "component=Widget") -> Path:
        header = f"// NEGATIVE-FIXTURE: {directive}\n" if directive is not None else ""
        return self.write(
            f"test/consumer/{name}_negative.cpp",
            header + '#include "Widget.hpp"\n' + PRELUDE + textwrap.dedent(body),
        )


class CheckerTestCase(unittest.TestCase):
    def setUp(self) -> None:
        TEMPORARY_ROOT.mkdir(parents=True, exist_ok=True)
        directory = Path(tempfile.mkdtemp(dir=TEMPORARY_ROOT, prefix="case "))
        self.addCleanup(shutil.rmtree, directory, True)
        self.fixture = FixtureRepository(directory)

    def check(self, **keywords):
        return CHECKER.check_repository(self.fixture.root, **keywords)

    def assertProblem(self, report, needle: str) -> None:
        self.assertFalse(report.ok, "expected a failure")
        self.assertTrue(
            any(needle in problem for problem in report.problems),
            f"no problem contained {needle!r}: {report.problems}",
        )


class SinglePassingSiteTests(CheckerTestCase):
    def test_one_correctly_failing_site_is_accepted(self) -> None:
        self.fixture.fixture(
            "one_site",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(bad-conversion): invalid conversion from 'const char*'
                Widget::Box box;
                box.value = "text";
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)
        self.assertEqual(report.site_count, 1)
        self.assertEqual(report.invocations, 2)

    def test_three_sites_fail_independently(self) -> None:
        self.fixture.fixture(
            "three_sites",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(alpha): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
            #if SHARP_RUNTIME_NEGATIVE_SITE == 2
                // NEGATIVE(beta): was not declared in this scope
                (void)undeclaredBeta;
            #endif
            #if SHARP_RUNTIME_NEGATIVE_SITE == 3
                // NEGATIVE(gamma): cannot bind non-const lvalue reference
                int& gamma = 5;
                (void)gamma;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)
        self.assertEqual(report.site_count, 3)
        self.assertEqual(report.invocations, 4)
        self.assertEqual(
            [result.site.identifier for result in report.results if result.site],
            ["alpha", "beta", "gamma"],
        )

    def test_a_multiline_expression_is_one_site(self) -> None:
        """GCC reports this on the statement's SECOND physical line.

        The region, not the marked line, is what the checker attributes to, so a
        statement written over four lines needs no special handling. Ticket
        #1796's line-scanning heuristic would have looked at the wrong line.
        """
        self.fixture.fixture(
            "multiline",
            """\
            int consume(int* pointer);
            int consume(int* pointer) { return *pointer; }

            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(multiline-call): cannot convert 'const char*' to 'int*'
                const int result = consume(
                    "a string literal spanning"
                    " a call written over four lines");
                (void)result;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)
        located = [
            line
            for result in report.results
            if result.site and result.outcome
            for diagnostic in CHECKER.parse_diagnostics(result.outcome.log)
            if diagnostic.kind == "error" and (line := diagnostic.line) is not None
        ]
        # The marker is on line 11 and the statement starts on line 12; the only
        # error is on line 13.
        self.assertEqual(located, [13])

    def test_a_later_fragment_alternative_is_accepted(self) -> None:
        self.fixture.fixture(
            "alternatives",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(alternatives): a wording no compiler has ever emitted
                //     | another wording no compiler emits
                //     | invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)
        matched = [result.detail for result in report.results if result.site]
        self.assertEqual(matched, ["invalid conversion from 'const char*'"])


class RejectionTests(CheckerTestCase):
    def test_a_site_that_compiles_is_rejected(self) -> None:
        self.fixture.fixture(
            "compiles",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(now-legal): invalid conversion from 'const char*'
                int alpha = 5;
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "COMPILED")

    def test_an_error_outside_the_site_region_is_rejected(self) -> None:
        """A second broken line must not be allowed to stand in for the site."""
        self.fixture.fixture(
            "masked",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(masked): invalid conversion from 'const char*'
            #define SHARP_RUNTIME_SELFTEST_BREAK_LATER 1
                int alpha = "text";
                (void)alpha;
            #endif
            #ifdef SHARP_RUNTIME_SELFTEST_BREAK_LATER
                int unrelated = "also text";
                (void)unrelated;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "diagnostics outside the site region")

    def test_a_broken_baseline_is_rejected(self) -> None:
        self.fixture.fixture(
            "broken_baseline",
            """\
            int main() {
                int unrelated = "a syntax error outside every guard";
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                (void)unrelated;
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "does not compile with every site disabled")

    def test_a_baseline_warning_is_rejected(self) -> None:
        self.fixture.fixture(
            "warning_baseline",
            """\
            int main() {
                int unused = 1;
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        # -Werror turns the unused variable into an error, so the baseline is
        # reported as not compiling; either message is a refusal to proceed.
        self.assertFalse(report.ok)
        self.assertTrue(
            any("[baseline]" in problem for problem in report.problems),
            report.problems,
        )

    def test_a_stale_expected_diagnostic_is_rejected(self) -> None:
        self.fixture.fixture(
            "stale_fragment",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(stale): a diagnostic this compiler never emits
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "none of the expected diagnostics matched")

    def test_a_site_without_a_marker_is_rejected(self) -> None:
        self.fixture.fixture(
            "no_marker",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "has no '// NEGATIVE(id): expected diagnostic' marker")

    def test_a_duplicate_marker_id_is_rejected(self) -> None:
        self.fixture.fixture(
            "duplicate_id",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(same-name): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
            #if SHARP_RUNTIME_NEGATIVE_SITE == 2
                // NEGATIVE(same-name): was not declared in this scope
                (void)undeclared;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "duplicate marker id 'same-name'")

    def test_a_stale_marker_outside_every_region_is_rejected(self) -> None:
        self.fixture.fixture(
            "stale_marker",
            """\
            // NEGATIVE(orphan): a marker whose guard was deleted
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(kept): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "stale NEGATIVE marker outside every")

    def test_a_gap_in_the_site_numbering_is_rejected(self) -> None:
        self.fixture.fixture(
            "numbering_gap",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(first): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
            #if SHARP_RUNTIME_NEGATIVE_SITE == 3
                // NEGATIVE(third): was not declared in this scope
                (void)undeclared;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertProblem(report, "with no gap")

    def test_a_fixture_with_no_site_is_rejected(self) -> None:
        self.fixture.fixture("no_site", "int main() { return 0; }\n")
        report = self.check()
        self.assertProblem(report, "asserts nothing")

    def test_a_fixture_without_a_directive_is_rejected(self) -> None:
        self.fixture.write(
            "test/consumer/undeclared_negative.cpp",
            "int main() { return 0; }\n",
        )
        report = self.check()
        self.assertProblem(report, "no 'NEGATIVE-FIXTURE:' directive")

    def test_a_fixture_without_the_prelude_is_rejected(self) -> None:
        self.fixture.write(
            "test/consumer/no_prelude_negative.cpp",
            "// NEGATIVE-FIXTURE: component=Widget\n"
            "int main() {\n"
            "#if SHARP_RUNTIME_NEGATIVE_SITE == 1\n"
            "    // NEGATIVE(site): invalid conversion from 'const char*'\n"
            '    int alpha = "text";\n'
            "    (void)alpha;\n"
            "#endif\n"
            "    return 0;\n"
            "}\n",
        )
        report = self.check()
        self.assertProblem(report, "prelude")

    def test_an_unknown_component_is_rejected(self) -> None:
        self.fixture.fixture(
            "unknown_component",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
            directive="component=NoSuchComponent",
        )
        report = self.check()
        self.assertProblem(report, "is not registered in the repository's CMake")

    def test_an_empty_fixture_directory_is_rejected(self) -> None:
        report = self.check()
        self.assertProblem(report, "must never pass vacuously")

    def test_a_filter_matching_nothing_is_rejected(self) -> None:
        self.fixture.fixture(
            "present",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check(patterns=["*no_such_fixture*"])
        self.assertProblem(report, "must never pass vacuously")


class ConfigurationTests(CheckerTestCase):
    def test_a_transitive_public_include_directory_is_supplied(self) -> None:
        """Widget publicly depends on Gadget, so Gadget.hpp must resolve."""
        self.fixture.write(
            "test/consumer/include_path_negative.cpp",
            "// NEGATIVE-FIXTURE: component=Widget\n"
            '#include "Widget.hpp"\n'
            '#include "Gadget.hpp"\n' + PRELUDE + "int main() {\n"
            "#if SHARP_RUNTIME_NEGATIVE_SITE == 1\n"
            "    // NEGATIVE(site): invalid conversion from 'const char*'\n"
            '    const int value = Gadget::identity("text");\n'
            "    (void)value;\n"
            "#endif\n"
            "    return 0;\n"
            "}\n",
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)

    def test_a_fixture_compile_definition_is_supplied(self) -> None:
        self.fixture.fixture(
            "with_definition",
            """\
            #ifndef SHARP_RUNTIME_SELFTEST_STRICT
            #error "the fixture's define= was not passed to the compiler"
            #endif
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
            directive="component=Widget define=SHARP_RUNTIME_SELFTEST_STRICT=1",
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)

    def test_a_path_containing_a_space_is_handled(self) -> None:
        """setUp() already puts every case under a 'case <random>' directory."""
        self.assertIn(" ", str(self.fixture.root))
        self.fixture.fixture(
            "spaced_path",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check()
        self.assertTrue(report.ok, report.problems)

    def test_a_missing_compiler_is_an_environment_failure(self) -> None:
        self.fixture.fixture(
            "missing_compiler",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        with self.assertRaises(CHECKER.EnvironmentProblem):
            self.check(compiler="sharp-runtime-no-such-compiler")

    def test_a_timeout_is_reported_rather_than_hanging(self) -> None:
        self.fixture.fixture(
            "timeout",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(site): invalid conversion from 'const char*'
                int alpha = "text";
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )
        report = self.check(timeout=0.001)
        self.assertProblem(report, "did not finish within the timeout")


class ParallelismTests(CheckerTestCase):
    def _four_sites(self) -> None:
        sites = "".join(
            f"#if SHARP_RUNTIME_NEGATIVE_SITE == {number}\n"
            f"    // NEGATIVE(site-{number}): invalid conversion from 'const char*'\n"
            f'    int value{number} = "text";\n'
            f"    (void)value{number};\n"
            "#endif\n"
            for number in range(1, 5)
        )
        self.fixture.write(
            "test/consumer/parallel_negative.cpp",
            "// NEGATIVE-FIXTURE: component=Widget\n"
            '#include "Widget.hpp"\n' + PRELUDE + "int main() {\n" + sites + "    return 0;\n}\n",
        )

    def test_peak_concurrency_never_exceeds_the_ceiling(self) -> None:
        self._four_sites()
        report = self.check(jobs=CHECKER.MAXIMUM_JOBS)
        self.assertTrue(report.ok, report.problems)
        self.assertEqual(report.invocations, 5)
        self.assertLessEqual(report.peak_jobs, CHECKER.MAXIMUM_JOBS)

    def test_a_job_count_above_the_ceiling_is_refused(self) -> None:
        self._four_sites()
        with self.assertRaises(CHECKER.EnvironmentProblem) as raised:
            self.check(jobs=CHECKER.MAXIMUM_JOBS + 1)
        self.assertIn("three parallel jobs", str(raised.exception))

    def test_a_single_job_is_permitted(self) -> None:
        self._four_sites()
        report = self.check(jobs=1)
        self.assertTrue(report.ok, report.problems)
        self.assertEqual(report.peak_jobs, 1)


class HygieneTests(CheckerTestCase):
    def _failing_fixture(self) -> None:
        self.fixture.fixture(
            "hygiene",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(now-legal): invalid conversion from 'const char*'
                int alpha = 5;
                (void)alpha;
            #endif
                return 0;
            }
            """,
        )

    def test_a_failing_run_leaves_no_artefact_behind(self) -> None:
        self._failing_fixture()
        before = sorted(
            path.relative_to(self.fixture.root).as_posix()
            for path in self.fixture.root.rglob("*")
        )
        report = self.check()
        self.assertFalse(report.ok)
        after = sorted(
            path.relative_to(self.fixture.root).as_posix()
            for path in self.fixture.root.rglob("*")
        )
        self.assertEqual(before, after)

    def test_output_is_deterministic_across_runs(self) -> None:
        self._failing_fixture()
        self.fixture.fixture(
            "hygiene_two",
            """\
            int main() {
            #if SHARP_RUNTIME_NEGATIVE_SITE == 1
                // NEGATIVE(also-legal): invalid conversion from 'const char*'
                int beta = 5;
                (void)beta;
            #endif
                return 0;
            }
            """,
        )
        first = self.check()
        second = self.check()
        self.assertEqual(first.problems, second.problems)
        self.assertEqual(
            [result.label for result in first.results],
            [result.label for result in second.results],
        )

    def test_a_requested_log_directory_receives_only_failures(self) -> None:
        self._failing_fixture()
        logs = self.fixture.root / "logs"
        report = self.check(log_directory=logs)
        self.assertFalse(report.ok)
        written = sorted(path.name for path in logs.iterdir())
        self.assertEqual(written, ["hygiene_negative.now-legal.log"])


class RealFixtureMutationTests(unittest.TestCase):
    """The permanent regression proof, on a real tracked fixture.

    ``build-probe/1801_mutation_campaign.py`` runs this over all seven fixtures
    and thirty-seven sites; that costs about ninety seconds, so the tracked gate
    keeps the cheapest real fixture -- one site, two compiles -- and asserts both
    directions: the unaltered fixture passes, and one legal spelling fails and is
    named.

    The mirror root symlinks ``cmake/``, ``modules/`` and ``vendor/`` at the real
    tree and copies the fixture, so no tracked file is ever modified.
    """

    FIXTURE = "collections_sorted_set_view_negative.cpp"
    MARKER = "const-getviewbetween"
    ORIGINAL = "    SortedSet<int> view = frozen.GetViewBetween(2, 4);"
    MADE_LEGAL = "    SortedSet<int> view = frozen;"

    def setUp(self) -> None:
        TEMPORARY_ROOT.mkdir(parents=True, exist_ok=True)
        self.root = Path(tempfile.mkdtemp(dir=TEMPORARY_ROOT, prefix="mirror "))
        self.addCleanup(shutil.rmtree, self.root, True)
        (self.root / "test/consumer").mkdir(parents=True)
        for name in ("cmake", "modules", "vendor"):
            (self.root / name).symlink_to(REPO_ROOT / name, target_is_directory=True)
        self.copy = self.root / "test/consumer" / self.FIXTURE
        shutil.copy2(REPO_ROOT / "test/consumer" / self.FIXTURE, self.copy)

    def check(self):
        return CHECKER.check_repository(self.root, patterns=[self.FIXTURE])

    def test_the_unaltered_copy_passes(self) -> None:
        report = self.check()
        self.assertTrue(report.ok, report.problems)
        self.assertEqual(report.site_count, 1)

    def test_making_the_marked_expression_legal_is_caught_and_named(self) -> None:
        text = self.copy.read_text(encoding="utf-8")
        self.assertEqual(text.count(self.ORIGINAL), 1)
        self.copy.write_text(text.replace(self.ORIGINAL, self.MADE_LEGAL), encoding="utf-8")
        report = self.check()
        self.assertFalse(report.ok)
        self.assertEqual(len(report.problems), 1, report.problems)
        self.assertIn(f"[{self.MARKER}]", report.problems[0])
        self.assertIn("COMPILED", report.problems[0])


class UnitTests(unittest.TestCase):
    def test_normalisation_folds_quotes_and_whitespace(self) -> None:
        self.assertEqual(
            CHECKER.normalise("cannot bind\n   ‘std::any&’   here"),
            "cannot bind 'std::any&' here",
        )

    def test_a_required_from_here_note_is_a_located_diagnostic(self) -> None:
        log = (
            "/usr/include/c++/14/any: In instantiation of 'T std::any_cast(any&&)':\n"
            "test/consumer/x_negative.cpp:119:52:   required from here\n"
            "  119 |     std::string& r = std::any_cast<std::string&>(t[k]);\n"
            "/usr/include/c++/14/any:342:9: error: static assertion failed: "
            "Template argument must be constructible from an rvalue.\n"
        )
        diagnostics = CHECKER.parse_diagnostics(log)
        kinds = [(item.kind, Path(item.path).name, item.line) for item in diagnostics]
        self.assertIn(("required from here", "x_negative.cpp", 119), kinds)
        self.assertIn(("error", "any", 342), kinds)
        self.assertIn("static assertion failed", CHECKER.error_text(diagnostics))

    def test_a_warning_does_not_absorb_the_following_notes(self) -> None:
        log = (
            "a.cpp:1:1: warning: unused variable 'x'\n"
            "a.cpp:2:2: note: a note about the warning\n"
            "a.cpp:3:3: error: the real error\n"
            "a.cpp:4:4: note: a note about the error\n"
        )
        text = CHECKER.error_text(CHECKER.parse_diagnostics(log))
        self.assertIn("the real error", text)
        self.assertIn("a note about the error", text)
        self.assertNotIn("a note about the warning", text)

    def test_the_unlimited_error_option_follows_the_compiler(self) -> None:
        self.assertEqual(
            CHECKER.unlimited_errors_option("g++ (Debian 14.2.0-19) 14.2.0"),
            "-fmax-errors=0",
        )
        self.assertEqual(
            CHECKER.unlimited_errors_option("Debian clang version 19.1.7"),
            "-ferror-limit=0",
        )

    def test_the_real_repository_registers_the_fixture_components(self) -> None:
        """The resolver must agree with CMake about the real component graph."""
        includes = CHECKER.component_include_directories(REPO_ROOT)
        collections = includes["Collections.Core"]
        self.assertIn(REPO_ROOT / "modules/collections/include", collections)
        self.assertIn(REPO_ROOT / "modules/core/include", collections)
        object_model = includes["Collections.ObjectModel"]
        self.assertIn(REPO_ROOT / "modules/collections-object-model/include", object_model)
        self.assertIn(REPO_ROOT / "modules/collections/include", object_model)


if __name__ == "__main__":
    unittest.main()
