<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `ObsoleteAttribute`'s three components are nullable (ticket #2295)

*2026-08-18.* `Message`, `DiagnosticId` and `UrlFormat` were non-nullable `std::string`, so an
**absent** value and an **empty** one were the same state. .NET's are all `string?`.

Landed under `docs/StandingApprovals.md` **SA-8** (representation) with **SA-3** (layout) and
**SA-10** (signatures), under SA-2's five conditions. `sizeof(ObsoleteAttribute)` grows
**112 → 136**, so **consumers must be recompiled**.

---

## 1. What changed

| | Was | Is |
|---|---|---|
| `getMessageProperty()` | `const std::string&` | **`const std::optional<std::string>&`** |
| `getDiagnosticIdProperty()`, `getUrlFormatProperty()` | same | same change |
| `ObsoleteAttribute(message)`, `(message, isError)` | `const std::string&` | **`std::optional<std::string>`** |
| `setDiagnosticIdProperty`, `setUrlFormatProperty` | `const std::string&` | **`std::optional<std::string>`** |
| `getIsErrorProperty()` | `bool` | **unchanged** — .NET's `IsError` is `bool`, not nullable |
| `sizeof` | 112 | **136** |

`Message` and `IsError` stay getter-only and `DiagnosticId`/`UrlFormat` keep their setters,
matching .NET's own `{ get; }` / `{ get; set; }` split.

## 2. Why a getter change alone could not have done it

The boundary was on the way **in** as well as on the way out: the message constructor and both
setters took `const std::string&`, so a caller could neither **supply** an absent value nor
**return** a component to that state. Measured before the repair, `ObsoleteAttribute def;` and
`ObsoleteAttribute empty(std::string{});` compared **equal**.

A test now asserts all three directions: absent ≠ empty on the way out, an explicitly absent
constructor argument, and a setter round-trip through value → empty → absent.

## 3. The objection the ticket raised is discharged by measurement

The ticket said: *"Zero first-party production consumers does NOT license option A: the header is
public in Core.Base and downstream consumers exist and **were not inspected**."*

They have been. Measured on 2026-08-18: `mobile-eggbert` mentions `ObsoleteAttribute` **zero**
times, and `cna` mentions it **once — inside a comment**
(`cna/modules/devices/include/Microsoft/Devices/Sensors/Accelerometer.hpp:433`), not in code.
Neither repository was modified. That is SA-2's condition 5 doing exactly the job it exists for.

## 4. To migrate

```cpp
// before
if (attr.getMessageProperty().empty()) { ... }
const std::string& m = attr.getMessageProperty();
takesAString(attr.getUrlFormatProperty());

// after
if (!attr.getMessageProperty().has_value()) { ... }          // absent
if (attr.getMessageProperty() == "") { ... }                 // present and empty — now distinct
const std::string m = attr.getMessageProperty().value_or("");
takesAString(attr.getUrlFormatProperty().value_or(""));
```

Equality against a string literal is the **one call shape that survives unchanged**, because
`std::optional` compares against a value of its own type. The negative fixture pins the three
broken shapes, a fourth that breaks *silently* (a `decltype` on the return type), and both
survivors.

## 5. Downstream, measured

See §3 — **zero code sites in either consumer**. The full-rebuild requirement is recorded here for
any future consumer.
