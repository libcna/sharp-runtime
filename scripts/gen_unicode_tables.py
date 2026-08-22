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

One generator over one source of record, emitting one header per table. #2018 extended the
category table with simple case mapping; #2338 records why full normalization/decomposition stays
outside the invariant subset rather than adding a second generator.

Usage:  scripts/gen_unicode_tables.py [--source PATH] [--check|--verify]

`--check` regenerates into memory and diffs against the committed file, exiting non-zero if they
differ. It requires the external source of record. `--verify` always checks the committed UCD 16.0
snapshot's cryptographic digest, declarations and trie bounds; when the source of record exists it
also performs the exact regeneration check. It never downloads a replacement, in accordance with
SA-4, so it is suitable for CI checkouts that do not have `/rv/tmp/runtime`.
"""
import argparse
import hashlib
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

# Offline identity of the committed UCD 16.0 snapshot. These hashes were recorded from files that
# pass the exact `--check` against SA-4's source of record. An explicit Unicode bump therefore
# updates the source version, generated headers and these two values in one reviewed change.
COMMITTED_INTEGRITY = {
    "UnicodeCategoryTable.hpp": {
        "sha256": "eb48a03c60eb264840f0cd1e713cdfe7514aa35a1ae312bdce637ce4cfae95e5",
        "size": 140589,
        "arrays": {
            "CategoryCasingLevel1Index": ("uint8_t", 2176),
            "CategoryCasingLevel2Index": ("uint8_t", 6912),
            "CategoryCasingLevel3Index": ("uint8_t", 12528),
            "CategoriesValues": ("uint8_t", 241),
            "UppercaseValues": ("int16_t", 241),
            "LowercaseValues": ("int16_t", 241),
        },
    },
    "UnicodeNumericTable.hpp": {
        "sha256": "15feb37d01193227b5770ad91d1a635328c93820a7b3aca82e9110bfe389e69f",
        "size": 90620,
        "arrays": {
            "NumericGraphemeLevel1Index": ("uint8_t", 2176),
            "NumericGraphemeLevel2Index": ("uint8_t", 5248),
            "NumericGraphemeLevel3Index": ("uint8_t", 6400),
            "DigitValues": ("uint8_t", 177),
            "NumericValues": ("double", 177),
        },
    },
}

GENERATED_ARRAY_RE = re.compile(
    r"inline constexpr (uint8_t|int16_t|double) k([A-Za-z0-9_]+)\[(\d+)\] = \{\n"
    r"(.*?)\n\};",
    re.DOTALL,
)


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


def _parse_generated_arrays(text):
    """Return generated C++ arrays as ``name -> (type, declared count, tokens)``."""
    result = {}
    for kind, name, count_text, body in GENERATED_ARRAY_RE.findall(text):
        if name in result:
            raise ValueError(f"array k{name} is declared more than once")
        tokens = [token.strip() for token in body.split(",") if token.strip()]
        result[name] = (kind, int(count_text), tokens)
    return result


def _integer_values(array, name, problems):
    """Parse one integer generated array, reporting an invalid literal without throwing."""
    kind, _, tokens = array
    try:
        return [int(token, 0) for token in tokens]
    except ValueError as error:
        problems.append(f"k{name} contains an invalid {kind} literal: {error}")
        return []


def _validate_trie(level1, level2, level3, value_count, label, problems):
    """Prove the generated 11:5:4 trie cannot address beyond any committed array."""
    if len(level1) != 2176:
        problems.append(f"{label} level-1 table must cover 0x110000 code points (2176 entries)")
        return

    level2_indexes = set()
    for first in set(level1):
        for suffix in range(0, 64, 2):
            offset = (first << 6) + suffix
            if offset + 1 >= len(level2):
                problems.append(
                    f"{label} trie level 1 selects level-2 byte {offset + 1}, "
                    f"outside {len(level2)} bytes"
                )
                return
            level2_indexes.add(level2[offset] | (level2[offset + 1] << 8))

    used_level3_offsets = set()
    for second in level2_indexes:
        last = (second << 4) + 15
        if last >= len(level3):
            problems.append(
                f"{label} trie level 2 selects level-3 byte {last}, "
                f"outside {len(level3)} bytes"
            )
            return
        used_level3_offsets.update(range(second << 4, last + 1))

    for offset in used_level3_offsets:
        if level3[offset] >= value_count:
            problems.append(
                f"{label} trie level-3 byte {offset} selects value {level3[offset]}, "
                f"outside {value_count} values"
            )
            return


def validate_committed_integrity(out_dir, ucd_version="16.0"):
    """Validate the committed snapshot without requiring any external Unicode checkout."""
    problems = []
    parsed_by_file = {}
    for filename, expected in COMMITTED_INTEGRITY.items():
        path = out_dir / filename
        if not path.is_file():
            problems.append(f"missing generated Unicode table: {path}")
            continue
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if len(raw) != expected["size"]:
            problems.append(
                f"{path} has {len(raw)} bytes; committed UCD {ucd_version} snapshot has "
                f"{expected['size']}"
            )
        if digest != expected["sha256"]:
            problems.append(
                f"{path} SHA-256 is {digest}; committed UCD {ucd_version} snapshot is "
                f"{expected['sha256']}"
            )

        text = raw.decode("utf-8", errors="replace")
        if (filename == "UnicodeCategoryTable.hpp" and
                f'kUnicodeVersion = "{ucd_version}"' not in text):
            problems.append(f"{path} does not declare UCD {ucd_version}")
        try:
            arrays = _parse_generated_arrays(text)
        except ValueError as error:
            problems.append(f"{path}: {error}")
            continue
        parsed_by_file[filename] = arrays
        expected_arrays = expected["arrays"]
        missing = sorted(set(expected_arrays) - set(arrays))
        extra = sorted(set(arrays) - set(expected_arrays))
        if missing:
            problems.append(f"{path} is missing generated array(s): {missing}")
        if extra:
            problems.append(f"{path} has unexpected generated array(s): {extra}")
        for name in sorted(set(expected_arrays) & set(arrays)):
            kind, declared_count, tokens = arrays[name]
            expected_kind, expected_count = expected_arrays[name]
            if kind != expected_kind or declared_count != expected_count:
                problems.append(
                    f"{path}: k{name} declares {kind}[{declared_count}], expected "
                    f"{expected_kind}[{expected_count}]"
                )
            if len(tokens) != declared_count:
                problems.append(
                    f"{path}: k{name} declares {declared_count} entries but contains {len(tokens)}"
                )

    category = parsed_by_file.get("UnicodeCategoryTable.hpp", {})
    required_category = {
        "CategoryCasingLevel1Index", "CategoryCasingLevel2Index",
        "CategoryCasingLevel3Index", "CategoriesValues", "UppercaseValues", "LowercaseValues",
    }
    if required_category <= set(category):
        values = {
            name: _integer_values(category[name], name, problems)
            for name in required_category
        }
        if all(values.get(name) for name in required_category):
            value_count = len(values["CategoriesValues"])
            if (len(values["UppercaseValues"]) != value_count or
                    len(values["LowercaseValues"]) != value_count):
                problems.append(
                    "category, uppercase and lowercase value tables must have equal lengths"
                )
            _validate_trie(
                values["CategoryCasingLevel1Index"],
                values["CategoryCasingLevel2Index"],
                values["CategoryCasingLevel3Index"],
                value_count,
                "category/casing",
                problems,
            )
            if any((value & 0x1F) > 29 for value in values["CategoriesValues"]):
                problems.append("category value table contains an undefined UnicodeCategory value")

    numeric = parsed_by_file.get("UnicodeNumericTable.hpp", {})
    required_numeric = {
        "NumericGraphemeLevel1Index", "NumericGraphemeLevel2Index",
        "NumericGraphemeLevel3Index", "DigitValues", "NumericValues",
    }
    if required_numeric <= set(numeric):
        integer_names = required_numeric - {"NumericValues"}
        integers = {
            name: _integer_values(numeric[name], name, problems)
            for name in integer_names
        }
        numeric_count = numeric["NumericValues"][1]
        if len(integers.get("DigitValues", [])) != numeric_count:
            problems.append("digit and numeric value tables must have equal lengths")
        if all(integers.get(name) for name in integer_names):
            _validate_trie(
                integers["NumericGraphemeLevel1Index"],
                integers["NumericGraphemeLevel2Index"],
                integers["NumericGraphemeLevel3Index"],
                numeric_count,
                "numeric",
                problems,
            )
            if any(
                (value & 0x0F) > 10 or (value >> 4) > 10
                for value in integers["DigitValues"]
            ):
                problems.append("digit value table contains an out-of-domain encoded digit")

    return problems


def check_exact(source, out_dir, ucd_version):
    """Compare committed headers with an in-memory generation from SA-4's source."""
    text = source.read_text(encoding="utf-8", errors="replace")
    problems = []
    messages = []
    for filename, ticket, names in TABLES:
        arrays = {name: extract(text, name) for name in names}
        generated = emit(arrays, names, ucd_version, ticket)
        out = out_dir / filename
        counts = ", ".join(f"{name}={len(arrays[name])}" for name in names)
        if not out.exists():
            problems.append(f"MISSING {out}")
        elif out.read_text(encoding="utf-8") != generated:
            problems.append(f"DIFFERS {out} -- hand-edited, or the source of record moved")
        else:
            messages.append(
                f"OK: {out} matches the generator (UCD {ucd_version}; {counts})"
            )
    return messages, problems


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    p.add_argument("--out-dir", type=Path, default=OUT_DIR)
    p.add_argument("--ucd-version", default="16.0")
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true")
    mode.add_argument(
        "--verify",
        action="store_true",
        help="check committed integrity, and exact regeneration when --source exists",
    )
    args = p.parse_args()

    if args.verify:
        integrity_problems = validate_committed_integrity(args.out_dir, args.ucd_version)
        if integrity_problems:
            for problem in integrity_problems:
                print(f"INTEGRITY ERROR: {problem}", file=sys.stderr)
            sys.exit(1)
        print(
            f"OK: committed Unicode tables are the structurally valid UCD {args.ucd_version} "
            "snapshot (SHA-256 verified)"
        )
        if not args.source.is_file():
            print(
                f"NOTE: exact regeneration skipped because SA-4 source is absent: "
                f"{args.source}; no download attempted"
            )
            return
        messages, exact_problems = check_exact(args.source, args.out_dir, args.ucd_version)
        for message in messages:
            print(message)
        if exact_problems:
            for problem in exact_problems:
                print(problem, file=sys.stderr)
            sys.exit(1)
        return

    if not args.source.is_file():
        sys.exit(f"source of record not present: {args.source}\n"
                 "SA-4 names this file; without it the tables cannot be regenerated, which is why "
                 "the generated headers are committed rather than built.")

    if args.check:
        messages, problems = check_exact(args.source, args.out_dir, args.ucd_version)
        for message in messages:
            print(message)
        if problems:
            for problem in problems:
                print(problem, file=sys.stderr)
            sys.exit(1)
        return

    text = args.source.read_text(encoding="utf-8", errors="replace")

    for filename, ticket, names in TABLES:
        arrays = {n: extract(text, n) for n in names}
        generated = emit(arrays, names, args.ucd_version, ticket)
        out = args.out_dir / filename
        counts = ", ".join(f"{n}={len(arrays[n])}" for n in names)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(generated, encoding="utf-8")
        print(f"wrote {out} ({len(generated)} bytes; UCD {args.ucd_version}; {counts})")


if __name__ == "__main__":
    main()
