#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Guard the one-definition rule for Sharp Runtime's test-only access seams.

A *test-only access seam* is a class template that a production header
DECLARES inside ``namespace SharpRuntime::Testing`` and never defines, and that
one or more production classes befriend. Two exist today:

* ``CollectionVersionAccess`` -- declared by
  ``System/Collections/detail/MutationCounter.hpp``, befriended by fifteen
  collections and by ``detail::BasicMutationCounter``;
* ``SortedSetVersionAccess`` -- declared by
  ``System/Collections/Generic/SortedSet.hpp``.

Because the declaration is production and the definition is test-only, a
consumer can never name a complete type -- which is the point. But nothing in
the language stops two test translation units of the SAME program from each
defining the same specialisation with a DIFFERENT body. That is a one-definition
rule violation, [basic.def.odr]/12, and it is ill-formed with NO DIAGNOSTIC
REQUIRED: no linker error, no ``-flto -Wodr`` warning, no sanitizer report. It
happened -- five suites, three specialisations, two bodies each -- and ticket
#1800 measured the consequence: at ``-O0`` the link order decides which body the
whole program executes; at ``-O1`` and above the two units inline their own
bodies and disagree with each other inside one process.

This checker makes that structurally impossible rather than merely unlikely. It
reads source text directly and needs no configured build.

Rules enforced:

1. No seam specialisation may be defined in a production tree
   (``modules/*/include`` or ``modules/*/src``). That is what keeps the seam
   unreachable from a consumer, as
   ``test/consumer/collections_mutation_version_negative.cpp`` proves.
2. Each ``(seam, template-argument list)`` pair may be defined in exactly ONE
   file. Suites share it by including that file.
3. All definitions of one pair must be token-for-token identical. Rule 2 makes
   this unreachable today; it is the rule that still holds if rule 2 is ever
   given an exception.
4. Within one file, the seam bodies that are written as a macro invocation must
   all invoke the SAME macro. This is the check that catches the divergence
   coming back INSIDE the authoritative header -- exactly how ``SR1787_SEAM_BODY``
   and ``SR1794_SEAM_BODY`` diverged when they lived in separate files.

Usage:
    scripts/check_version_seam_odr.py [--root PATH]
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import hashlib
from pathlib import Path
import re
import sys


REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl"}
SKIPPED_DIRECTORIES = {"vendor", ".git", "docs", "audit", "cmake-build-debug"}
PRODUCTION_SECTIONS = ("include", "src")
SEAM_NAMESPACE_RE = re.compile(r"namespace\s+SharpRuntime\s*::\s*Testing\b")

# Deliberately small C++ tokeniser. `>>` is NOT produced as one token, so a
# nested template argument list closes correctly; the seams contain no shift
# operator, so nothing is lost.
_TOKEN_RE = re.compile(
    r"""
    (?P<ws>\s+)
  | (?P<id>[A-Za-z_][A-Za-z_0-9]*)
  | (?P<num>[0-9][0-9A-Za-z_.']*)
  | (?P<punc>
        \.\.\.|->\*|<=>
      | ::|->|\+\+|--|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\^=|\|=|\.\*
      | [-+*/%^&|~!<>=?:;,.(){}\[\]#\\@$]
    )
  | (?P<other>.)
    """,
    re.VERBOSE,
)
_MACRO_NAME_RE = re.compile(r"^[A-Z][A-Z0-9_]*$")


@dataclass(frozen=True)
class SeamDefinition:
    seam: str
    arguments: str
    path: str
    digest: str
    macro: str | None


@dataclass
class SeamReport:
    problems: list[str] = field(default_factory=list)
    seams: dict[str, str] = field(default_factory=dict)
    definitions: list[SeamDefinition] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.problems


def strip_comments_and_literals(text: str) -> str:
    """Replace comments with a space and string/character literals with a
    placeholder, so neither can perturb a token comparison."""
    out: list[str] = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        pair = text[index : index + 2]
        if pair == "//":
            end = text.find("\n", index)
            index = length if end == -1 else end
            out.append(" ")
        elif pair == "/*":
            end = text.find("*/", index + 2)
            index = length if end == -1 else end + 2
            out.append(" ")
        elif char in {'"', "'"}:
            quote = char
            index += 1
            while index < length:
                if text[index] == "\\":
                    index += 2
                    continue
                if text[index] == quote:
                    index += 1
                    break
                index += 1
            out.append("0")
        else:
            out.append(char)
            index += 1
    return "".join(out)


def tokenize(text: str) -> list[str]:
    tokens: list[str] = []
    position = 0
    while position < len(text):
        match = _TOKEN_RE.match(text, position)
        if match is None:  # pragma: no cover - the tokeniser matches every char
            position += 1
            continue
        position = match.end()
        if match.lastgroup == "ws":
            continue
        tokens.append(match.group())
    return tokens


def _matching(tokens: list[str], start: int, opener: str, closer: str) -> int:
    """Index of the token closing the group that opens at *start*, or -1."""
    depth = 0
    for index in range(start, len(tokens)):
        if tokens[index] == opener:
            depth += 1
        elif tokens[index] == closer:
            depth -= 1
            if depth == 0:
                return index
    return -1


def find_declared_seams(text: str) -> set[str]:
    """Names a production header declares, and does not define, inside
    ``namespace SharpRuntime::Testing``."""
    declared: set[str] = set()
    stripped = strip_comments_and_literals(text)
    if not SEAM_NAMESPACE_RE.search(stripped):
        return declared
    tokens = tokenize(stripped)
    for index in range(len(tokens) - 3):
        if tokens[index : index + 4] != ["namespace", "SharpRuntime", "::", "Testing"]:
            continue
        brace = index + 4
        while brace < len(tokens) and tokens[brace] != "{":
            if tokens[brace] == ";":
                brace = -1
                break
            brace += 1
        if brace <= 0:
            continue
        end = _matching(tokens, brace, "{", "}")
        if end == -1:
            continue
        body = tokens[brace + 1 : end]
        for position, token in enumerate(body):
            if token != "struct" and token != "class":
                continue
            if position + 2 < len(body) and body[position + 2] == ";":
                declared.add(body[position + 1])
    return declared


def find_seam_definitions(text: str, seams: set[str]) -> list[tuple[str, str, str, str | None]]:
    """Return (seam, argument spelling, body digest, macro name or None)."""
    stripped = strip_comments_and_literals(text)
    tokens = tokenize(stripped)
    found: list[tuple[str, str, str, str | None]] = []
    for index, token in enumerate(tokens):
        if token not in seams:
            continue
        if index == 0 or tokens[index - 1] not in {"struct", "class"}:
            continue
        if index >= 2 and tokens[index - 2] == "friend":
            continue
        cursor = index + 1
        arguments = ""
        if cursor < len(tokens) and tokens[cursor] == "<":
            end = _matching(tokens, cursor, "<", ">")
            if end == -1:
                continue
            arguments = " ".join(tokens[cursor + 1 : end])
            cursor = end + 1
        if cursor >= len(tokens) or tokens[cursor] != "{":
            continue  # a declaration, a friend, or a use -- not a definition
        end = _matching(tokens, cursor, "{", "}")
        if end == -1:
            continue
        body = tokens[cursor : end + 1]
        digest = hashlib.sha256(" ".join(body).encode()).hexdigest()[:16]
        macro = None
        inner = body[1:-1]
        if (
            len(inner) >= 3
            and _MACRO_NAME_RE.match(inner[0])
            and inner[1] == "("
            and _matching(inner, 1, "(", ")") == len(inner) - 1
        ):
            macro = inner[0]
        found.append((tokens[index], arguments, digest, macro))
    return found


def _iter_sources(root: Path):
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
            continue
        relative = path.relative_to(root)
        parts = relative.parts
        if parts[0] in SKIPPED_DIRECTORIES or parts[0].startswith("build"):
            continue
        yield path, relative.as_posix()


def check_repository(root: Path) -> SeamReport:
    root = root.resolve()
    report = SeamReport()

    sources: list[tuple[Path, str, str]] = []
    for path, display in _iter_sources(root):
        try:
            sources.append((path, display, path.read_text(encoding="utf-8", errors="replace")))
        except OSError as error:
            report.problems.append(f"cannot read {display}: {error}")

    for _, display, text in sources:
        parts = display.split("/")
        is_production_header = (
            len(parts) > 2 and parts[0] == "modules" and parts[2] == "include"
        )
        if not is_production_header:
            continue
        for seam in sorted(find_declared_seams(text)):
            if seam in report.seams and report.seams[seam] != display:
                report.problems.append(
                    f"test-only seam '{seam}' is declared by both "
                    f"'{report.seams[seam]}' and '{display}'"
                )
            else:
                report.seams[seam] = display

    if not report.seams:
        report.problems.append(
            "no test-only access seam was found; this checker expects at least "
            "SharpRuntime::Testing::CollectionVersionAccess to be declared by a "
            "production header"
        )
        return report

    seam_names = set(report.seams)
    macros_by_file: dict[str, dict[str, set[str]]] = {}
    for _, display, text in sources:
        for seam, arguments, digest, macro in find_seam_definitions(text, seam_names):
            report.definitions.append(
                SeamDefinition(seam, arguments, display, digest, macro)
            )
            if macro is not None:
                macros_by_file.setdefault(display, {}).setdefault(seam, set()).add(macro)

    # Rule 1: never in a production tree.
    for definition in report.definitions:
        parts = definition.path.split("/")
        if (
            len(parts) > 2
            and parts[0] == "modules"
            and parts[2] in PRODUCTION_SECTIONS
        ):
            report.problems.append(
                f"{definition.path} defines test-only seam "
                f"'{definition.seam}<{definition.arguments}>' in a production tree; "
                "a defined seam is reachable from a consumer"
            )

    # Rules 2 and 3: one defining file, one token body.
    by_key: dict[tuple[str, str], list[SeamDefinition]] = {}
    for definition in report.definitions:
        by_key.setdefault((definition.seam, definition.arguments), []).append(definition)
    for (seam, arguments), definitions in sorted(by_key.items()):
        files = sorted({definition.path for definition in definitions})
        digests = sorted({definition.digest for definition in definitions})
        spelling = f"{seam}<{arguments}>" if arguments else seam
        if len(files) > 1:
            report.problems.append(
                f"test-only seam '{spelling}' is defined in {len(files)} files "
                f"({', '.join(files)}); exactly one file may define it and the "
                "others must include that file"
            )
        if len(digests) > 1:
            report.problems.append(
                f"test-only seam '{spelling}' has {len(digests)} different token "
                f"bodies ({', '.join(digests)}); differing definitions of one "
                "specialisation violate the one-definition rule"
            )

    # Rule 4: one seam macro per file.
    for display in sorted(macros_by_file):
        for seam in sorted(macros_by_file[display]):
            macros = sorted(macros_by_file[display][seam])
            if len(macros) > 1:
                report.problems.append(
                    f"{display} writes '{seam}' bodies through {len(macros)} "
                    f"different macros ({', '.join(macros)}); one seam has one body"
                )

    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=REPO_ROOT,
        help="Sharp Runtime repository root",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="print the full seam-definition inventory",
    )
    args = parser.parse_args(argv)
    report = check_repository(args.root)

    if args.list:
        for definition in sorted(
            report.definitions,
            key=lambda item: (item.seam, item.arguments, item.path),
        ):
            spelling = (
                f"{definition.seam}<{definition.arguments}>"
                if definition.arguments
                else definition.seam
            )
            macro = definition.macro or "-"
            print(f"{definition.digest}  {macro:38}  {definition.path}  {spelling}")

    if report.ok:
        print(
            "OK: test-only access seams have one definition each "
            f"({len(report.seams)} seam(s), {len(report.definitions)} "
            "specialisation definition(s))"
        )
        return 0
    print(
        f"FAIL: {len(report.problems)} test-seam ODR problem(s) found:",
        file=sys.stderr,
    )
    for problem in report.problems:
        print(f"  - {problem}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
