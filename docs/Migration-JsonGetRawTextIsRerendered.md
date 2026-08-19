<!-- SPDX-License-Identifier: MIT -->
# Declaration — `JsonElement::GetRawText()` re-renders and will not preserve source text (#2118)

Ticket **#2118** (SR-AUD-325, cause TJ-D), decided 2026-08-19. **It changes no production
statement**: it is a decision and its evidence, the shape of #2202, #2015 and #2324.

## The contract, and what this port does instead

.NET's `JsonElement.GetRawText()` is `_parent.GetRawValueAsString(_idx)`
(`JsonElement.cs:1196-1201`), which slices the **original document bytes** and transcodes them
(`JsonDocument.cs:700-704`). Nothing is re-rendered.

This port holds a *parsed* `nlohmann` tree. **`nlohmann`'s DOM retains no source spans at all**, so
`dump()` can only re-render from the parsed value:

| source | .NET returns | this port returns |
|---|---|---|
| `1e+01` | `1e+01` | `10.0` |
| `1.10` | `1.10` | `1.1` |
| `"a"` | `"a"` | `"a"` |
| `{ "a" : 1 }` | `{ "a" : 1 }` | `{"a":1}` |

## Why declared rather than repaired

Honouring the contract means `JsonDocument` retaining the original text **and every `JsonElement`
carrying an offset and length into it** — an object-layout change to *both* types, and a parse-time
and memory cost paid by **every** caller whether or not `GetRawText` is ever called. That was
offered and **declined**; #2117 had already grown `JsonElement` 48 → 56 in the same session, and
this would have grown it again to buy a member most callers never touch.

Note what the gate was *not*: the `/rv RE-VERIFIED ABSENT` line in the ticket's notes was stale, and
the reference **confirms** the divergence rather than resolving it. `/rv` was never this ticket's
gate; the substrate was.

## What is lost, precisely

The **representation**, never the **value**. `GetRawText` always returns valid JSON that parses back
to an equal element, and re-rendering is **idempotent** — a second pass changes nothing. A document
already in the renderer's canonical form round-trips byte for byte. A caller using `GetRawText` as a
value carrier is unaffected; only one reading it as *source text* is.

## Pins

`JsonGatedBehaviourPins.PIN2118GetRawTextReRendersRatherThanReturningSourceText` was **renamed and
re-roled** as `Decl2118_GetRawTextReRendersRatherThanReturningSourceText`: it was a *gated* pin ("a
defect knowingly still present") and is now a *declaration*. Each row additionally records what .NET
would have returned, so the gap is explicit rather than implied, and the test fails the moment the
limitation lifts. `Decl2118_TheValueSurvivesEvenThoughTheRepresentationDoesNot` pins the half that
makes the limitation tolerable.
