<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->
# Migration: BinaryWriter character, string-literal, array and decimal overloads

2026-11-12, for CNA review rv-netapi #1 and #4 (CNA's `PacketWriter`/`PacketReader` inherit
`BinaryWriter`/`BinaryReader`).

## What changed

`BinaryWriter` gains non-virtual overloads: `Write(charcs)`, `Write(char)`, `Write(const char*)`,
`Write(const std::vector<bytecs>&)`, `Write(const bytecs (&)[N])`, `Write(const std::vector<charcs>&)`,
`Write(const charcs*, intcs index, intcs count)`, `Write(const charcs (&)[N])` and
`Write(const System::Decimal&)` (only where `SHARP_RUNTIME_HAS_NATIVE_INT128` is 1, as for `BinaryReader::ReadDecimal`; MSVC has no native 128-bit integer). `BinaryReader` gains the non-virtual `intcs Read()`.

## Why it is a behaviour change for existing calls

Before, these arguments silently converted to an existing overload and wrote the wrong bytes:

| Call | Before | Now (.NET Framework 4) |
|---|---|---|
| `Write(u'A')` | `Write(intcs)`: `41 00 00 00` | UTF-8 `41` |
| `Write('A')` | `Write(intcs)`: `41 00 00 00` | `41` |
| `Write("AB")` | `Write(bool)`: `01` | `02 41 42` |
| `Write(bytecs[2]{42,43})` | `Write(bool)`: `01` | `2A 2B` |
| `Write(charcs[2]{u'A',u'B'})` | `Write(bool)`: `01` | `41 42` |

A consumer that wrote such values and read them back with the matching mistaken reader call sees
different bytes now; one that relied on .NET's format is fixed.

## Layout and ABI

No virtual member is added and no existing signature changes: `sizeof`, the vtables and every
existing mangled symbol are unchanged. Consumers rebuild from source as usual; no
full-consumer-rebuild beyond the ordinary header change is required.
