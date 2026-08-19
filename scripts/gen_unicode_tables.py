#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Generate the Unicode data tables System::Globalization::CharUnicodeInfo consumes.

Tickets #2315 (general category) and #2336 (Numeric_Type/Numeric_Value), under
`docs/StandingApprovals.md` SA-4, which names the source of record:

    /rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/Globalization/
    CharUnicodeInfoData.cs

-- the table .NET itself consumes, MIT-licensed and already covered by this project's .NET
attribution header. `/rv/tmp/runtime` declares `<UnicodeUcdVersion>16.0</UnicodeUcdVersion>`, so
the derived table is at Unicode 16.0 and parity with .NET is DERIVED rather than declared.

The layout is .NET's own 11:5:4 three-level trie, copied wholesale rather than re-derived: a
different packing would be a second table to keep correct, and the point of SA-4 is that there is
one source of record.

One generator over one source of record, emitting one header per table. #2018 (simple case
mapping) and #2338 (decomposition) will extend this rather than adding generators of their own.

Usage:  scripts/gen_unicode_tables.py [--source PATH] [--check]

`--check` regenerates into memory and diffs against the committed file, exiting non-zero if they
differ; that is what makes the committed table verifiable rather than merely present.
"""
import argparse
import re
import struct
import sys
from pathlib import Path

DEFAULT_SOURCE = Path("/rv/tmp/runtime/src/libraries/System.Private.CoreLib/src/System/"
                      "Globalization/CharUnicodeInfoData.cs")
OUT_DIR = Path("modules/core/include/System/Globalization/detail")

# One entry per emitted header: (filename, ticket, arrays, extra emitter).
#
# GraphemeSegmentationValues is deliberately NOT extracted: grapheme segmentation is not ported,
# and generating data no caller reads would be committing a table nothing verifies. The casing
# tables joined in #2018 and share the CATEGORY trie -- .NET indexes all three with
# GetCategoryCasingTableOffsetNoBoundsChecks, which is why they live in the same header.
TABLES = [
    ("UnicodeCategoryTable.hpp", "#2315", [
        "CategoryCasingLevel1Index",
        "CategoryCasingLevel2Index",
        "CategoryCasingLevel3Index",
        "CategoriesValues",
        "UppercaseValues",
        "LowercaseValues",
    ]),
    ("UnicodeNumericTable.hpp", "#2336", [
        "NumericGraphemeLevel1Index",
        "NumericGraphemeLevel2Index",
        "NumericGraphemeLevel3Index",
        "DigitValues",
        "NumericValues",
    ]),
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


def emit(arrays, names, ucd_version, ticket):
    w = []
    a = w.append
    a("// SPDX-License-Identifier: MIT")
    a("// Copyright (c) Robert Vokac and contributors")
    a("// Portions based on .NET runtime API (MIT License, Copyright .NET Foundation and Contributors)")
    a("//")
    a("// GENERATED FILE -- DO NOT EDIT BY HAND.")
    a(f"// Ticket {ticket}. Regenerate with scripts/gen_unicode_tables.py; verify with its --check.")
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
    if "CategoriesValues" in names:
        a("/** @brief The UCD version these tables were generated from. Pinned by SA-4. */")
        a(f'inline constexpr const char* kUnicodeVersion = "{ucd_version}";')
        a("")
    for name in names:
        data = arrays[name]
        if name in ("UppercaseValues", "LowercaseValues"):
            # 16-bit SIGNED deltas added to the code point (CharUnicodeInfo.cs:280-291), stored
            # little-endian. Emitted as int16_t so the lookup adds rather than reassembling.
            assert len(data) % 2 == 0, f"{name} is not a whole number of int16"
            vals = [struct.unpack("<h", bytes(data[i:i + 2]))[0] for i in range(0, len(data), 2)]
            a(f"/** @brief .NET's `{name}`, decoded to signed deltas. {len(vals)} entries. */")
            a(f"inline constexpr int16_t k{name}[{len(vals)}] = {{")
            for i in range(0, len(vals), 12):
                a("    " + ", ".join(str(v) for v in vals[i:i + 12]) + ",")
            a("};")
            a("")
            continue
        if name == "NumericValues":
            # A ReadOnlySpan<byte> of little-endian doubles, indexed as offset * sizeof(double)
            # (CharUnicodeInfo.cs:263). Emitted as doubles so the lookup needs no byte assembly
            # and no strict-aliasing cast; the bit patterns are preserved exactly.
            assert len(data) % 8 == 0, "NumericValues is not a whole number of doubles"
            vals = [struct.unpack("<d", bytes(data[i:i + 8]))[0] for i in range(0, len(data), 8)]
            a(f"/** @brief .NET's `{name}`, decoded from its little-endian bytes. {len(vals)} doubles. */")
            a(f"inline constexpr double k{name}[{len(vals)}] = {{")
            for i in range(0, len(vals), 4):
                a("    " + ", ".join(_dbl(v) for v in vals[i:i + 4]) + ",")
            a("};")
            a("")
            continue
        a(f"/** @brief .NET's `{name}`, transcribed. {len(data)} bytes. */")
        a(f"inline constexpr uint8_t k{name}[{len(data)}] = {{")
        for i in range(0, len(data), 16):
            a("    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
        a("};")
        a("")
    a("}  // namespace System::Globalization::detail")
    return "\n".join(w) + "\n"


def _dbl(v):
    """A C++ literal that round-trips this double exactly."""
    if v != v:
        return "__builtin_nan(\"\")"
    return repr(v) if "." in repr(v) or "e" in repr(v) or "n" in repr(v) else repr(v) + ".0"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    p.add_argument("--out-dir", type=Path, default=OUT_DIR)
    p.add_argument("--ucd-version", default="16.0")
    p.add_argument("--check", action="store_true")
    args = p.parse_args()

    if not args.source.exists():
        sys.exit(f"source of record not present: {args.source}\n"
                 "SA-4 names this file; without it the tables cannot be regenerated, which is why "
                 "the generated headers are committed rather than built.")
    text = args.source.read_text(encoding="utf-8", errors="replace")

    failed = False
    for filename, ticket, names in TABLES:
        arrays = {n: extract(text, n) for n in names}
        generated = emit(arrays, names, args.ucd_version, ticket)
        out = args.out_dir / filename
        counts = ", ".join(f"{n}={len(arrays[n])}" for n in names)
        if args.check:
            if not out.exists():
                print(f"MISSING {out}", file=sys.stderr)
                failed = True
                continue
            if out.read_text(encoding="utf-8") != generated:
                print(f"DIFFERS {out} -- hand-edited, or the source of record moved.",
                      file=sys.stderr)
                failed = True
                continue
            print(f"OK: {out} matches the generator (UCD {args.ucd_version}; {counts})")
        else:
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(generated, encoding="utf-8")
            print(f"wrote {out} ({len(generated)} bytes; UCD {args.ucd_version}; {counts})")
    if failed:
        sys.exit(1)


if __name__ == "__main__":
    main()
