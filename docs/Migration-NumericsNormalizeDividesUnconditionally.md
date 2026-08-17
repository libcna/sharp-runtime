<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) Robert Vokac and contributors -->

# Migration — `Normalize` divides unconditionally, so a degenerate vector is now NaN (ticket #2175)

*2026-08-17.* `System::Numerics::Vector2`, `Vector3` and `Vector4`'s `Normalize` no longer guard
on `Length() > 0`, and `Plane::Normalize` no longer guards on `Length() < 1e-10f`. Both now match
.NET.

**This changes numeric answers from finite to `NaN` with no diagnostic**, in geometry code. Read
§1 before upgrading.

Landed under `docs/StandingApprovals.md` SA-5. No signature, layout, vtable or `noexcept` change,
and no ordinary vector's result moves by a bit.

---

## 1. What changed

| Call | Was | Is |
|---|---|---|
| `Vector3::Normalize({0,0,0})` | `{0,0,0}` | `{NaN,NaN,NaN}` |
| `Vector3::Normalize({-0,-0,-0})` | `{-0,-0,-0}` | `{NaN,NaN,NaN}` |
| `Vector3::Normalize({NaN,0,0})` | `{NaN,0,0}` — NaN swallowed | `{NaN,NaN,NaN}` — propagated |
| `Vector3::Normalize({1e-25,1e-25,1e-25})` | returned unnormalized | `{+Inf,+Inf,+Inf}` |
| `Vector2`/`Vector4` | as `Vector3` | as `Vector3` |
| `Plane::Normalize({1e-11,0,0}, 1)` | returned unchanged | `({1,0,0}, 1e+11)` |
| `Plane::Normalize({0,0,0}, 5)` | returned unchanged, `D` unscaled | `({NaN,NaN,NaN}, +Inf)` |
| `Plane::Normalize` with an overflowing normal | divided anyway | the **all-zero plane** |
| `Plane::CreateFromVertices` on identical/collinear points | unoriented plane, finite `D` | all `NaN` |
| `Matrix4x4::CreateLookAt` with `eye == target` | silently **singular** matrix | all `NaN` |
| any well-formed input | — | **bit-identical** |
| `Quaternion::Normalize` | already unconditional | unchanged |

Note the underflow row: the answer is `+Infinity`, not `NaN`, because `Length()` is `sqrt(+0)`
and a *nonzero* component divided by `+0` is an infinity. Only components that are themselves
zero give `NaN`. An underflowing vector and a zero vector — which the old guard treated
identically — now differ, exactly as they do in .NET.

## 2. Why

.NET has no guard:

```csharp
public static Vector2 Normalize(Vector2 value) => value / value.Length();       // Vector2.cs:822
public static Vector3 Normalize(Vector3 value) => value / value.Length();       // Vector3.cs:852
public static Vector4 Normalize(Vector4 vector) => vector / vector.Length();    // Vector4.cs:902
public static Quaternion Normalize(Quaternion value) =>
    (value.AsVector128() / value.Length()).AsQuaternion();                      // Quaternion.cs:380
```

`Quaternion::Normalize` in this port already did this, which is why SR-AUD-276 called it a real
finding: one member of a five-member family followed .NET and four did not.

**`Plane` really is different, but not for the reason the review recorded.** The plan said .NET's
`Plane.Normalize` carries an already-normalized epsilon fast path. It does not — that is a
pre-.NET-5 memory. What it has is DirectXMath's *overflow* mask (`Plane.cs:127-138`): divide all
four lanes unconditionally, then force every lane to zero **iff the squared length was
`+Infinity`**. The port's `< 1e-10f` threshold had no counterpart in .NET at all.

The two dependents follow from the same source: `Plane.CreateFromVertices` (`Plane.cs:84`) and
`Matrix4x4.Impl.CreateLookToLeftHanded` (`Matrix4x4.Impl.cs:365-366`) both call the unconditional
`Vector3.Normalize`, so .NET produces `NaN` there too.

## 3. Why NaN is the better answer

Returning the zero vector for a zero-length input is not a safe default — it is a wrong unit
vector. Downstream, it produces a plane with no orientation, a view matrix that is silently
singular, or a lighting term quietly stuck at zero, and none of those announce themselves. `NaN`
propagates and shows up.

That was the argument this ticket was weighed against for months, and it is the one .NET already
made.

## 4. To migrate

Test before you normalize, which is what .NET's own callers do:

```cpp
// before, relying on the guard
Vector3 dir = Vector3::Normalize(target - eye);

// after
Vector3 delta = target - eye;
Vector3 dir = delta.LengthSquared() > 0.0f ? Vector3::Normalize(delta) : Vector3{0, 0, -1};
```

Three call shapes are worth auditing specifically:

1. `Matrix4x4::CreateLookAt` where the camera may sit exactly on its target;
2. `Plane::CreateFromVertices` over a mesh that can contain degenerate triangles;
3. any normalization of a *difference* of two positions that may be equal.

`Plane::Normalize` gained one safety property rather than losing it: a normal whose squared
length overflows now returns the zero plane instead of dividing.

## 5. Downstream, measured

Per SA-2 condition 5, both consumer checkouts were searched, and the result decides the risk
question outright: **`cna` references `System::Numerics` in zero places, and so does
`mobile-eggbert`.** `cna` has 46 `Normalize` call sites, and every one of them resolves to its
own `Microsoft::Xna::Framework::Vector2/3/4` and `Plane` in `cna/modules/math/`, which this
change does not touch. Neither repository was modified.

If CNA later adopts `System::Numerics`, this note is the checklist — and CNA's own
`Vector3::Normalize` guards internally today, so the two would need reconciling as a deliberate
XNA-vs-.NET decision rather than by accident.
