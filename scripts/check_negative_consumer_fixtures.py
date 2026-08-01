#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Prove that every marked site in every negative consumer fixture is REJECTED.

A *negative consumer fixture* is a file under ``test/consumer/`` that writes
source a remediation ticket made illegal on purpose, so that the ticket's claim
("the old spelling no longer compiles") is checked by the compiler instead of
being asserted in a doc-comment.

"The file failed to compile" proves almost nothing. One broken line hides every
other line: a fixture with eleven marked sites still exits non-zero when ten of
them have silently become legal again. Ticket #1796 established the per-site
rule and wrote ``build-probe/1796_check_negative.py`` for one fixture; that
checker was never committed, so no tracked job ran it. Ticket #1801 replaces it
with this one.

How a fixture declares itself
-----------------------------

A fixture carries one directive naming the component whose public include
surface it consumes, and a numbered preprocessor guard per negative site::

    // NEGATIVE-FIXTURE: component=Collections.Core

    #ifndef SHARP_RUNTIME_NEGATIVE_SITE
    #define SHARP_RUNTIME_NEGATIVE_SITE 0
    #endif
    ...
    #if SHARP_RUNTIME_NEGATIVE_SITE == 1
        // NEGATIVE(indexer-alias-bind): cannot bind non-const lvalue reference
        //     | invalid initialization of reference
        std::any& alias = table["alpha"];
    #else
        std::any alias = table["alpha"];      // the migrated spelling
    #endif

The site numbers must be ``1..N``. The fixture is compiled ``N + 1`` times: once
with **no** site enabled, which must SUCCEED with zero diagnostics, and once per
site with only that site enabled, which must FAIL.

Why the all-sites-off baseline matters
--------------------------------------

It is what makes "the site failed for its own reason" a proof rather than a
hope. Enabling a guard can only ADD uses of the surrounding scaffolding, never
remove one, so if the baseline is diagnostic-free then any diagnostic in a
single-site variant is caused by that site. On top of that the checker requires
every diagnostic located *in the fixture* to fall inside the enabled guard, so a
site that happens to fail because an unrelated line broke is reported as a
failure, not as a pass.

The baseline also keeps the tracked fixtures honest C++: with no ``-D`` they
compile, so an IDE does not present them as permanently broken files, and the
``#else`` branch of each guard documents the migration in compilable form.

Rules enforced
--------------

1.  Every ``test/consumer/*_negative.cpp`` carries a ``NEGATIVE-FIXTURE``
    directive; a fixture that does not is reported, never skipped.
2.  The directive names a component the repository's own CMake metadata knows,
    and the include surface is derived from that metadata rather than duplicated.
3.  The fixture defaults ``SHARP_RUNTIME_NEGATIVE_SITE`` to ``0`` itself.
4.  Site numbers are exactly ``1..N``, with no gap and no duplicate.
5.  Each site region contains exactly one ``NEGATIVE(id)`` marker with at least
    one expected-diagnostic fragment; ids are unique within a fixture.
6.  A ``NEGATIVE(id)`` marker outside every site region is a stale marker.
7.  The baseline compile succeeds with no diagnostic at all.
8.  Every site compile fails.
9.  Every diagnostic located in the fixture -- error or instantiation context --
    falls inside the enabled site region.
10. At least one diagnostic is attributed to the region, so "the fixture stopped
    being compiled" cannot be mistaken for "the fixture passed".
11. At least one of the site's declared fragments appears in the error text.

Usage:
    scripts/check_negative_consumer_fixtures.py [--root PATH] [--jobs N]
        [--compiler CXX] [--fixture PATTERN] [--list] [--verbose]
        [--log-directory PATH] [--timeout SECONDS] [--no-ccache]

Job precedence is ``--jobs``, then ``SHARP_RUNTIME_BUILD_JOBS``, then the safe
repository default of 2. Values outside 1..2 are rejected.

Exit codes: 0 every site rejected; 1 a contract failure; 2 a usage or
environment failure (missing compiler, unreadable metadata, invalid job count).
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import fnmatch
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor


SCRIPTS_DIRECTORY = Path(__file__).resolve().parent
REPO_ROOT = SCRIPTS_DIRECTORY.parent
if str(SCRIPTS_DIRECTORY) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIRECTORY))

from generate_component_catalog import _load_metadata  # noqa: E402
from job_count_policy import (  # noqa: E402
    DEFAULT_JOBS,
    ENVIRONMENT_VARIABLE,
    MAXIMUM_JOBS,
    JobPolicyError,
    resolve_jobs,
)

DEFAULT_TIMEOUT_SECONDS = 300.0
FIXTURE_DIRECTORY = Path("test/consumer")
FIXTURE_GLOB = "*_negative.cpp"
SITE_MACRO = "SHARP_RUNTIME_NEGATIVE_SITE"
LANGUAGE_STANDARD = "c++23"
WARNING_OPTIONS = ("-Wall", "-Wextra", "-Wpedantic", "-Werror")

_FIXTURE_DIRECTIVE_RE = re.compile(r"^\s*//\s*NEGATIVE-FIXTURE:\s*(?P<body>\S.*?)\s*$")
_SITE_GUARD_RE = re.compile(
    rf"^\s*#\s*if\s+{SITE_MACRO}\s*==\s*(?P<number>\d+)\s*$"
)
_MARKER_RE = re.compile(r"^\s*//\s*NEGATIVE\((?P<id>[^)]*)\)\s*:\s*(?P<fragment>\S.*?)\s*$")
_CONTINUATION_RE = re.compile(r"^\s*//\s*\|\s*(?P<fragment>\S.*?)\s*$")
_MARKER_ANYWHERE_RE = re.compile(r"//\s*NEGATIVE\(")
_PRELUDE_RE = re.compile(rf"^\s*#\s*ifndef\s+{SITE_MACRO}\s*$", re.MULTILINE)
_IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")
_CONDITIONAL_OPEN_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef)\b")
_CONDITIONAL_MIDDLE_RE = re.compile(r"^\s*#\s*(else|elif)\b")
_CONDITIONAL_CLOSE_RE = re.compile(r"^\s*#\s*endif\b")

_DIAGNOSTIC_RE = re.compile(
    r"^(?P<path>[^\s].*?):(?P<line>\d+):(?P<column>\d+):\s+"
    r"(?P<kind>fatal error|error|warning|note|required from here)\b"
)
_LOCATIONLESS_DIAGNOSTIC_RE = re.compile(
    r"^(?P<path>[^\s].*?):\s+(?P<kind>fatal error|error|warning)\b"
)
_WHITESPACE_RE = re.compile(r"\s+")
# GCC quotes identifiers with U+2018/U+2019 in a UTF-8 locale and with an ASCII
# apostrophe under LC_ALL=C. Compilation forces LC_ALL=C for determinism, and
# normalisation folds the directional forms anyway so a fragment written either
# way keeps matching if that ever changes.
_QUOTE_TRANSLATION = str.maketrans(
    {
        "‘": "'",
        "’": "'",
        "“": '"',
        "”": '"',
        "«": '"',
        "»": '"',
    }
)
# A compiler must answer in the C locale so that one message is one message on
# every machine that runs the gate.
COMPILER_ENVIRONMENT_OVERRIDES = {
    "LC_ALL": "C",
    "LANG": "C",
    "LANGUAGE": "C",
}


def normalise(text: str) -> str:
    """Collapse whitespace and fold quotes, so a wrapped or locale-quoted
    diagnostic still matches a fragment."""
    return _WHITESPACE_RE.sub(" ", text.translate(_QUOTE_TRANSLATION)).strip()


# --------------------------------------------------------------------------
# fixture model
# --------------------------------------------------------------------------


@dataclass(frozen=True)
class NegativeSite:
    number: int
    identifier: str
    fragments: tuple[str, ...]
    guard_line: int
    first_line: int
    last_line: int

    def contains(self, line: int) -> bool:
        return self.first_line <= line <= self.last_line


@dataclass(frozen=True)
class NegativeFixture:
    path: Path
    display: str
    component: str
    definitions: tuple[str, ...]
    standard: str
    sites: tuple[NegativeSite, ...]


@dataclass
class CompileOutcome:
    command: tuple[str, ...]
    returncode: int
    log: str
    seconds: float
    timed_out: bool = False


@dataclass
class SiteResult:
    fixture: str
    site: NegativeSite | None
    label: str
    ok: bool
    detail: str
    outcome: CompileOutcome | None


@dataclass
class Report:
    problems: list[str] = field(default_factory=list)
    fixtures: list[NegativeFixture] = field(default_factory=list)
    results: list[SiteResult] = field(default_factory=list)
    compiler: str = ""
    compiler_version: str = ""
    invocations: int = 0
    peak_jobs: int = 0
    seconds: float = 0.0

    @property
    def ok(self) -> bool:
        return not self.problems

    @property
    def site_count(self) -> int:
        return sum(len(fixture.sites) for fixture in self.fixtures)


class EnvironmentProblem(Exception):
    """A usage or environment failure, distinct from a fixture contract failure."""


# --------------------------------------------------------------------------
# parsing
# --------------------------------------------------------------------------


def _parse_directive(body: str, display: str) -> tuple[dict[str, list[str]], list[str]]:
    problems: list[str] = []
    values: dict[str, list[str]] = {}
    for item in body.split():
        if "=" not in item:
            problems.append(
                f"{display}: NEGATIVE-FIXTURE directive item '{item}' is not key=value"
            )
            continue
        key, value = item.split("=", 1)
        values.setdefault(key, []).append(value)
    return values, problems


def _site_regions(lines: list[str], display: str) -> tuple[dict[int, tuple[int, int, int]], list[str]]:
    """Map a site number to ``(guard line, first line, last line)``, 1-based."""
    problems: list[str] = []
    regions: dict[int, tuple[int, int, int]] = {}
    for index, line in enumerate(lines):
        match = _SITE_GUARD_RE.match(line)
        if match is None:
            continue
        number = int(match.group("number"))
        depth = 1
        terminator = -1
        for probe in range(index + 1, len(lines)):
            text = lines[probe]
            if _CONDITIONAL_OPEN_RE.match(text):
                depth += 1
            elif _CONDITIONAL_CLOSE_RE.match(text):
                depth -= 1
                if depth == 0:
                    terminator = probe
                    break
            elif _CONDITIONAL_MIDDLE_RE.match(text) and depth == 1:
                terminator = probe
                break
        if terminator == -1:
            problems.append(
                f"{display}:{index + 1}: site guard for {SITE_MACRO} == {number} "
                "is never closed"
            )
            continue
        if number in regions:
            problems.append(
                f"{display}:{index + 1}: site number {number} is guarded twice "
                f"(first at line {regions[number][0]})"
            )
            continue
        regions[number] = (index + 1, index + 2, terminator + 1)
    return regions, problems


def _parse_marker(
    lines: list[str], first_line: int, last_line: int
) -> tuple[str | None, tuple[str, ...], list[int]]:
    """Return the marker id, its fragments, and every marker line in the region."""
    identifier: str | None = None
    fragments: list[str] = []
    marker_lines: list[int] = []
    collecting = False
    for number in range(first_line, last_line + 1):
        text = lines[number - 1]
        match = _MARKER_RE.match(text)
        if match is not None:
            marker_lines.append(number)
            collecting = identifier is None
            if identifier is None:
                identifier = match.group("id").strip()
                fragments.append(normalise(match.group("fragment")))
            continue
        continuation = _CONTINUATION_RE.match(text)
        if continuation is not None and collecting:
            fragments.append(normalise(continuation.group("fragment")))
            continue
        collecting = False
    return identifier, tuple(fragments), marker_lines


def parse_fixture(path: Path, display: str, text: str) -> tuple[NegativeFixture | None, list[str]]:
    problems: list[str] = []
    lines = text.split("\n")

    directives = [
        match.group("body")
        for match in (_FIXTURE_DIRECTIVE_RE.match(line) for line in lines)
        if match is not None
    ]
    if not directives:
        problems.append(
            f"{display}: no 'NEGATIVE-FIXTURE:' directive; every negative consumer "
            "fixture must declare the component whose headers it consumes so that "
            "this checker cannot skip it silently"
        )
        return None, problems
    if len(directives) > 1:
        problems.append(
            f"{display}: {len(directives)} 'NEGATIVE-FIXTURE:' directives; expected one"
        )
        return None, problems

    values, directive_problems = _parse_directive(directives[0], display)
    problems.extend(directive_problems)
    components = values.pop("component", [])
    if len(components) != 1:
        problems.append(
            f"{display}: NEGATIVE-FIXTURE directive must name exactly one "
            f"component= (found {len(components)})"
        )
        return None, problems
    definitions = tuple(values.pop("define", []))
    standards = values.pop("standard", [])
    if len(standards) > 1:
        problems.append(f"{display}: NEGATIVE-FIXTURE names {len(standards)} standards")
        return None, problems
    standard = standards[0] if standards else LANGUAGE_STANDARD
    for unknown in sorted(values):
        problems.append(
            f"{display}: NEGATIVE-FIXTURE directive key '{unknown}' is not recognised "
            "(component, define, standard)"
        )

    if _PRELUDE_RE.search(text) is None:
        problems.append(
            f"{display}: no '#ifndef {SITE_MACRO}' prelude; a fixture must default "
            "the site macro itself so that it compiles with no -D"
        )

    regions, region_problems = _site_regions(lines, display)
    problems.extend(region_problems)

    sites: list[NegativeSite] = []
    identifiers: dict[str, int] = {}
    claimed: list[tuple[int, int]] = []
    for number in sorted(regions):
        guard_line, first_line, last_line = regions[number]
        claimed.append((first_line, last_line))
        identifier, fragments, marker_lines = _parse_marker(lines, first_line, last_line)
        if identifier is None:
            problems.append(
                f"{display}:{guard_line}: site {number} has no "
                "'// NEGATIVE(id): expected diagnostic' marker"
            )
            continue
        if len(marker_lines) > 1:
            problems.append(
                f"{display}:{guard_line}: site {number} carries {len(marker_lines)} "
                f"NEGATIVE markers (lines {', '.join(str(item) for item in marker_lines)}); "
                "one site has one marker"
            )
            continue
        if not _IDENTIFIER_RE.match(identifier):
            problems.append(
                f"{display}:{marker_lines[0]}: marker id '{identifier}' must be "
                "lower-case kebab-case"
            )
            continue
        if identifier in identifiers:
            problems.append(
                f"{display}:{marker_lines[0]}: duplicate marker id '{identifier}' "
                f"(already used by site {identifiers[identifier]})"
            )
            continue
        if not fragments:
            problems.append(
                f"{display}:{marker_lines[0]}: marker '{identifier}' declares no "
                "expected diagnostic fragment"
            )
            continue
        identifiers[identifier] = number
        sites.append(
            NegativeSite(
                number=number,
                identifier=identifier,
                fragments=fragments,
                guard_line=guard_line,
                first_line=first_line,
                last_line=last_line - 1,
            )
        )

    for number, line in enumerate(lines, 1):
        if _MARKER_ANYWHERE_RE.search(line) is None:
            continue
        if any(first <= number <= last for first, last in claimed):
            continue
        problems.append(
            f"{display}:{number}: stale NEGATIVE marker outside every "
            f"'#if {SITE_MACRO} == N' region"
        )

    expected = list(range(1, len(regions) + 1))
    if sorted(regions) != expected:
        problems.append(
            f"{display}: site numbers are {sorted(regions)}; they must be "
            f"{expected} with no gap"
        )
    if not regions:
        problems.append(
            f"{display}: no '#if {SITE_MACRO} == N' site; a negative fixture with no "
            "site asserts nothing"
        )

    if problems:
        return None, problems
    return (
        NegativeFixture(
            path=path,
            display=display,
            component=components[0],
            definitions=definitions,
            standard=standard,
            sites=tuple(sites),
        ),
        [],
    )


# --------------------------------------------------------------------------
# component include resolution
# --------------------------------------------------------------------------


def component_include_directories(root: Path) -> dict[str, tuple[Path, ...]]:
    """Component name -> its consumer include surface, from CMake metadata.

    This mirrors ``sharp_runtime_register_component``: a component contributes
    its own ``INCLUDE_DIRECTORY`` and, transitively, that of every PUBLIC
    dependency. Private dependencies are deliberately not followed -- they are
    not part of the consumer include surface, which is the whole point of a
    consumer fixture.
    """
    try:
        modules, compatibility = _load_metadata(root)
    except (OSError, ValueError) as error:
        raise EnvironmentProblem(f"cannot read component metadata: {error}") from error

    own: dict[str, Path | None] = {}
    dependencies: dict[str, tuple[str, ...]] = {}
    for module in modules:
        own[module.name] = root / "modules" / module.directory / "include"
        dependencies[module.name] = tuple(
            sorted(module.public_dependencies | module.legacy_dependencies)
        )
    for component in compatibility:
        if component.name in own:
            continue
        if component.component_type.startswith("alias of"):
            own[component.name] = None
        else:
            own[component.name] = component.include_directory
        dependencies[component.name] = tuple(component.dependencies)

    resolved: dict[str, tuple[Path, ...]] = {}

    def resolve(name: str, seen: frozenset[str]) -> tuple[Path, ...]:
        if name in resolved:
            return resolved[name]
        if name in seen:
            return ()
        collected: list[Path] = []
        directory = own.get(name)
        if directory is not None:
            collected.append(directory)
        for dependency in dependencies.get(name, ()):
            for item in resolve(dependency, seen | {name}):
                if item not in collected:
                    collected.append(item)
        ordered = tuple(collected)
        if not seen:
            resolved[name] = ordered
        return ordered

    for name in list(own):
        resolve(name, frozenset())
    return resolved


# --------------------------------------------------------------------------
# compilation
# --------------------------------------------------------------------------


class JobMeter:
    """Counts concurrent compiler processes so the ceiling can be asserted."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self.active = 0
        self.peak = 0

    def __enter__(self) -> JobMeter:
        with self._lock:
            self.active += 1
            self.peak = max(self.peak, self.active)
        return self

    def __exit__(self, *_: object) -> None:
        with self._lock:
            self.active -= 1


def unlimited_errors_option(compiler_version: str) -> str:
    """GCC and Clang spell "report every error, do not stop early" differently."""
    if "clang" in compiler_version.lower():
        return "-ferror-limit=0"
    return "-fmax-errors=0"


def build_command(
    compiler_prefix: tuple[str, ...],
    fixture: NegativeFixture,
    include_directories: tuple[Path, ...],
    vendor_directory: Path | None,
    site_number: int,
    unlimited_errors: str = "-fmax-errors=0",
) -> tuple[str, ...]:
    command: list[str] = list(compiler_prefix)
    command.append(f"-std={fixture.standard}")
    command.extend(WARNING_OPTIONS)
    command.extend(("-fsyntax-only", unlimited_errors, "-fdiagnostics-color=never"))
    if vendor_directory is not None:
        command.extend(("-isystem", str(vendor_directory)))
    for directory in include_directories:
        command.extend(("-I", str(directory)))
    for definition in fixture.definitions:
        command.append(f"-D{definition}")
    command.append(f"-D{SITE_MACRO}={site_number}")
    command.append(str(fixture.path))
    return tuple(command)


def run_compile(
    command: tuple[str, ...], timeout: float, meter: JobMeter
) -> CompileOutcome:
    started = time.monotonic()
    environment = dict(os.environ)
    environment.update(COMPILER_ENVIRONMENT_OVERRIDES)
    with meter:
        try:
            process = subprocess.run(
                list(command),
                capture_output=True,
                text=True,
                timeout=timeout,
                check=False,
                env=environment,
            )
        except subprocess.TimeoutExpired as expired:
            return CompileOutcome(
                command=command,
                returncode=-1,
                log=(expired.stdout or "") + (expired.stderr or ""),
                seconds=time.monotonic() - started,
                timed_out=True,
            )
        except OSError as error:
            raise EnvironmentProblem(
                f"cannot run the compiler ({command[0]}): {error}"
            ) from error
    return CompileOutcome(
        command=command,
        returncode=process.returncode,
        log=process.stdout + process.stderr,
        seconds=time.monotonic() - started,
    )


# --------------------------------------------------------------------------
# diagnostic analysis
# --------------------------------------------------------------------------


@dataclass
class Diagnostic:
    path: str
    line: int | None
    kind: str
    text: str


def parse_diagnostics(log: str) -> list[Diagnostic]:
    diagnostics: list[Diagnostic] = []
    current: list[str] | None = None
    for raw in log.split("\n"):
        match = _DIAGNOSTIC_RE.match(raw)
        locationless = None if match is not None else _LOCATIONLESS_DIAGNOSTIC_RE.match(raw)
        if match is not None or locationless is not None:
            source = match if match is not None else locationless
            assert source is not None
            kind = source.group("kind")
            if kind == "fatal error":
                kind = "error"
            line_group = source.groupdict().get("line")
            diagnostics.append(
                Diagnostic(
                    path=source.group("path"),
                    line=int(line_group) if line_group else None,
                    kind=kind,
                    text=raw,
                )
            )
            current = [raw]
            continue
        if current is not None and diagnostics:
            current.append(raw)
            diagnostics[-1].text = "\n".join(current)
    return diagnostics


def error_text(diagnostics: list[Diagnostic]) -> str:
    """The text of every error, with the notes that GCC attaches to it."""
    chunks: list[str] = []
    attaching = False
    for diagnostic in diagnostics:
        if diagnostic.kind == "error":
            attaching = True
            chunks.append(diagnostic.text)
        elif diagnostic.kind == "note":
            if attaching:
                chunks.append(diagnostic.text)
        else:
            attaching = False
    return normalise("\n".join(chunks))


def evaluate_site(
    fixture: NegativeFixture, site: NegativeSite, outcome: CompileOutcome
) -> tuple[bool, str]:
    label = f"{fixture.display} [{site.identifier}]"
    if outcome.timed_out:
        return False, f"{label}: the compiler did not finish within the timeout"
    if outcome.returncode == 0:
        return False, (
            f"{label}: site {site.number} COMPILED; lines "
            f"{site.first_line}-{site.last_line} are legal again"
        )

    diagnostics = parse_diagnostics(outcome.log)
    fixture_name = fixture.path.name
    in_fixture = [
        diagnostic
        for diagnostic in diagnostics
        if diagnostic.line is not None
        and Path(diagnostic.path).name == fixture_name
        and diagnostic.kind in {"error", "required from here"}
    ]
    outside = [
        diagnostic
        for diagnostic in in_fixture
        if diagnostic.line is not None and not site.contains(diagnostic.line)
    ]
    if outside:
        places = ", ".join(
            f"line {diagnostic.line} ({diagnostic.kind})" for diagnostic in outside
        )
        return False, (
            f"{label}: diagnostics outside the site region "
            f"({site.first_line}-{site.last_line}): {places}; the site's own "
            "rejection cannot be trusted while unrelated source is broken"
        )
    if not in_fixture:
        return False, (
            f"{label}: the compile failed but no diagnostic names lines "
            f"{site.first_line}-{site.last_line}; the fixture may no longer be "
            "compiled at all"
        )

    text = error_text(diagnostics)
    if not text:
        return False, f"{label}: the compile failed with no error diagnostic"
    for fragment in site.fragments:
        if fragment in text:
            return True, fragment
    return False, (
        f"{label}: none of the expected diagnostics matched. Expected one of: "
        + "; ".join(f"'{fragment}'" for fragment in site.fragments)
    )


def evaluate_baseline(
    fixture: NegativeFixture, outcome: CompileOutcome
) -> tuple[bool, str]:
    label = f"{fixture.display} [baseline]"
    if outcome.timed_out:
        return False, f"{label}: the compiler did not finish within the timeout"
    if outcome.returncode != 0:
        return False, (
            f"{label}: the fixture does not compile with every site disabled, so no "
            "site result can be attributed to its own source"
        )
    diagnostics = [
        diagnostic
        for diagnostic in parse_diagnostics(outcome.log)
        if diagnostic.kind in {"error", "warning"}
    ]
    if diagnostics:
        return False, (
            f"{label}: the baseline compile emitted {len(diagnostics)} diagnostic(s); "
            "the baseline must be clean for a per-site result to be sound"
        )
    return True, "clean"


# --------------------------------------------------------------------------
# driver
# --------------------------------------------------------------------------


def discover_fixtures(
    root: Path, patterns: list[str]
) -> tuple[list[NegativeFixture], list[str]]:
    problems: list[str] = []
    fixtures: list[NegativeFixture] = []
    directory = root / FIXTURE_DIRECTORY
    if not directory.is_dir():
        problems.append(f"fixture directory not found: {FIXTURE_DIRECTORY}")
        return fixtures, problems
    for path in sorted(directory.glob(FIXTURE_GLOB)):
        display = path.relative_to(root).as_posix()
        if patterns and not any(
            fnmatch.fnmatch(display, pattern) or fnmatch.fnmatch(path.name, pattern)
            for pattern in patterns
        ):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except OSError as error:
            problems.append(f"cannot read {display}: {error}")
            continue
        fixture, fixture_problems = parse_fixture(path, display, text)
        problems.extend(fixture_problems)
        if fixture is not None:
            fixtures.append(fixture)
    return fixtures, problems


def resolve_compiler(compiler: str | None, use_ccache: bool) -> tuple[tuple[str, ...], str, str]:
    name = compiler or os.environ.get("CXX") or "g++"
    executable = shutil.which(name)
    if executable is None:
        raise EnvironmentProblem(
            f"compiler '{name}' was not found; pass --compiler or set CXX"
        )
    try:
        version = subprocess.run(
            [executable, "--version"],
            capture_output=True,
            text=True,
            timeout=60,
            check=False,
        ).stdout.split("\n")[0]
    except (OSError, subprocess.SubprocessError) as error:
        raise EnvironmentProblem(f"cannot query '{name}' for its version: {error}") from error
    prefix: tuple[str, ...] = (executable,)
    if use_ccache:
        launcher = shutil.which("ccache")
        if launcher is not None:
            prefix = (launcher, executable)
    return prefix, name, version.strip()


def check_repository(
    root: Path,
    *,
    compiler: str | None = None,
    jobs: int | str | None = None,
    patterns: list[str] | None = None,
    timeout: float = DEFAULT_TIMEOUT_SECONDS,
    use_ccache: bool = True,
    log_directory: Path | None = None,
) -> Report:
    try:
        resolved_jobs = resolve_jobs(jobs)
    except JobPolicyError as problem:
        raise EnvironmentProblem(str(problem)) from problem
    root = root.resolve()
    report = Report()
    started = time.monotonic()

    fixtures, problems = discover_fixtures(root, patterns or [])
    report.fixtures = fixtures
    report.problems.extend(problems)
    if not fixtures:
        report.problems.append(
            f"no negative consumer fixture was found under {FIXTURE_DIRECTORY}/"
            f"{FIXTURE_GLOB}; this checker must never pass vacuously"
        )
        report.seconds = time.monotonic() - started
        return report

    includes = component_include_directories(root)
    vendor = root / "vendor"
    vendor_directory = vendor if vendor.is_dir() else None

    prefix, name, version = resolve_compiler(compiler, use_ccache)
    report.compiler = name
    report.compiler_version = version

    cases: list[tuple[NegativeFixture, NegativeSite | None]] = []
    for fixture in fixtures:
        if fixture.component not in includes:
            report.problems.append(
                f"{fixture.display}: component '{fixture.component}' is not "
                "registered in the repository's CMake component metadata"
            )
            continue
        cases.append((fixture, None))
        cases.extend((fixture, site) for site in fixture.sites)

    if not cases:
        report.seconds = time.monotonic() - started
        return report

    meter = JobMeter()

    def execute(case: tuple[NegativeFixture, NegativeSite | None]) -> SiteResult:
        fixture, site = case
        number = 0 if site is None else site.number
        command = build_command(
            prefix,
            fixture,
            includes[fixture.component],
            vendor_directory,
            number,
            unlimited_errors_option(report.compiler_version),
        )
        outcome = run_compile(command, timeout, meter)
        if site is None:
            ok, detail = evaluate_baseline(fixture, outcome)
            label = f"{fixture.display} [baseline]"
        else:
            ok, detail = evaluate_site(fixture, site, outcome)
            label = f"{fixture.display} [{site.identifier}]"
        return SiteResult(
            fixture=fixture.display,
            site=site,
            label=label,
            ok=ok,
            detail=detail,
            outcome=outcome,
        )

    with ThreadPoolExecutor(max_workers=resolved_jobs) as pool:
        results = list(pool.map(execute, cases))

    report.results = results
    report.invocations = len(results)
    report.peak_jobs = meter.peak
    for result in results:
        if result.ok:
            continue
        report.problems.append(result.detail)
        if log_directory is not None and result.outcome is not None:
            log_directory.mkdir(parents=True, exist_ok=True)
            stem = Path(result.fixture).stem
            suffix = "baseline" if result.site is None else result.site.identifier
            (log_directory / f"{stem}.{suffix}.log").write_text(
                " ".join(result.outcome.command) + "\n\n" + result.outcome.log,
                encoding="utf-8",
            )
    report.seconds = time.monotonic() - started
    return report


def _print_inventory(report: Report) -> None:
    for fixture in report.fixtures:
        print(f"{fixture.display}  component={fixture.component}  sites={len(fixture.sites)}")
        for site in fixture.sites:
            print(
                f"    {site.number:>2}  {site.identifier:<38} "
                f"lines {site.first_line}-{site.last_line}"
            )
            for fragment in site.fragments:
                print(f"        expect: {fragment}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Per-site validation of the negative consumer fixtures."
    )
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root")
    parser.add_argument("--compiler", default=None, help="C++ compiler (default: $CXX or g++)")
    parser.add_argument(
        "--jobs",
        default=None,
        help=(
            f"parallel compiler processes in 1..{MAXIMUM_JOBS}; overrides "
            "$SHARP_RUNTIME_BUILD_JOBS; safe default 2"
        ),
    )
    parser.add_argument(
        "--fixture",
        action="append",
        default=[],
        metavar="PATTERN",
        help="only check fixtures matching this glob (repeatable)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_SECONDS,
        help="seconds allowed for one compiler process",
    )
    parser.add_argument("--no-ccache", action="store_true", help="do not use ccache")
    parser.add_argument(
        "--log-directory",
        type=Path,
        default=None,
        help="write the full compiler log of every FAILING case here",
    )
    parser.add_argument("--list", action="store_true", help="print the fixture inventory")
    parser.add_argument("--verbose", action="store_true", help="print every case")
    args = parser.parse_args(argv)

    try:
        report = check_repository(
            args.root,
            compiler=args.compiler,
            jobs=args.jobs,
            patterns=args.fixture,
            timeout=args.timeout,
            use_ccache=not args.no_ccache,
            log_directory=args.log_directory,
        )
    except EnvironmentProblem as problem:
        print(f"FAIL: {problem}", file=sys.stderr)
        return 2

    if args.list:
        _print_inventory(report)

    if args.verbose:
        print(f"compiler: {report.compiler_version}")
        for result in report.results:
            print(f"    {'OK  ' if result.ok else 'FAIL'}  {result.label}: {result.detail}")

    if report.ok:
        print(
            f"OK: {len(report.fixtures)} negative consumer fixture(s), "
            f"{report.site_count} negative site(s), every site rejected "
            f"({report.compiler}, {report.invocations} compiler invocation(s), "
            f"peak {report.peak_jobs} job(s), {report.seconds:.1f}s)"
        )
        return 0
    print(
        f"FAIL: {len(report.problems)} negative-fixture problem(s) found:",
        file=sys.stderr,
    )
    for problem in report.problems:
        print(f"  - {problem}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
