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

The generator prints a coverage/skip/cross-check summary to stderr (also
embedded verbatim, as `//`-prefixed lines, near the top of the generated
header) — check it after every regeneration:

```
pure_points_2004: 265 emitted, 23 skipped (non-finite)
pure_points_2008: 309 emitted, 27 skipped (non-finite)
mix_points_2004 (binary pairs only): 153 emitted, 0 dropped, 19 with w = NaN
mix_points_2008 (binary pairs + AGA8): 397 emitted (210 pairs + 187 AGA8 gases), ...
AGA8 p_teqp vs validation_data.P_MPa: worst 6.770e-04 (gas 193), median 3.514e-12 ...
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

Conclusion: `validation_data.P_MPa` is real published AGA8 measurement/
reference data that GERG-2008 does not reproduce exactly for its most
unusual compositions — a modeling limitation, not a transcription bug. The
reference values Tasks 8/9 validate against come from teqp directly, not
from this column, so this discrepancy does not propagate into the fixture.

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
    double T_K, rhomolar, alphar, alphaig, p_Pa, cvmolar, w; // w may be NaN, never for pure fluids in practice
};

struct MixRefPoint {
    std::vector<const char*> names;  // parallel to z -- component order is AGA8 order for
    std::vector<double> z;           // AGA8 rows, GERGData.h component_names() order for pair rows
    double T_K, rhomolar, p_Pa, cvmolar, w;  // w is NaN (std::isnan(w) true) on the mechanically-
                                              // unstable branch for some binary pairs -- see the
                                              // struct's own doc comment in the generated header.
};

extern const std::vector<PureRefPoint> pure_points_2004, pure_points_2008;
extern const std::vector<MixRefPoint> mix_points_2004, mix_points_2008;

}}}  // namespace CoolProp::GERG::reference
```

`w == NaN` (`std::isnan(w)`) happens for 19/153 GERG-2004 and 40/210
GERG-2008 binary-pair rows at the fixed (T=250K, rho=5000 mol/m^3, z=[0.4,
0.6]) state — that state sits on the mechanically-unstable (spinodal)
branch of the single-phase EOS for some dissimilar-component pairs, where
`w^2 < 0`. `p_Pa`/`cvmolar` remain finite and meaningful there; Task 9 must
compare them unconditionally and skip only the `w` comparison when
`std::isnan(w)` is true for that row.
