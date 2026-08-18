<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — a default-constructed exception carries .NET's fallback message (ticket #2323)

*2026-08-18.* `System::Exception{}.getMessageProperty()` is now
`"Exception of type 'System.Exception' was thrown."` It used to be empty.

**This changes a message downstream code may assert on.** `cna` has six such assertions — see §6.
Landed under `docs/StandingApprovals.md` SA-5. No signature, layout, vtable or `noexcept` change.

---

## 1. What changed

| Expression | Was | Is |
|---|---|---|
| `System::Exception{}.getMessageProperty()` | `""` | `"Exception of type 'System.Exception' was thrown."` |
| `System::Exception{}.what()` | `""` | the same string |
| `HttpRequestException{}` | `""` | `"Exception of type 'System.Net.Http.HttpRequestException' was thrown."` |
| `JsonException{}` | `""` | `"Exception of type 'System.Text.Json.JsonException' was thrown."` |
| `System::Exception("")` | `""` | `""` — **unchanged** |
| every other exception type's default | its own message | **unchanged** |

An **explicitly empty** message stays empty, and that is .NET too: its fallback fires on a
**null** message, and `new Exception("")` has `Message == ""`.

## 2. The reference

```csharp
public virtual string Message => _message ?? SR.Format(SR.Exception_WasThrown, GetClassName());
private string GetClassName() => GetType().ToString();
```
*(`Exception.cs:61,65`; `Exception_WasThrown` is `"Exception of type '{0}' was thrown."`,
`Strings.resx:2333`.)*

The review recorded two blockers. The first — the exact resource text — is simply readable now.
The second is real and permanent: `{0}` is `GetType()`, which is reflection this port does not
have.

## 3. What dissolves the second blocker

.NET computes the fallback **lazily** only so `_message` can stay null for serialization; the
observable is identical if the constructor just stores it — which is what a hundred subclasses in
this repository already do.

So `{0}` is resolved **statically, at each site, by the one entity that knows the answer**: the
type itself. No reflection, no new virtual, no layout change, no signature change, no `optional`.

Hard-coding the base's string and letting subclasses inherit it was the option the review
rejected, and rightly: **a message naming the wrong type is a lie, where an empty one is merely an
absence.** The subclasses that reach the base fallback are given their own.

## 4. The review's premise measurement was wrong

It recorded that *"18 subclasses supply a NON-EMPTY default message and exactly ONE type does
not — `System::Exception` itself"*, concluding the blast radius was the base constructed directly.

Re-measured across all **103** exception subclasses in the repository: **three** types reach the
base fallback, not one. `= default` on a derived exception default-constructs its base, and two
types spell it that way — `HttpRequestException` and `JsonException`. Both are given their own
message, matching what .NET produces for each.

## 5. The guard

The cost of static resolution is that a **future** subclass written as `= default` would silently
report `System.Exception`. `ExceptionFallbackMessageTests.NoOtherExceptionInheritsTheBaseFallback`
asserts that a representative set of types does not carry the base string and does not carry an
empty message, so such a subclass is caught rather than discovered downstream.

The two out-of-module rows live in `modules/net-http/tests` and `modules/text-json/tests`, because
`Core.Base` does not depend on either and **a test is not a reason to add a public component
edge** (#2354's rule). The module graph is unchanged at 41/92.

## 6. Downstream, measured — and it is not empty

Per SA-2 condition 5. `mobile-eggbert`: **zero** sites.

`cna`: **five** exception types whose default constructors chain to `System::Exception()` —
`GameUpdateRequiredException`, `GuideAlreadyVisibleException`, `GamerPrivilegeException`,
`GamerServicesNotAvailableException` and `NetworkException`, each at
`modules/gamer-services/src/Xna/<Type>.cpp` lines 7 and 25. Their default message changes from
`""` to the base string, **which names the wrong type**, and `cna` has **six** tests asserting
`EXPECT_STREQ("", ex.what())` that will fail.

`cna` may be read but not edited without a per-action instruction, so this is recorded rather
than performed: ticket **#2377**. The correct downstream repair is the same one applied here to
`HttpRequestException` and `JsonException` — give each of the six its own message naming itself,
which is what FNA's and .NET's counterparts produce.

## 7. To migrate

If you assert on a default-constructed exception's message, assert the new string, or construct
with an explicit `""` if you genuinely want an empty one. If you derive from `System::Exception`
with `= default`, supply your own default message instead:

```cpp
// before
MyException() = default;                       // now reports "System.Exception"

// after
MyException() : System::Exception("Exception of type 'My.Namespace.MyException' was thrown.") {}
```

## 8. Evidence

| Mutation | Caught |
|---|---|
| The base goes back to an empty message | ✅ |
| `HttpRequestException` inherits the base string (`= default`) | ✅ (2 tests, one of them the integration suite) |
| `JsonException` inherits the base string (`= default`) | ✅ |
