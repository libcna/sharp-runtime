<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration: `System::Xml` gets strict names, real lifecycle states, and live node-change events

*Landed by tickets **#2074**, **#2075**, **#2076**, **#2077**, **#2078** and **#2079** on
2026-08-04, remediating audit findings **SR-AUD-348**, **349**, **350**, **351**, **352** and
**353**. Durable design record:
[`docs/SystemXmlNamespaceReviewPlan.md`](SystemXmlNamespaceReviewPlan.md) §4, §11 and §20.*

---

## 1. What changed, in one paragraph

`System::Xml` used to accept invalid input at several public doors and answer as if nothing
were wrong: an invalid `InnerXml` fragment **destroyed the node's existing children and threw
nothing**; four DOM mutators accepted a node belonging to a **different parent**; the writer
emitted names its own reader could not parse; a **closed** reader kept traversing and stopped
reporting itself as closed; `HasNamespace` denied prefixes its own sibling `LookupNamespace`
resolved; and six public event handlers were **never invoked**. Each of those is now either an
exception or a dispatched event. Nothing changed shape: **no public type, member signature,
object layout, vtable or exception specification was modified by any of the six tickets.**

## 2. Every behaviour change, and what to do about it

| # | Before | After | If your code relied on the old behaviour |
|---|---|---|---|
| **#2074** | `node.InnerXml = "<bad>"` emptied the node and returned normally | throws `System::Xml::XmlException`; the existing children survive intact | call `RemoveAll()` explicitly if you wanted the clearing side effect |
| **#2075** | `RemoveChild`/`InsertBefore`/`InsertAfter`/`ReplaceChild` accepted a node that is **not a child of the receiver** — silently detaching another parent's child, ignoring the reference node, or leaving the new node nowhere | throws `System::ArgumentException` | pass a node that is actually a child of the receiver; code that was mutating the wrong subtree by accident now stops loudly |
| **#2076** | the writer emitted `<1bad/>`, `<e 1bad="v"/>`, `<?a?>b d?>` and `<!DOCTYPE 1bad>`; every `Write*` stayed callable after `Close()`; an unbalanced `WriteEndElement()` was discarded | invalid names throw `XmlException` (empty names `ArgumentException`); writes after `Close()` and unbalanced/ownerless writes throw `System::InvalidOperationException` | validate names before writing, or catch `XmlException`. **Note the one extra narrowing:** a name with a **leading colon** (`":x"`) is now rejected, because that is the rule `XmlConvert::VerifyName` — and therefore `XmlDocument::CreateElement` — has always applied |
| **#2077** | `HasNamespace(prefix)` was `false` for a prefix declared in an **outer** scope, and for the built-in `xml` prefix | `true`, agreeing with `LookupNamespace` | nothing returned `true` before and `false` now; this direction only |
| **#2078** | `Read()` after `Close()` returned `true`, advanced the cursor, and reset `ReadState` to `Interactive`; `MoveToElement`/`MoveToNextAttribute`/`GetAttribute` kept answering | `Read()` returns `false`, the cursor never moves, `ReadState` stays `Closed`, and every accessor reports the reader's "no current node" answer (`None` / `""` / `false`) | do not call `Close()` until you have finished reading. `EndOfFile` is unchanged |
| **#2079** | the six `XmlDocument` node-change handlers were **never invoked** | they are dispatched by every mutation whose affected node survives the operation | **read §3 — this is the only change that can run your code where none ran before** |

## 3. #2079 needs a second look before you upgrade

The other five tickets can only make a call that used to succeed start throwing. #2079 is
different: `NodeInserting`, `NodeInserted`, `NodeRemoving`, `NodeRemoved`, `NodeChanging` and
`NodeChanged` are public `std::function` **data members**. If your code assigned one of them —
for logging, for a dirty flag, for anything — that callable has never run. **It runs now.**

What dispatches:

- **Insert pair** — `PrependChild`, `AppendChild`, `InsertBefore`, `InsertAfter`,
  `ReplaceChild`, `InnerXml =`, `InnerText =`, and once per child when a
  `XmlDocumentFragment` is appended or prepended.
- **Remove pair** — `RemoveChild`, and `ReplaceChild`'s removal half. Appending a fragment
  also raises it once per child as each leaves the fragment.
- **Change pair** — `XmlCharacterData`'s data/value setters and `AppendData`, `InsertData`,
  `DeleteData`, `ReplaceData`; `XmlAttribute::setValueProperty`;
  `XmlDeclaration::setValueProperty`; `XmlProcessingInstruction::setDataProperty`.

Contracts you can rely on:

- the `*ing` handler runs **before** the mutation and the `*ed` handler **after**;
- `sender` is the owning `XmlDocument`;
- a handler that throws from the `*ing` half leaves the tree **untouched**; one that throws
  from the `*ed` half leaves it **fully mutated** — neither leaves it half-changed;
- a handler may mutate the tree during dispatch, and may reassign its own handler field
  during dispatch (the dispatcher copies the callable before invoking it);
- with **no** handler installed, nothing is allocated and nothing is called.

### The one door that deliberately stays silent

`XmlNode::RemoveAllChildren` — reached through `RemoveAll()` — raises **nothing**. Unlike
`RemoveChild`, which detaches, it destroys its children, so a `NodeRemoved` handler's
`XmlNode*` would name freed storage. That is a borrowed-pointer hazard this change refuses to
introduce in order to complete an event pair. The silence is deliberate, documented on
`XmlDocument` itself, and pinned by a test; completing the pair safely is tracked separately.

## 4. What did **not** change

- No public type, member signature, object layout, vtable or exception specification.
- Valid input at every repaired door: valid names (including `ns:local`, `a.b`, `a-b`, `a_b`
  and non-ASCII), valid fragments, correct children, `null` reference nodes, `Close()` called
  twice, `ToString()` after `Close()`, and a reader driven to `EndOfFile`.
- The parser's acceptance policy. `System::Xml` still does **not** expand internal entities
  and still bounds nesting depth; no DTD, entity, URI or resolver behaviour was widened.
- `modules/xml-linq`, which consumes every repaired writer door through `WriteTo`/`Save`:
  its full suite stays green, unchanged.

## 5. Known remaining strictness gaps

Recorded rather than silently left open, each with its own ticket: `RemoveAllChildren`'s
event pair is still absent for the lifetime reason in §3 (**#2086**); characters outside the
XML 1.0 `Char` production other than NUL are still emitted raw, pending the
`CheckCharacters` decision (**#2349**); and a DOCTYPE internal subset is still lost on
read-back (**#2348**).

**Closed since (#2085, 2026-08-12) — writer content doors now reject an embedded NUL.**
Every writer body handed `std::string::c_str()` to tinyxml2, whose API is `const char*`, so
content was silently truncated at its first NUL with no diagnostic. Measured, **six** doors
did this, not the three originally recorded: `WriteString`, `WriteAttributeString` (value),
`WriteCData`, `WriteComment`, `WriteProcessingInstruction` (data) and — via `WriteString` —
`WriteElementString`, plus `WriteDocType`'s internal subset. All now throw `XmlException`
naming the door and the cause.

Rejection is not a policy choice here and is **not** governed by
`XmlWriterSettings::CheckCharacters`: the XML `Char` production excludes U+0000 and a
character reference must itself match `Char`, so no spelling carries a NUL through a
document, and this runtime's own parser already rejects one. **Content that does not contain
a NUL is completely unaffected**, byte for byte — including tab, CR, LF and multi-byte UTF-8.
Characters outside `Char` other than NUL are still emitted raw; that is **#2349**.

**Closed since (#2084, 2026-08-12) — a source-compatible narrowing worth knowing about.**
`WriteDocType` and `XmlDocument::CreateDocumentType` now validate the DOCTYPE `ExternalID`
literals instead of concatenating them raw. Calls that previously produced malformed or
silently truncated output now throw `XmlException`:

| Input | Before | After |
|---|---|---|
| `publicId` containing `"` | `<!DOCTYPE r PUBLIC "pu"b" "s">`, read back as `pu` | throws — `"` is not a `PubidChar` |
| `systemId` containing `"` (only) | read back truncated at the quote | **accepted**, re-delimited to `'…'`, full value round-trips |
| `systemId` containing both `"` and `'` | truncated | throws — unrepresentable, XML has no escape here |
| `systemId` containing `>` | terminated the declaration early | throws — RFC 3986 excludes `>` from a URI |
| `systemId` containing NUL or a control character | truncated at the NUL, losing the closing quote | throws |

**Every value that does not contain `"` keeps its exact previous output**, byte for byte: `"`
remains the preferred delimiter for that reason, and an apostrophe alone does not flip it. The
`internalSubset` is **unchanged** and still emitted verbatim — its read-back limitation is
substrate-bounded and tracked as **#2348**.
