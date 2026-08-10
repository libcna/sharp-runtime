# Audit: `modules/numerics/include/System/Numerics/Vector3.hpp`

## Metadata

- Audit status: AUDITED (3-D vector subset).
- Reference: current .NET `Vector3.Normalize` delegates to unconditional
  `Vector128.Normalize`; the local direct probe returns finite zero instead.

## Assessment

The zero-length branch in `Normalize` preserves the original vector rather
than performing the .NET division. It feeds Plane creation and Matrix4x4
camera construction, turning invalid geometry into finite output. This is
SR-AUD-276.

### SR-AUD-276 — medium — Degenerate vector and plane normalization returns finite zero rather than .NET NaNs

The direct probe records finite zero components for `Vector3::Normalize({})`
and `Plane::Normalize({})`. Current .NET reaches an unconditional vector
division, so the equivalent zero components are NaN. This changes both the
direct API and dependent camera/plane construction.

## Finding references

- SR-AUD-276 — medium — guarded degenerate normalization changes public
  numeric behavior and dependent geometry results.

## Other missing assertions and diagnostics

- Test zero/subnormal/infinite/NaN normalization, index bounds, reflected
  nonunit normals, NaN component Min/Max/Clamp, and quaternion transforms.

## Final assessment

SR-AUD-276 applies.

---

## Post-audit correction — ticket #2173 / #2175 (2026-08-10)

*Appended by review #2167. The original report above is retained verbatim.*

**The finding is materially wider and far less uniform than recorded.** Measured with `volatile`
operands so nothing is constant-folded (`build-probe/2167_probe1_numerics_before.log`):

1. **The vector guard is not a zero guard.** `Vector2/3/4::Normalize` is
   `l = Length(); return l > 0 ? v/l : v`. Because `NaN > 0` is **false**, the branch also fires
   for **any NaN component** — `{NaN,0,0}` returns `{NaN,0,0}`, so the NaN is *swallowed* rather
   than propagated to the other components, and `{inf,NaN,-inf}` returns itself. It also fires for
   **any vector whose squared length underflows to zero** (components below roughly 1e-22), which
   is a perfectly normalizable direction returned unnormalized.
2. **`{-0,-0,-0}` returns `-0`,** not the "finite zero" recorded; the sign survives.
3. **`Plane::Normalize` does not share this behaviour, although the finding groups the two.** Its
   guard is `len < 1e-10f`, and `NaN < 1e-10f` is false, so it **propagates NaN to all four
   fields** — the exact opposite of `Vector3::Normalize`. It also returns `{1e-11,0,0}`
   unnormalized while normalizing `{1e-9,0,0}` to `(1,0,0)` with `D` scaled to `1e+09`. The
   sentence "the direct probe records finite zero components for `Vector3::Normalize({})` **and**
   `Plane::Normalize({})`" is true for the zero vector and false for every other special input.
4. **The module holds three thresholds for one structural question:** `> 0` on the length
   (vectors), `< 1e-10f` on the length (`Plane`), `> 1.192092896e-7f` on the **square**
   (`Quaternion::Inverse`, the only one documented as matching a .NET threshold) — while
   `Quaternion::Normalize` already divides unconditionally and cites `Quaternion.cs` for it.
5. **Dependents, measured:** `Plane::CreateFromVertices` on identical or collinear points yields a
   `(0,0,0)` normal with `D = -0`; `Matrix4x4::CreateLookAt(eye == target)` yields a **singular**
   view matrix (`M11 = M22 = M33 = 0`, `M44 = 1`), silently.
6. **Not caused by the guard, and shared with .NET:** `Normalize({FLT_MAX,FLT_MAX,FLT_MAX})`
   returns `(0,0,0)` because the squared length **overflows**. Recorded so a future repair of the
   guard is not credited with fixing this too.

**Disposition.** **Ticket #2173 (done)** states the real contract in all four doc-comments and
pins every row above with 16 `PIN_` tests, including both dependents and `Quaternion::Normalize`
as the contrasting control. The semantic change is **ticket #2175, blocked on evidence**: this
finding carries **no managed probe**, its .NET claim is the auditor's reading of the .NET source,
`/rv` is absent, and it does not cover `Plane::Normalize`'s believed already-normalized fast path.
Turning previously-accepted input from a finite answer into NaN, with no diagnostic, in geometry
code is the class `CLAUDE.md` records as approved Groups A–D of `RemainingApprovalDecisions.md`,
not the class #2148/#2163 shipped without approval.
See `docs/SystemNumericsNamespaceReviewPlan.md` §4.1–§4.3 and §6.2.

**Status:** `confirmed` → **`confirmed (design-complete)`**.
