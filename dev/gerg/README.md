# GERG-2004/GERG-2008 developer tooling

Two standalone developer scripts supporting the strict GERG-2004/GERG-2008
backend (`src/Backends/GERG/`). Neither is invoked by CMakeLists.txt or CTest
— CoolProp's build has no dependency on `teqp` and never will; these are
one-off, by-hand tools you re-run after touching the GERG tables or after a
`teqp` upgrade.

## `verify_transcription.py` — table-family cross-check (Tasks 1-6)

Mechanically re-verifies all four coefficient-table families
(pure-fluid residual coefficients, ideal-gas coefficients, binary
reducing parameters, departure functions/F_ij) transcribed into
`src/Backends/GERG/GERGData.h` / `GERGBackend.cpp` against a local `teqp`
checkout's `include/teqp/models/GERG/GERG.hpp`. It parses both sources with
regexes (no build/link against teqp — that would pull Eigen/Boost/autodiff
into a lookup-table sanity check) and diffs every extracted value.

```bash
python3 dev/gerg/verify_transcription.py                      # ~/Code/teqp by default
python3 dev/gerg/verify_transcription.py --teqp-root /path/to/teqp
```

Exits 0 and prints "OK" for each of the four families on success; exits 1
with a diff-shaped mismatch report otherwise. Run this after editing any
table in `GERGData.h`/`GERGBackend.cpp` — a typo fix, a new fluid, a value
correction.

## `generate_reference_values.py` — EOS-level reference values (Task 7)

Generates `src/Backends/GERG/GERGReferenceValues.h`, the fixture Tasks 8-9
validate CoolProp's *assembled* GERG equation of state against. This is a
different, stronger claim than `verify_transcription.py`: that script only
proves CoolProp's *tables* match teqp's tables. This one proves CoolProp's
*evaluated EOS* (residual/ideal Helmholtz derivatives, pressure, c_v, speed
of sound) reproduces teqp's numbers at specific (T, rho, z) points.

### Why the header is committed, not built from teqp

CoolProp's build must not depend on teqp (a research-code Eigen/autodiff/
Boost-heavy C++ library) just to run its own test suite. So the reference
values are generated once, on a developer machine with a teqp venv, and the
resulting `GERGReferenceValues.h` is committed like any other source file.
Regenerating it (same teqp version, same script) is expected to reproduce a
byte-identical file unless teqp's own GERG implementation changes — if a
regeneration attempt produces a diff, that is itself worth investigating
before assuming the new values are "more correct."

### Environment (teqp 0.23.2)

The system `python3`'s teqp (0.15.3 as of this writing) predates the GERG
factory registration and fails with `Unknown kind:GERG2008resid`. Use an
isolated venv:

```bash
python3 -m venv /tmp/gergenv
/tmp/gergenv/bin/pip install "teqp>=0.18" numpy
/tmp/gergenv/bin/python -c "import teqp; print(teqp.__version__)"   # sanity check
```

### Regenerating the header

```bash
/tmp/gergenv/bin/python dev/gerg/generate_reference_values.py \
    > src/Backends/GERG/GERGReferenceValues.h
uvx clang-format@18.1.8 -i src/Backends/GERG/GERGReferenceValues.h
```

Run both commands with a CWD inside this repo, writing directly to
`src/Backends/GERG/GERGReferenceValues.h` (or another path under the repo
tree) — not to a scratch directory elsewhere on disk. clang-format finds
its style by walking UP from the target file looking for `.clang-format`;
outside the repo tree it silently falls back to a different default style
(e.g. Allman brace-wrapping becomes attached braces) with no warning, which
looks like nondeterminism but is really "formatted against the wrong
config." Verified: regenerating+reformatting in place under the repo
reproduces the committed header byte-for-byte; doing the same in `/tmp`
does not.

The clang-format pass is required, not cosmetic: the pre-PR gate runs
`clang-format --dry-run -Werror` against every changed `.h` file, and the
generator's own 4-space, one-point-per-line emission does not satisfy it.
Applying clang-format re-indents everything to 2 spaces and, for the AGA8
mixture rows (21-element `names`/`z` vectors per row), explodes each row
across roughly 20 lines — one element per line — to respect the column
limit. **This looks bad** (a single logical mixture-composition record is
no longer one line you can eyeball), but every value is complete and
unchanged, the file still compiles, and there is no `// clang-format off`
precedent anywhere in this repository to reach for instead. This was
flagged as an expected risk before generation, and it is exactly what
happened: the honest trade-off is "correct but visually noisy" over adding
a suppression convention that doesn't otherwise exist in this codebase.

The generator prints a coverage/cross-check summary to stderr (also
embedded verbatim, as `//`-prefixed lines, near the top of the generated
header) — check it after every regeneration:

```
pure_points_2004: 288 emitted (18 fluids x 16 grid pts, floor 14/16 finite alphar enforced per fluid), NaN'd-field counts: alphar=0, alphaig=0, p=0, cv=0, w=23
pure_points_2008: 336 emitted (21 fluids x 16 grid pts, floor 14/16 finite alphar enforced per fluid), NaN'd-field counts: alphar=0, alphaig=0, p=0, cv=0, w=27
mix_points_2004 (binary pairs only): 153 of 153 expected emitted (assert enforced), 19 with w = NaN
mix_points_2008 (binary pairs + AGA8): 397 emitted (210 of 210 expected pairs + 187 of 187 expected AGA8 gases, both counts assert-enforced), 40 pairs with w = NaN, 0 AGA8 with w = NaN
Mixture alphar/alphaig: 0 NaN on every emitted row of both vectors (assert-enforced). w is the ONLY mixture column that is ever NaN, so Task 9's per-field skip may fire on w and on nothing else.
AGA8 p_teqp vs validation_data.P_MPa: worst 6.770e-04 (gas 193), median 3.514e-12 ...
```

### Coverage is enforced, not just reported

`main()` asserts, and aborts with a descriptive message rather than
emitting a short header, if any of the following don't hold exactly:

- `len(mix_points_2004)` (binary pairs) `== 153` (`C(18,2)`)
- binary pairs in `mix_points_2008 == 210` (`C(21,2)`)
- AGA8 rows emitted `== len(VALIDATION_DATA) == 187`
- **every** emitted mixture row has a finite `alphar` *and* a finite
  `alphaig` (`nanalpha_*` / `aga8_nanalpha` must all be empty)

Why this matters more here than almost anywhere else in this backend: the
Task 5 review established that a single mistyped digit in one of the 225
binary reducing-parameter rows (153 GERG-2004 + 72 GERG-2008-override) is
guarded by **nothing** except these binary-pair reference points existing
and being numerically compared in Task 8/9. Before this assertion existed,
a future regeneration (a teqp version bump, a coefficient edit) that made
one pair evaluate to a caught exception instead of a finite-but-wrong
number would make that pair silently vanish from the header — removing the
only guard on that reducing-parameter row while every test stayed green.
Now that regeneration aborts loudly instead.

The pure-fluid grid gets a **per-fluid** floor instead of a global count:
each fluid must retain a finite `alphar` (teqp's `get_Ar00`) at least
`MIN_FINITE_ALPHAR_PER_FLUID = 14` of its 16 grid points. This is
per-fluid, not "at least N out of 288/336 total," specifically so one
fluid failing broadly cannot hide behind other fluids that have full
coverage — a global total can stay comfortably above threshold even if one
fluid degrades to near-zero usable points. In practice every fluid
currently has all 16/16 finite (see the next section for why alphar itself
essentially never goes non-finite); the floor is a tripwire for future
regressions, not a currently-binding constraint.

All three assertion classes were verified to actually fire: temporarily
forcing one binary pair, one AGA8 gas, and one fluid's `alphar` to drop out
(each on a throwaway copy of this script, never committed) independently
raised `AssertionError`, e.g.:

```
AssertionError: mix_points_2004 has 152 binary-pair rows, expected exactly 153 -- 1 pair(s)
dropped: [('methane', 'nitrogen')]. A missing pair removes the ONLY numeric guard on that
reducing-parameter row; investigate, do not ship.

AssertionError: AGA8 rows: 186 emitted from 187 validation_data entries, expected 187 of
both -- 1 gas(es) dropped: [2]

AssertionError: 2004/methane: only 0/16 grid points have a finite alphar -- this fluid is
failing broadly (not just isolated near-phase-boundary points); investigate before
shipping, do not silently ship a near-empty fluid.
```

### `validation_data.P_MPa` cross-check: why the worst-case row is 6.8e-4, not 1e-12

`_validation_data.py`/`_mixture_comps.py` are byte-for-byte transcriptions
of teqp's own `validation_data`/`mixture_comps` tables
(`~/Code/teqp/src/tests/catch_test_GERG.cxx:119-321,322-...`), verified by
diffing every row against the C++ source at generation time. All 187 AGA8
rows are generated with teqp directly (full `%.17g` double precision); the
table's own `P_MPa` column is used only as an independent cross-check, not
as the fixture.

When the mixture is built with the *correct* AGA8 component ordering (see
below), the median relative disagreement between teqp's computed pressure
and `validation_data.P_MPa` across all 187 rows is 3.5e-12 — machine-noise
level, confirming the ordering/composition wiring is right. But the *worst*
row (gas 193) disagrees by 6.8e-4 (41/187 rows exceed 1e-6). This is not a
composition/ordering bug:

- Deliberately rebuilding the model with the *wrong* ordering
  (`GERG2008::component_names` order instead of the AGA8 test-code order)
  changes the picture completely: median jumps to 7.1e-4, the worst row
  to 0.27 (27%), and only 7/187 rows stay below 1e-9 (down from 104/187).
  A real ordering bug produces that kind of uniform, everywhere-large
  disagreement — not a smooth distribution where most rows are at
  machine precision and a handful of unusual mixtures are worse.
- The worst-offending rows are consistently rich, atypical natural-gas
  blends far from GERG-2008's typical calibration composition — e.g. gas
  193 is 64.5% CO2, 21% methane, 7.7% N2, 5.1% H2S, 0.6% helium. GERG-2008
  is documented to have larger uncertainty for such acid-gas/CO2-dominant,
  multi-component blends than for typical pipeline-quality natural gas.
- teqp's own test (`catch_test_GERG.cxx:696-698`) computes this exact
  comparison and then *comments out* the `CHECK_THAT` against
  `validation_data[i].P_MPa` entirely, at a 1e-5 tolerance — i.e. teqp's own
  authors already know this comparison doesn't reliably pass and disabled
  it, rather than it being an oversight in this script.
- Independent confirmation from the Task 7 review: NIST's own bundled AGA8
  reference implementation, re-run directly (not through teqp) against the
  same gas-193 state, *also* disagrees with the published table value.
  That upgrades the conclusion from "teqp disagrees with the table" to
  "the reference C/FORTRAN code disagrees with its own published table" —
  i.e. this is a table-vs-reference-code discrepancy in the published AGA8
  validation data itself, not anything specific to teqp or to this
  generator.

Conclusion: `validation_data.P_MPa` is real published AGA8 measurement/
reference data that neither GERG-2008 (via teqp) nor NIST's own bundled
AGA8 reference code reproduces exactly for its most unusual compositions —
a property of the published reference table, not a transcription or
ordering bug anywhere in this pipeline. The reference values Tasks 8/9
validate against come from teqp directly, not from this column, so this
discrepancy does not propagate into the fixture.

### AGA8 component ordering (load-bearing, do not "simplify")

The columns of `mixture_comps` are **not** in `GERG2008::component_names`
order. They follow the AGA8 test-code ordering declared separately as
`components` (`catch_test_GERG.cxx:512`):

```
methane, nitrogen, carbondioxide, ethane, propane, isobutane, n-butane,
isopentane, n-pentane, n-hexane, n-heptane, n-octane, n-nonane, n-decane,
hydrogen, oxygen, carbonmonoxide, water, hydrogensulfide, helium, argon
```

Note isobutane precedes n-butane, isopentane precedes n-pentane, and
helium/argon are last — all three differ from `component_names` order.
Every `MixRefPoint` emitted by this generator carries its component
**names** alongside the mole fractions (not a bare `std::vector<double>`)
specifically so this ordering is self-describing in the fixture itself and
Task 9 cannot reintroduce this bug by re-deriving "which index is which
component" from a convention that lives only in a comment.

### Derivative-accessor naming (load-bearing)

teqp's Python accessors are `get_Ar<TAU order><DELTA order>` — e.g.
`get_Ar20` is the tau-tau second derivative, `get_Ar02` is delta-delta. An
earlier draft of this generator had these reversed, which still produced a
finite, plausible-looking number. Verified empirically: on the ideal-gas
model, `get_Ar20` gives `-cv_ideal/R` (3.303 for methane at 300 K, correct),
while `get_Ar02` gives exactly `-1.0` (a tell, not a real c_v). The p/c_v/w
expressions in `point()` mirror teqp's own test
(`catch_test_GERG.cxx:675-694`) exactly rather than being re-derived; any
future disagreement between this script and that test is a bug in this
script, not in teqp.

### Struct shapes consumed by Tasks 8/9

```cpp
namespace CoolProp { namespace GERG { namespace reference {

struct PureRefPoint {
    const char* name;                                       // GERG component name
    double T_K, rhomolar, alphar, alphaig, p_Pa, cvmolar, w; // ANY field may independently be NaN
};

struct MixRefPoint {
    std::vector<const char*> names;  // parallel to z -- component order is AGA8 order for
    std::vector<double> z;           // AGA8 rows, GERGData.h component_names() order for pair rows
    double T_K, rhomolar, alphar, alphaig, p_Pa, cvmolar, w;
    // w may independently be NaN (see below).  alphar/alphaig/p_Pa/cvmolar are never NaN --
    // for alphar/alphaig that is now assert-enforced at generation time, because those two
    // columns are the ones Task 9's gate depends on and a NaN in them would be silently
    // SKIPPED by the per-field comparison rather than failing.  (See the pure-fluid note
    // below for why the same guarantee is not claimed for PureRefPoint in general.)
};

extern const std::vector<PureRefPoint> pure_points_2004, pure_points_2008;
extern const std::vector<MixRefPoint> mix_points_2004, mix_points_2008;

}}}  // namespace CoolProp::GERG::reference
```

**Why `MixRefPoint` carries `alphar`/`alphaig` (added in Task 9).** The
struct originally had only `p_Pa`, `cvmolar` and `w`. Task 8's mutation
ledger established that `alphaig` is the *only* instrument in this suite
that can see an error in the ideal-gas integration constants below the
`h = s = 0` reference-state test's `1e-8` absolute floor: a `1e-9` additive
shift in `n0[1]` fails **624 `alphaig` assertions and nothing else** —
`alphar`, `p_Pa`, `cvmolar` and `w` all stay clean. Task 9's central hazard
(the mixture branch of `calc_alpha0_deriv_nocache` calling `set_Tred(Tr)`
while passing `tau_i = Tc_i/T`) is an ideal-gas error that *only* mixtures
expose, so without a mixture `alphaig` column the Task 9 gate could not see
the very bug it was written to catch. Re-running that same `n0[1] += 1e-9`
mutation with the column in place now fails **1174** assertions, every one
of them `alphaig`: 624 pure + 550 mixture. `alphar` was added alongside it
for symmetry with `PureRefPoint` and because it isolates the departure
function from the gas constant — a wrong `R` moves `p`, `cvmolar` and `w`
but leaves `alphar` exact.

**Per-FIELD nulling, not per-row dropping (load-bearing contract for Tasks
8-9).** Every field of both structs is computed and null'd to NaN
*independently*. No row is ever dropped because one field came out
non-finite. Concretely:

- `pure_points_2004`/`pure_points_2008` contain **exactly** 16 rows per
  fluid (4 T-factors x 4 rho-factors), always — nothing is ever omitted
  from the grid. In the currently-committed header, `alphar`, `alphaig`,
  `p_Pa`, and `cvmolar` are finite on **every single one** of the 624 pure
  rows; only `w` is ever NaN (23/288 rows in `pure_points_2004`, 27/336 in
  `pure_points_2008`), always at `T = 0.7 x Tc` combined with `rho = 0.5 x
  rhoc` or `1.5 x rhoc` — i.e. exactly where the 0.7-Tc isotherm is likely
  to cross the two-phase dome, putting that state on the
  mechanically-unstable branch where `w^2 < 0`, the same phenomenon
  described for mixtures below. An earlier version of this generator
  dropped the WHOLE row whenever ANY of the 5 fields was non-finite,
  which — since only `w` was ever actually the culprit — silently threw
  away ~50 perfectly good `p_Pa`/`cvmolar` points clustered in exactly the
  near-phase-boundary region where a wiring bug is most likely to show up.
  **Tasks 8/9 must check `std::isnan(...)` per field being compared, not
  skip the whole row when any single field is NaN.**
- `w == NaN` (`std::isnan(w)`) happens for 19/153 GERG-2004 and 40/210
  GERG-2008 binary-pair rows at the fixed (T=250K, rho=5000 mol/m^3,
  z=[0.4, 0.6]) state — that state sits on the mechanically-unstable
  (spinodal) branch of the single-phase EOS for some dissimilar-component
  pairs, where `w^2 < 0`. `p_Pa`/`cvmolar` remain finite and meaningful
  there; Task 9 must compare them unconditionally and skip only the `w`
  comparison when `std::isnan(w)` is true for that row.
