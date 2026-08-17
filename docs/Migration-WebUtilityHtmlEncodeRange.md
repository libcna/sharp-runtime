<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `WebUtility::HtmlEncode` encodes the Latin-1 supplement (ticket #2044)

*2026-08-17.* `System::Net::WebUtility::HtmlEncode` escaped exactly five ASCII characters and
passed every non-ASCII byte through, so the **decoder understood more than the encoder could
ever emit**. It now matches `System.Net.WebUtility.HtmlEncode`.

Landed under `docs/StandingApprovals.md` SA-5. No public signature, layout, vtable or `noexcept`
change.

---

## 1. What changed

| Input | Was | Is |
|---|---|---|
| `<`, `>`, `"`, `&` | named references | **unchanged** |
| `'` | `&#39;` | **unchanged** — .NET uses the numeric form here, not `&apos;` |
| U+00A0 … U+00FF (e.g. `é`) | passed through | `&#160;` … `&#255;` — **decimal** |
| a supplementary scalar (e.g. U+1F600) | passed through | `&#128512;` — one reference for the whole scalar |
| U+0080 … U+009F | passed through | **unchanged** |
| any BMP scalar above U+00FF (e.g. `€`, `中`) | passed through | **unchanged** |
| plain ASCII | passed through | **unchanged** |

Transcribed from `WebUtility.cs:78-140`. The reference's own comment on the lower bound is worth
keeping: *"The seemingly arbitrary 160 comes from RFC"*.

**Encode-then-decode is still the identity**, asserted over six inputs including the new cases.

## 2. The premise the deferral rested on was wrong

The ticket said the escape policy *"must be decided together with `System::Text`'s #2019, or two
HTML encoders in one repository will diverge"*, and the class doc-comment said the two *"must not
be given two different escape sets"*.

**.NET has exactly two HTML encoders, with exactly two different escape sets, deliberately:**

| | Escape set | Form |
|---|---|---|
| `System.Text.Encodings.Web.HtmlEncoder` (default) | an **allow-list** — Basic Latin, everything above escaped | `&#xHH;`, **uppercase hex** |
| `System.Net.WebUtility.HtmlEncode` | the five specials, plus 160..255 and supplementary scalars | `&#NNN;`, **decimal** |

They serve different purposes: `HtmlEncoder` is defence-in-depth for untrusted output;
`WebUtility` is the older, laxer one. Each matches its own counterpart, and a repository-wide
"consistency" would match **neither**.

Both halves are pinned, in their own suites: `NetGatedBehaviourPinTests.Fix2044_ThisEncoder-
DIFFERSFromHtmlEncoderAndThatIsDotNets` and `HtmlEncoderRangeTests.Fix2019_TheDefault-
EncodersEscapeOutsideBasicLatin`, each naming the other. They are deliberately *not* combined
into one assertion: that would need a component edge between `Net` and `Text` for a comparison
two independent pins make just as well. A mutation that "makes them consistent" by switching
`WebUtility` to hex is caught.

## 3. To migrate

If your output goes into HTML, this is the fix: `é` now arrives as `&#233;` and renders
identically, and a downstream consumer that mangles Latin-1 bytes no longer can.

If you were relying on `HtmlEncode` passing bytes through unchanged, it was the wrong tool and
now says so.

## 4. Downstream, measured

Neither `cna` nor `mobile-eggbert` references `WebUtility` — **zero sites in both**. Neither
repository was modified.
