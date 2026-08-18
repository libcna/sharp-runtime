<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — the encoder fallback takes a `char32_t` (ticket #2355)

*2026-08-19.* `EncoderFallback::GetFallbackBytes` and `EncoderFallbackBuffer::Fallback` now take a
`char32_t` instead of a `char`, and `EncoderFallbackException::getCharUnknownProperty()` returns
one.

**This is a public source break** for any subclass. Landed under `docs/StandingApprovals.md` SA-2
with all five conditions discharged — §6. No layout or vtable change: the virtuals are replaced,
not added.

---

## 1. What was wrong

Every scalar an encoding cannot represent is by definition outside that encoding, and for ASCII
and Latin-1 outside `char`'s useful range too. #2017 routed those two through their configured
fallback and had to narrow the scalar to fit the signature. Measured, the narrowing was
`static_cast<char>(scalar & 0x7F)`:

| Scalar | What the fallback was handed |
|---|---|
| `U+1F600` | the byte `0x00` |
| `U+00E9` | `0x69`, the letter `i` |
| `U+20AC` | `0x2C`, a comma |

No shipped result was wrong — both shipped fallbacks ignore the argument — but a custom fallback
could not see what it was being asked about.

## 2. Why the repair is not "add .NET's second overload"

.NET's parameter is a `char` too, and it has a **second** overload taking a surrogate **pair**,
because a UTF-16 `char` cannot hold a supplementary scalar. But .NET **reassembles that pair into
a single integer** before formatting its message (`EncoderExceptionFallback.cs:53-59`) — which is
itself the evidence that the scalar is the value a caller wants.

This port has no pair to reassemble. Carrying the scalar directly needs neither the second
overload nor the second field, and reproducing them would be reproducing a limitation this port
does not have — the same argument #2299 made about `Func<void>` and #2172 about `AbsD`.

## 3. A behaviour change came with it, and it is a repair

`Encoding::ASCII()->GetBytes("\U0001F600")` produced **two** `?` and now produces **one**.

#2017 called the fallback twice for a supplementary scalar, mimicking .NET's surrogate-pair
delivery — the only shape a `char` parameter could express. Measured against the reference, .NET
delivers the pair in **one** call, and `EncoderReplacementFallback`'s pair overload sets
`_fallbackCount = _strDefault.Length`, i.e. the replacement string **once**
(`EncoderReplacementFallback.cs:117-138`).

So the doubling existed only to work around the narrow parameter, and the workaround went with the
limitation it worked around. **Two pins asserted the old answer and both stated the wrong
inference** — *"a supplementary-plane scalar produces TWO '?', matching the two UTF-16 code units
.NET would encode it from"*. The premise is right; the conclusion does not follow, because .NET
does not call the fallback once per code unit.

## 4. The exception message is now .NET's

`SR.Argument_InvalidCodePageConversionIndex` (`Strings.resx:1221`):
*"Unable to translate Unicode character \\uXXXX at index N to specified code page."* — formatted
with the scalar as an integer in at-least-four uppercase hex digits. The port's own wording is
replaced, since the signature was changing anyway.

## 5. To migrate

```cpp
// before
std::vector<bytecs> GetFallbackBytes(char unknownChar) const override;
bool Fallback(char charUnknown, intcs index) override;

// after
std::vector<bytecs> GetFallbackBytes(char32_t unknownChar) const override;
bool Fallback(char32_t charUnknown, intcs index) override;
```

**Spell `override`.** Without it the old declaration is still legal, the class silently stays
abstract, and the compiler reports it only where you try to instantiate — a diagnostic far from
the mistake. That spelling is site 2 of the negative fixture precisely because it is the one most
likely to survive a careless migration.

## 6. SA-2's five conditions

1. **Migration note** — this document.
2. **Negative consumer fixture** — `test/consumer/text_encoder_fallback_scalar_negative.cpp`,
   three sites: the `char` override on the fallback, the same on the buffer, and the silent
   non-override above. The fixture set grows to **37 fixtures / 200 sites**.
3. **Downstream ticket** — #2380.
4. **Full gate** — 17,339 run, 17,339 passed, 0 failed.
5. **Measured impact** — neither `cna` nor `mobile-eggbert` references `EncoderFallback` or
   `GetFallbackBytes`: **zero sites in both**. Neither repository was modified.

## 7. Evidence

| Mutation | Caught |
|---|---|
| The dispatch narrows the scalar again | yes (2 tests) |
| The supplementary double call comes back | yes (2 tests) |
| The exception message loses the fixed-width hex rendering | yes |
