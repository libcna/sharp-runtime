# `System.Xml.Serialization` — scope, evidence, and known deviations

Ticket: `SAMPLES-DEC-008`. Module: `modules/xml-serialization` (`Xml.Serialization`, header-only,
`INTERFACE`, depends on `Core.Base` and `Xml`).

This module exists to unblock three `cna-samples` ports — `SAMPLE-014` (Spacewar),
`SAMPLE-066` (ShipGame) and `SAMPLE-070` (RolePlayingGame) — which the owner marked blocked on
2026-08-28 with an explicit instruction not to add another hand-written per-sample XML parser.
It is a shared, generic engine, not a parser for any one sample.

## Design

Reflection is a permanent deviation in this runtime (CLAUDE.md), so a type opts in explicitly:

```cpp
struct Entity {
    std::string name;
    Matrix transform;
    SHARP_XML_SERIALIZABLE(Entity, "Entity",
                            SHARP_XML_M(Entity, name),
                            SHARP_XML_M(Entity, transform))
};

std::string xml = XmlSerializer<EntityList>{}.Serialize(list);
EntityList back  = XmlSerializer<EntityList>{}.Deserialize(xml);
```

The macros expand to two friend functions found by ADL, which is the same
compile-time-customization-point shape `JsonSerializer` already uses through `nlohmann`'s
`to_json`/`from_json`. The difference is that no vendored library does the work underneath:
`tinyxml2` is a parser, not an object mapper, so the traversal, naming, ordering, collection
handling and text conversion are this module's own.

A registered type must live at namespace or class scope. A friend function cannot be *defined*
inside a local class, so a type declared inside a function body will not compile.

## What is implemented, and the evidence for each

| Capability | Evidence it is needed |
|---|---|
| Composite types, members in registered order | every call site |
| Value-type flattening (`Matrix` → 16 `<M11>`…`<M44>`) | `ShipGame/EntityList.cs`; fixture `level1_spawns.xml` |
| `List<T>` field → `<fieldName><ItemType>…` | `EntityList.entities`, `LightList.lights` |
| Root-level `List<T>` → `<ArrayOfItemType>` | `Session.cs`'s `new XmlSerializer(typeof(List<…>))`; fixture `SupportedUnits.xml` is a real `<ArrayOfCategoryInformation>` |
| `List<primitive>` → `<string>`, `<int>`, `<boolean>`, `<long>`, `<float>`, `<double>` | `PartySaveData.monsterKillNames` (`List<string>`) and `.monsterKillCounts` (`List<int>`) |
| Enums as member **name** | `PlayerPosition.Direction`; a numeric cast would emit `<Direction>2</Direction>` |
| Inherited members | `WorldEntry<T> : MapEntry<T> : ContentEntry<T>` |
| Generic instantiations | registered root name is an explicit string, so `WorldEntry<Chest>` → `"WorldEntryOfChest"` → `<ArrayOfWorldEntryOfChest>` |
| Missing element → member keeps default | .NET's own behaviour; rejecting instead would refuse saves written by older builds |
| Unknown element ignored | same |
| Whitespace-insensitive reading | every authentic fixture is indented |
| Floats without a leading zero (`.4`) | Spacewar's `settings.xml` ShipLights block writes exactly that, and the original game loads it |
| `INF`/`-INF`/`NaN` schema tokens, not .NET's `Infinity` spelling | XML Schema's `float` lexical space |
| Markup escaping (`&`, `<`, `>`, quotes) and non-ASCII text | a quest named `Smith & Son` must not corrupt a save |
| **Nested serialization into a caller's document** (`SerializeInto`/`DeserializeFrom`/`RootElementName`) | **16 of the 20** `Session.cs` call sites serialize into an already-open `XmlWriter` |

## The dominant call-site shape: nesting, not standalone documents

RolePlayingGame does **not** produce one document per serialized object. `Session.Save` opens a
single writer and nests:

```csharp
xmlWriter.WriteStartElement("rolePlayingGameSaveData");
xmlWriter.WriteStartElement("mapData");
xmlWriter.WriteElementString("mapContentName", TileEngine.Map.AssetName);
new XmlSerializer(typeof(PlayerPosition)).Serialize(xmlWriter, ...);
new XmlSerializer(typeof(List<WorldEntry<Chest>>)).Serialize(xmlWriter, ...);
new XmlSerializer(typeof(List<WorldEntry<FixedCombat>>)).Serialize(xmlWriter, ...);
new XmlSerializer(typeof(List<WorldEntry<Player>>)).Serialize(xmlWriter, ...);
new XmlSerializer(typeof(List<ModifiedChestEntry>)).Serialize(xmlWriter, ...);
xmlWriter.WriteEndElement();
```

Sixteen of the twenty call sites look like this; only `SaveGameDescription` and the standalone
routes write a document of their own. `SerializeInto(doc, parent, value)` and
`DeserializeFrom(element)` cover it: the nested element carries the `xsi`/`xsd` declarations
(.NET writes them on each element it creates, whatever the surrounding namespace scope) and no
XML declaration of its own. `RootElementName()` gives the caller the element name to look for
without re-deriving the `ArrayOf` rule.

`RolePlayingGameSaveTests.cpp` builds that exact document and reads it back through the same
route `Session.Load` uses.

### A C# shape that does not survive the port

`PlayerPosition`, `MapEntry<T>` and others declare `public Direction Direction`, a member named
after its own type. C++ rejects that (`-Wchanges-meaning`), so a port renames the **type** and
keeps the **member** name. The wire form is unaffected, because the element name comes from the
member. Anyone porting these types will hit the same rule; it is recorded here so it reads as a
known translation step rather than a surprise.

## Deliberately out of scope

Each of these is **absent by evidence, not by omission**:

- **`[XmlInclude]` / `xsi:type` polymorphic dispatch.** `grep -rn XmlInclude` over
  `RolePlayingGame_4_0_Win_Xbox` and `ShipGame_4_0` returns zero hits. The
  `Character → FightingCharacter → Monster` hierarchy that looked like a polymorphism problem
  belongs to the **Content Pipeline** writers (`CharacterWriter.cs`,
  `FightingCharacterWriter.cs` in `RolePlayingGameProcessors`), which build `.xnb` at build time
  and never reach `XmlSerializer`.
- **`[XmlArray]` / `[XmlArrayItem]` naming overrides.** No occurrences in the three samples.
- **`[XmlAttribute]`-mapped members.** Every reachable member is element-mapped.
- **Circular-reference detection.** The reachable save graphs are trees and lists, with content
  referenced by asset *name* rather than by object identity.

If any of these turns up in a fourth sample, it is new work with its own ticket — not a silent
gap here.

## Known deviations

### 1. Exponent case is corrected in this module, not in `XmlConvert`

XML Schema's canonical lexical form for `float`/`double` uses an uppercase `E`, .NET's
`XmlConvert.ToString` emits `E`, and the authentic fixture
`Samples/NetRumble_4_0/NetRumble/Content/Particles/rocketTrail.xml` contains
`<Duration>3.40282347E+38</Duration>`.

`System::Xml::XmlConvert::ToString(float)` produces a **lowercase** `e` (`3.4028235e+38`),
because it delegates to `System::Single::ToString`, whose lowercase form is pinned by an
existing core test — `modules/core/tests/System/DoubleTests.cpp:643` asserts
`Double::ToString(1e100, "R") == "1e+100"`, where real .NET gives `1E+100`.

**That looks like a genuine defect in `Core`/`Xml`, and it is left alone here on purpose.**
Changing it would rewrite a pinned expectation in another module and needs its own ticket and
audit entry. This module uppercases the exponent at its own boundary, where the contract is
XML's, and `XnaFixtureTests.NetRumbleFloatMax_…` pins both directions.

### 2. Significant-digit count differs from .NET Framework 4.0 — harmless

XNA ran on .NET Framework 4.0, whose `R` format wrote `3.40282347E+38` (9 significant digits).
The shortest round-trippable form, which this runtime produces, is `3.4028235E+38` (8). Both
parse to the identical `float`; the fixture test asserts that rather than assuming it.

### 3. A whitespace-only string is lost on read

Localised with `build-probe/xml_probe_whitespace_text.cpp`. The write side is correct and emits
`<Value> </Value>`; the **parser** discards it, so `XmlDocument::LoadXml` returns an element
with no children (`OuterXml == "<Value/>"`). tinyxml2 drops a text node that is entirely
whitespace. Real .NET preserves it.

The deviation lives in `modules/xml` and its vendored parser, not here — this module never sees
the text node. Its blast radius was checked rather than assumed: nothing in the three samples'
save or content routes stores an all-whitespace string, and whitespace *around* real content is
unaffected (`"  a  "` survives). `XmlLexicalFormTests.WhitespaceOnlyString_IsLostByTheParser_…`
pins the current behaviour, so a future parser change that fixes it fails there and gets
noticed.

### 4. Numeric text with a leading `+`

XML Schema allows an explicit `+` and .NET accepts it; `XmlConvert::ToSingle("+0.4")` throws
here, because it reaches `std::from_chars`, which rejects it. No authentic fixture uses the form
(grepped across Spacewar's `settings.xml`, ShipGame's level and ship content, and NetRumble's
particle effects — zero hits), so it is a conformance gap rather than a blocker. This module
strips the sign at its own boundary anyway: a reader strictly more tolerant than the writer can
only help, and a hand-edited save may carry it.

### 5. Whitespace is not part of the contract

`Samples/Spacewar_4_0/settings.xml` is indented with two spaces and
`Samples/ShipGame_4_0/.../level1_spawns.xml` with four — both genuine `XmlSerializer` output in
the same official source tree. Element names, order, text and the root namespace declarations
are the contract; indentation is not. `XmlSerializationOptions::Indent` emits tinyxml2's fixed
four-space form.

## Float round-trip: measured, not assumed

`build-probe/xml_probe_float_roundtrip.cpp` swept 47 values — a full `Matrix.Identity`, a placed
entity transform, `0.1f`, `1/3`, denormals, `epsilon`, `±INF`, `NaN`, `float`/`double` extremes —
through `XmlConvert::ToString` → `ToSingle`/`ToDouble` and compared **bit patterns**. 47/47
round-tripped exactly. This is why the module adds no float-formatting logic of its own: the
capability already existed, tested and audited, in `modules/xml`.

## Test corpus

`modules/xml-serialization/tests/System/Xml/Serialization/`:

- `XmlSerializerTests.cpp` — engine behaviour, exact-wire pins, and a negative control proving
  the exact-wire assertion fails on a swapped-member-order defect that a round-trip-only
  assertion would miss.
- `XnaFixtureTests.cpp` — two tiers. Transcribed excerpts that run everywhere, and tests that
  read the **real** shipped XNA files when the tree is present (`XNA_SAMPLES_ROOT`, default
  `/rv/tmp/XNAGameStudio/Samples`), skipping otherwise.

### Negative controls

Four defects were planted and removed, each proving a specific test can fail:

| Planted defect | Tests that failed |
|---|---|
| List items renamed to `<Item>` | 11, including every fixture test |
| Nested roots omit `xsi`/`xsd` | the save-document wire-shape test |
| `RootElementName` drops the `ArrayOf` prefix | the save/load round-trip test |
| All nested list roots share one name | all three save-route tests |

### The negative control that mattered

The fixture sweep originally asserted only that a round trip preserved the entity count. Under a
planted defect that renamed every list item, it **passed**: the fixture parsed to zero entities,
the re-serialization wrote zero, the re-parse read zero, the counts matched, and the comparison
loop never executed. The expected per-file counts now written into that test are what make a
silent-empty-parse regression fail. With the defect planted, 11 tests fail; with it removed,
26/26 pass.

## Not covered here

The `cna-samples` side — registering each game's types, removing the hand-written workarounds,
and re-qualifying `SAMPLE-014`/`066`/`070` — is separate work in that repository. This module is
the engine and the proof that the wire format is right.
