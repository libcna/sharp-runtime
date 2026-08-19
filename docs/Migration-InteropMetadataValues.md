<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the interop metadata values match .NET (ticket #1980, group G-2)

*2026-08-19.* `System::Runtime::InteropServices`' `UnmanagedType`, `StructLayoutAttribute` and
`DllImportAttribute` carried wrong constants and wrong defaults. All of them now match the
reference.

Landed under **SA-5**. No layout, vtable, signature or `noexcept` change — only *values*.

---

## 1. Why the numbers are the whole contract

P/Invoke and interop are a **declared permanent deviation**: these types exist so ported
declarations keep compiling and so the **managed metadata values are preserved**, not to produce
a native effect. The header says so itself. Getting the numbers right is therefore not cosmetic —
it is the entire purpose of the types.

## 2. What changed

| Declaration | Was | Is | Reference |
|---|---|---|---|
| `UnmanagedType::LPStruct` | **48** | **43** | `UnmanagedType.cs:55` (`0x2b`) |
| `UnmanagedType::Currency` | **absent** | **15** | `UnmanagedType.cs:22` (`0xf`) |
| `UnmanagedType::IDispatch` | **absent** | **26** | `UnmanagedType.cs:30` (`0x1a`) |
| `StructLayoutAttribute::Pack` | **8** | **0** | `StructLayoutAttribute.cs:21` |
| `StructLayoutAttribute::CharSet` | `Ansi` (2) | **0** | `StructLayoutAttribute.cs:23` |
| `DllImportAttribute::CharSet` | `None` (1) | **0** | plain field, no initializer |
| `DllImportAttribute::PreserveSig` | **true** | **false** | `DllImportAttribute.cs:22` |
| `DllImportAttribute::BestFitMapping` | **true** | **false** | `DllImportAttribute.cs:21` |

## 3. The `LPStruct` finding was understated

The plan recorded *"`LPStruct` 48 → 43"* — a wrong number. Measured, it is worse than that: **48
is `LPUTF8Str`'s value**, so the two enumerators were **indistinguishable**.
`UnmanagedType::LPStruct == UnmanagedType::LPUTF8Str` was **true**, and a `switch` over
`UnmanagedType` could not carry both arms — it would not compile.

A test now asserts the inequality, not just the two numbers.

## 4. Two divergences the plan did not name

G-2's list named four items. Measuring the reference alongside them found a fifth and sixth: both
`CharSet` fields.

.NET declares `public CharSet CharSet;` on both attributes **with no initializer**, so the default
is `0` — and `CharSet` has **no enumerator with that value** (`None` is 1). This port defaulted
them to named values instead.

**Reproducing an unnamed default is deliberate**, and it follows directly from §1: the header
exists to preserve the managed metadata, and .NET's metadata really does carry an unset `CharSet`
here. A test states the reasoning so it is not later "corrected" back to a named value.

## 5. What did *not* change, and why that matters

Every other `UnmanagedType` value already matched .NET exactly — `Bool`, the whole `I1`…`U8`
block, `BStr`, `IUnknown`, `Struct`, `LPArray`, `CustomMarshaler`, `HString` and the rest. A test
asserts a representative spread of them, so the three repairs above cannot be mistaken for a
wholesale renumbering.

The same applies to `DllImportAttribute`'s booleans: `SetLastError`, `ExactSpelling` and
`ThrowOnUnmappableChar` were **already** `false`. That the port got three right and two wrong is
what makes `PreserveSig`/`BestFitMapping` a divergence rather than a deliberate policy.

## 6. Evidence

Five mutations, **all caught** — one per repaired value, plus the `CharSet` default:

| Mutation | Caught by |
|---|---|
| M1 — `LPStruct` back to 48 | `Fix1980G2_LPStructNoLongerCollidesWithLPUTF8Str` |
| M2 — `Currency` removed | **at compile time** — `error: 'Currency' is not a member of 'UnmanagedType'`, which is the only way C++ can report a missing enumerator |
| M3 — `Pack` back to 8 | `Fix1980G2_DefaultPackIsZeroNotEight` |
| M4 — `PreserveSig` back to true | `Fix1980G2_DllImportBoolDefaultsAreAllFalse` |
| M5 — `CharSet` named again | `Decl1980G2_BothCharSetDefaultsAreUnsetNotNamed` |

**One pre-existing test was inverted, and the plan predicted exactly that**:
`StructLayoutAttributeTests.DefaultPack_IsEight` pinned the wrong value, and the plan's own G-2
row said it *"would have to be rewritten"*. It is the only pre-existing test this group touches.

Gate: **17,503 run, 17,503 passed, 0 failed, 0 skipped** across 38 executables — `+5` on 17,498
(`SharpRuntimeTests_Runtime` 181 → 186; one pin inverted in place, five cases added). No other
executable moved. Module graph unchanged at 41/93.

## 7. Downstream, measured

`UnmanagedType`, `StructLayoutAttribute`, `DllImportAttribute` and `MarshalAsAttribute` appear in
**zero** places in `cna` and **zero** in `mobile-eggbert`. Neither repository was modified.

## 8. Scope

This closes **G-2** of #1980. Remaining: **G-3** (reparenting and sealing — a vtable *and* layout
change SA-3 excludes), **G-4** (removing `setIsOptionalProperty`, moving `Url` to a settable
property — mandatory migration), and **G-5** / SR-AUD-167 (retyping `MarshalAs` fields, adding
`ComInterfaceType`/`ClassInterfaceType`). G-1 landed earlier the same day.
