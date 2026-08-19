#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Generate the Unicode general-category table for System::Globalization::CharUnicodeInfo.

Ticket #2315, under `docs/StandingApprovals.md` SA-4, which names the source of record:

    /rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Globalization/
    CharUnicodeInfoData.cs

-- the table .NET itself consumes, MIT-licensed and already covered by this project's .NET
attribution header. `/rv/tmp/runtime` declares `<UnicodeUcdVersion>16.0</UnicodeUcdVersion>`, so
the derived table is at Unicode 16.0 and parity with .NET is DERIVED rather than declared.

The layout is .NET's own 11:5:4 three-level trie, copied wholesale rather than re-derived: a
different packing would be a second table to keep correct, and the point of SA-4 is that there is
one source of record.

Usage:  scripts/gen_unicode_category_table.py [--source PATH] [--out PATH] [--check]

`--check` regenerates into memory and diffs against the committed file, exiting non-zero if they
differ; that is what makes the committed table verifiable rather than merely present.
"""
import argparse
import re
import sys
from pathlib import Path

DEFAULT_SOURCE = Path("/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/"
                      "Globalization/CharUnicodeInfoData.cs")
DEFAULT_OUT = Path("modules/core/include/System/Globalization/detail/UnicodeCategoryTable.hpp")

# The four arrays the category lookup needs. The casing tables are deliberately NOT extracted:
# they belong to #2018 (Rune simple case mapping), and generating data no caller reads would be
# committing a table nothing verifies.
WANTED = [
    ("CategoryCasingLevel1Index", "uint8_t"),
    ("CategoryCasingLevel2Index", "uint8_t"),
    ("CategoryCasingLevel3Index", "uint8_t"),
    ("CategoriesValues", "uint8_t"),
]


def extract(text, name):
    """Return the byte list of `private static ReadOnlySpan<byte> <name> => [ ... ];`."""
    m = re.search(r"ReadOnlySpan<byte>\s+" + re.escape(name) + r"\s*=>\s*(?://[^\n]*\n)?\s*\[",
                  text)
    if not m:
        raise SystemExit(f"array {name} not found in the source of record")
    i = m.end()
    depth = 1
    while depth:
        if text[i] == "[":
            depth += 1
        elif text[i] == "]":
            depth -= 1
        i += 1
    body = text[m.end():i - 1]
    return [int(tok, 16) for tok in re.findall(r"0x([0-9a-fA-F]{2})", body)]


def emit(arrays, ucd_version):
    w = []
    a = w.append
    a("// SPDX-License-Identifier: MIT")
    a("// Copyright (c) Robert Vokac and contributors")
    a("// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)")
    a("//")
    a("// GENERATED FILE -- DO NOT EDIT BY HAND.")
    a("// Regenerate with scripts/gen_unicode_category_table.py; verify with its --check.")
    a("//")
    a(f"// Unicode version: UCD {ucd_version}, pinned by docs/StandingApprovals.md SA-4 until an")
    a("// explicit bump ticket. Derived from .NET's own generated CharUnicodeInfoData.cs, which")
    a("// carries the same MIT licence as the rest of the .NET material this project ports, and")
    a("// which is itself generated from the Unicode Character Database by dotnet/runtime's")
    a("// GenUnicodeProp tool. The Unicode Character Database is (c) Unicode, Inc. and is used")
    a("// under the Unicode Licence.")
    a("#pragma once")
    a("")
    a("#include <cstddef>")
    a("#include <cstdint>")
    a("")
    a("namespace System::Globalization::detail {")
    a("")
    a("/** @brief The UCD version this table was generated from. Pinned by SA-4. */")
    a(f'inline constexpr const char* kUnicodeVersion = "{ucd_version}";')
    a("")
    for name, _ in WANTED:
        data = arrays[name]
        a(f"/** @brief .NET's `{name}`, transcribed. {len(data)} bytes. */")
        a(f"inline constexpr uint8_t k{name}[{len(data)}] = {{")
        for i in range(0, len(data), 16):
            a("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
        a("};")
        a("")
    a("}  // namespace System::Globalization::detail")
    return "\n".join(w) + "\n"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    p.add_argument("--out", type=Path, default=DEFAULT_OUT)
    p.add_argument("--ucd-version", default="16.0")
    p.add_argument("--check", action="store_true")
    args = p.parse_args()

    if not args.source.exists():
        sys.exit(f"source of record not present: {args.source}\n"
                 "SA-4 names this file; without it the table cannot be regenerated, which is why "
                 "the generated header is committed rather than built.")
    text = args.source.read_text(encoding="utf-8", errors="replace")
    arrays = {name: extract(text, name) for name, _ in WANTED}
    generated = emit(arrays, args.ucd_version)

    if args.check:
        if not args.out.exists():
            sys.exit(f"missing generated file {args.out}")
        if args.out.read_text(encoding="utf-8") != generated:
            sys.exit(f"{args.out} differs from what the generator produces -- it was hand-edited "
                     "or the source of record moved.")
        counts = ", ".join(f"{n}={len(arrays[n])}" for n, _ in WANTED)
        print(f"OK: {args.out} matches the generator (UCD {args.ucd_version}; {counts})")
        return
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(generated, encoding="utf-8")
    print(f"wrote {args.out} ({len(generated)} bytes; UCD {args.ucd_version})")


if __name__ == "__main__":
    main()
