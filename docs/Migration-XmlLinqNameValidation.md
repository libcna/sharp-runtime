<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `System::Xml::Linq` validates element and attribute names when it serialises them (ticket #2350)

*2026-08-12.* This change is **source-, ABI-, layout-, vtable- and `noexcept`-compatible**. It is
**behaviour-incompatible on purpose**, in one direction only: a call that used to emit text can
now throw. Nothing that used to throw now succeeds, and every valid name keeps its bytes.

Durable design record:
[`docs/SystemXmlLinqNamespaceReviewPlan.md`](SystemXmlLinqNamespaceReviewPlan.md) §24.

## What was wrong

Every element and attribute has two serialisation doors:

| Door | Reached by | Before #2350 |
|---|---|---|
| `WriteTo(XmlWriter&)` | `Save(XmlWriter&)` | routed the name through `XmlConvert::VerifyName` (since #2076) — **rejected** an invalid name |
| `SerializeTo(ostream&)` | `ToString()`, `ToString(SaveOptions)`, `Save(fileName)`, and `XAttribute::ToString()` | built the text itself and **emitted the name unchecked** |

So one door refused what the other wrote. Measured over 37 names, the two disagreed on **26** —
at the element door and the attribute door alike:

| Constructed | `ToString()` emitted | `WriteTo` did | This runtime's own reader |
|---|---|---|---|
| `XElement(XName("a b"))` | `<a b/>` | threw | **rejects** |
| `XElement(XName("1bad"))` | `<1bad/>` | threw | **rejects** |
| `XElement(XName("a<b"))` | `<a<b/>` | threw | **rejects** |
| `XElement(XName())` (default, empty) | `</>` | threw | **rejects** |

## What changed

Both doors now apply the same shipped validator, `XmlConvert::VerifyName`, to the same string —
the **resolved qualified name** (`c`, `p:c`, `xmlns:p`), which is the only name a serialiser ever
writes. An invalid name throws `System::Xml::XmlException`; an **empty** name throws
`System::ArgumentException`, matching what the writer door already threw.

## What did **not** change

- **`XName` itself is untouched.** Constructing, storing, mutating (`setNameProperty`), comparing
  and hashing an invalid name are all still legal. Validation happens where the *text* is
  produced — the same boundary #2196 (the PI target) and #2200 (the DOCTYPE name) already hold.
  Only asking for the text throws.
- **Every valid name is byte-identical**, including namespace-prefixed names and UTF-8 names:
  the name predicates treat every byte ≥ 128 as a name character, so non-ASCII names are as
  accepted as they always were.
- **No parsed tree is affected.** `VerifyName` accepts every resolved name this runtime's parser
  can produce (measured, 12 of 12 parsed documents), and the parser rejects `<1bad/>`,
  `<.lead/>`, `<-lead/>`, `<a$b/>` and `<r 1x='v'/>` on its own. A parse → serialise round trip
  that worked before works now.

## If your code relied on the old behaviour

Validate names before serialising, or catch `System::Xml::XmlException`. If you were calling
`WriteTo`/`Save(XmlWriter&)` you already received this exception and nothing changes for you.

**One extra narrowing worth naming:** a name with a **leading colon** (`":x"`) used to emit from
the direct door and could be read back — but the reader silently renamed it to `x`, so that round
trip already lost the colon. It is rejected at both doors now. This is the same extra narrowing
[`Migration-XmlStrictnessAndLifecycle.md`](Migration-XmlStrictnessAndLifecycle.md) §2 recorded for
the writer door under #2076. Every other name this door now rejects produced output that this
runtime's own reader already refused to parse.
