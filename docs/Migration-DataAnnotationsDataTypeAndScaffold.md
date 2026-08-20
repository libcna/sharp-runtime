<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `DataTypeAttribute` gets .NET's enum, and the validation family is declared inert (ticket #2406)

*2026-08-20.* The last item of the `modules/component-model` sweep. Two shape repairs and **one
declaration, which is the larger half**.

**Downstream, measured:** **zero** `DataAnnotations` sites in `cna` and in `mobile-eggbert`.
Downstream record: **#2408**.

---

## 1. The declaration: this family validates nothing

`System::ComponentModel::DataAnnotations` has eleven `ValidationAttribute` subclasses — `Required`,
`Range`, `StringLength`, `MaxLength`, `MinLength`, `RegularExpression`, `EmailAddress`, `Phone`,
`Url`, `CreditCard`, `Compare` — and **not one of them validates anything**.

.NET's `ValidationAttribute` carries `IsValid(object?)` (`ValidationAttribute.cs:352`),
`Validate(...)` (`:468,497`), `FormatErrorMessage(string)` (`:330`) and `RequiresValidationContext`
(`:139`), and every subclass overrides `IsValid`. **None of those exist here.** The namespace is
metadata only.

**The names are what make this worth a `@warning` rather than a footnote.** A caller who writes
`RequiredAttribute` and sets an error message has every reason to believe something checks it, and
nothing does — there is no member to call, so the mistake surfaces as *validation that silently
never happened*. The error-message template is stored and read back faithfully and **is inert**;
a test asserts exactly that.

**Why it is a declaration and not a repair.** Implementing it is not one member:
`RegularExpressionAttribute` needs the regular-expression engine, `EmailAddressAttribute` /
`UrlAttribute` / `PhoneAttribute` need .NET's exact accepted grammars, and `Validate` needs
`ValidationContext` and `ValidationResult`, neither of which this port has. That is a feature.

The absence is **pinned**, using this repository's dependent-parameter detection idiom (the #2299
gcc trap): mutation M7 adds an `IsValid` to the family and **is caught**, so the declaration is
enforced rather than merely written down.

---

## 2. `DataTypeAttribute` was structurally wrong

```cpp
class DataTypeAttribute : public System::Attribute {
public:
    std::string DataType;                                  // public, mutable, untyped
    explicit DataTypeAttribute(const std::string& dt) : DataType(dt) {}
};
```

.NET has **two constructors with different meanings** (`DataTypeAttribute.cs:20,55`) — a known kind
from an **enum**, or a custom one named by the caller — and this port collapsed both into one
string, so *"the kind"* and *"a custom name"* were the same field and **any** string was accepted
where only seventeen values are meaningful.

**`DataType` (17 enumerators, `Custom = 0` … `Upload = 16`) is transcribed exactly** from
`DataType.cs`. The string constructor **chains to `Custom`** and stores the name **beside** the
kind, as .NET's does, so the kind really is `Custom` and the name is a separate fact.

`GetDataTypeName()` (`:83-96`) returns the enumerator's name, or the custom string for `Custom`, and
throws `InvalidOperationException` with .NET's text — *"The custom DataType string cannot be null or
empty."* — when the kind is `Custom` and no usable name was supplied (`:115-121`,
`EnsureValidDataType`). **The test is `IsNullOrWhiteSpace`, not `IsNullOrEmpty`**, which is the row
an `.empty()` reading gets wrong; mutation M4 makes exactly that mistake and is caught.

**.NET reads `Enum.GetNames<DataType>()`, which is reflection.** The substitute is an **exhaustive
`switch` with no `default:`**, so under `-Wall -Wextra -Werror` a new enumerator is a **compile
error** here rather than a silently missing name — the idiom #1980 G-5 established, and *stronger*
than a name table, which cannot pin an enum's membership.

**`DisplayFormat` is deliberately absent.** .NET's constructor sets a `DisplayFormatAttribute` for
`Date`, `Time` and `Currency` (`:26-45`) and publishes it (`:56`). `DisplayFormatAttribute` does not
exist in this port, and inventing it to hold a value nothing reads would be adding a type to have
somewhere to put it. The constructor's three-case switch exists **only** to populate it, so omitting
the switch *is* the omission of `DisplayFormat`.

## 3. `ScaffoldColumnAttribute` is get-only

`public bool Scaffold { get; }` in .NET; a public mutable field here. Same shape as #2403's six.

---

## 4. What a caller changes

| Was | Now |
|---|---|
| `DataTypeAttribute("EmailAddress")` | `DataTypeAttribute(DataType::EmailAddress)` |
| `DataTypeAttribute("MyOwnKind")` | unchanged — the string constructor still exists and now means *custom* |
| `attr.DataType` (a `std::string`) | `attr.getDataTypeProperty()` (a `DataType`), `attr.getCustomDataTypeProperty()`, or `attr.GetDataTypeName()` for the display name |
| `attr.DataType = x` | *(no replacement — .NET publishes no setter)* |
| `scaffold.Scaffold` | `scaffold.getScaffoldProperty()` |

## 5. What is *not* a divergence, checked and recorded

`RequiredAttribute::AllowEmptyStrings` and the eight fields of `DisplayAttribute` stay **public data
members**, and that is deliberate: .NET's are `{ get; set; }` auto-properties, so a public field is
**observationally identical** — #1969's recorded reasoning for the `BoundedChannelOptions` base
flags. SA-8 does not reach them, and converting them would be a source break buying no behaviour.

**What does still diverge on `DisplayAttribute` is nullability**, and it is left open rather than
half-done: .NET's five string properties are `string?`, and `Order` / `AutoGenerateField` /
`AutoGenerateFilter` are backed by `int?` / `bool?` with `GetOrder()` / `GetAutoGenerateField()`
accessors, so *"not set"* and *"set to the default"* are different states there and the same state
here — the #2295 shape. Closing it changes the signature of eight members and is recorded on the
ticket rather than attempted here.
