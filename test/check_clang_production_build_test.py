#!/usr/bin/env python3
"""Focused regression tests for the cheap Clang production warning gate."""

from __future__ import annotations

import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import textwrap
import unittest


REPOSITORY = Path(__file__).resolve().parents[1]
SCRIPT = REPOSITORY / "scripts" / "check_clang_production_build.sh"
LOCAL_CI = REPOSITORY / "scripts" / "local_ci_check.sh"
WORKFLOW = REPOSITORY / ".github" / "workflows" / "components.yml"
COMMON_CMAKE = REPOSITORY / "cmake" / "SharpRuntimeCommon.cmake"


class ClangProductionBuildGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.calls = self.root / "cmake-calls.jsonl"

        self.write_executable(
            "clang",
            """
            #!/usr/bin/env bash
            if [ "${1:-}" = "--version" ]; then
                echo "mock clang version 1.0"
                exit 0
            fi
            exit 0
            """,
        )
        self.write_executable(
            "clang++",
            """
            #!/usr/bin/env bash
            if [ "${1:-}" = "--version" ]; then
                echo "mock clang++ version 1.0"
                exit 0
            fi
            exit 0
            """,
        )
        self.write_executable(
            "cmake",
            """
            #!/usr/bin/env python3
            import json
            import os
            from pathlib import Path
            import sys

            args = sys.argv[1:]
            with Path(os.environ["FAKE_CMAKE_CALLS"]).open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(args) + "\\n")

            mode = os.environ.get("FAKE_CMAKE_MODE", "ok")
            if "--build" in args:
                if mode == "build-warning":
                    print("mock.cpp:1: warning: warning that escaped -Werror")
                if mode == "build-error":
                    print("mock.cpp:1: error: compile failed")
                    raise SystemExit(1)
                raise SystemExit(0)

            build_dir = Path(args[args.index("-B") + 1])
            build_dir.mkdir(parents=True, exist_ok=True)
            if mode == "no-production-commands":
                commands = []
            else:
                warning_flag = "" if mode == "missing-werror" else " -Werror"
                commands = [{
                    "directory": str(build_dir),
                    "command": (
                        "mock-clang++ -Wall -Wextra" + warning_flag
                        + " -c /repo/modules/core/src/System/Mock.cpp"
                    ),
                    "file": "/repo/modules/core/src/System/Mock.cpp",
                }]
            (build_dir / "compile_commands.json").write_text(
                json.dumps(commands, indent=2), encoding="utf-8"
            )
            """,
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_executable(self, name: str, body: str) -> None:
        path = self.bin / name
        path.write_text(textwrap.dedent(body).lstrip(), encoding="utf-8")
        path.chmod(path.stat().st_mode | stat.S_IXUSR)

    def run_gate(
        self,
        *,
        mode: str = "ok",
        jobs: str = "2",
        extra_environment: dict[str, str] | None = None,
    ) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment.update(
            {
                "PATH": f"{self.bin}{os.pathsep}{environment['PATH']}",
                "FAKE_CMAKE_CALLS": str(self.calls),
                "FAKE_CMAKE_MODE": mode,
                "SHARP_RUNTIME_BUILD_JOBS": jobs,
            }
        )
        if extra_environment:
            environment.update(extra_environment)
        return subprocess.run(
            [str(SCRIPT)],
            cwd=REPOSITORY,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )

    def read_calls(self) -> list[list[str]]:
        if not self.calls.exists():
            return []
        return [json.loads(line) for line in self.calls.read_text(encoding="utf-8").splitlines()]

    def test_gate_configures_every_production_component_with_clang_and_no_tests(self) -> None:
        completed = self.run_gate()
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        calls = self.read_calls()
        self.assertEqual(len(calls), 2)
        configure, build = calls

        self.assertIn("-DSHARP_RUNTIME_COMPONENTS=All", configure)
        self.assertIn("-DSHARP_RUNTIME_BUILD_TESTS=OFF", configure)
        self.assertIn("-DSHARP_RUNTIME_BUILD_BENCHMARKS=OFF", configure)
        self.assertIn(f"-DCMAKE_C_COMPILER={self.bin / 'clang'}", configure)
        self.assertIn(f"-DCMAKE_CXX_COMPILER={self.bin / 'clang++'}", configure)
        self.assertIn("-DCMAKE_C_COMPILER_LAUNCHER=", configure)
        self.assertIn("-DCMAKE_CXX_COMPILER_LAUNCHER=", configure)

        build_dir = Path(configure[configure.index("-B") + 1])
        self.assertTrue(build_dir.is_relative_to(REPOSITORY / "build-tmp"))
        self.assertFalse(build_dir.exists(), "the temporary Clang tree was not removed")
        self.assertEqual(build[-2:], ["--parallel", "2"])
        self.assertIn("1 translation unit(s), 0 warnings, 0 errors", completed.stdout)

    def test_gate_rejects_a_production_command_without_werror(self) -> None:
        completed = self.run_gate(mode="missing-werror")
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("does not carry -Werror", completed.stderr)
        self.assertEqual(len(self.read_calls()), 1, "build must not start after policy drift")

    def test_gate_rejects_a_warning_even_if_the_build_command_exits_zero(self) -> None:
        completed = self.run_gate(mode="build-warning")
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("produced 1 warning(s) and 0 error(s)", completed.stderr)

    def test_gate_rejects_more_than_two_jobs_before_configuring(self) -> None:
        completed = self.run_gate(jobs="3")
        self.assertEqual(completed.returncode, 2)
        self.assertIn("aggregate compilation ceiling is two jobs", completed.stderr)
        self.assertEqual(self.read_calls(), [])

    def test_gate_rejects_a_missing_explicit_clang_compiler(self) -> None:
        completed = self.run_gate(
            extra_environment={"SHARP_RUNTIME_CLANG_CXX_COMPILER": "missing-clang++"}
        )
        self.assertEqual(completed.returncode, 2)
        self.assertIn("was not found", completed.stderr)
        self.assertEqual(self.read_calls(), [])

    def test_warning_policy_and_local_ci_workflow_wiring_are_pinned(self) -> None:
        common = COMMON_CMAKE.read_text(encoding="utf-8")
        local_ci = LOCAL_CI.read_text(encoding="utf-8")
        workflow = WORKFLOW.read_text(encoding="utf-8")

        self.assertIn(
            'target_compile_options("${target}" PRIVATE -Wall -Wextra -Werror)', common
        )
        self.assertIn("python3 test/check_clang_production_build_test.py", local_ci)
        self.assertIn("scripts/check_clang_production_build.sh", local_ci)
        self.assertIn("scripts/local_ci_check.sh build", workflow)


if __name__ == "__main__":
    unittest.main()
