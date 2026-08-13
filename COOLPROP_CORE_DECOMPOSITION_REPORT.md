# CoolProp → BasisThread Thermophysical Core: Decomposition & Dependency Audit

**Repository:** `basisthread-thermophysical-core`
**Branch:** `bt-core-trim`
**Upstream baseline:** `CoolProp/CoolProp` @ `e47acc2ecd70436a8051ae14f0219ef5e2d96eab`
**Audit date:** 2026-08-13
**Scope:** Read-only. No files were edited, deleted, renamed, committed, or pushed. The only working-tree change is this file.

---

## 1. Executive Summary

The repository is **40.10 MiB** on disk excluding `.git` (`.git` itself is 73.22 MiB of history, not counted below), across **1,800 tracked files**, and the working tree is currently byte-for-byte clean (no untracked/modified files prior to this report).

The single most important structural finding: **`dev/` is not disposable "developer-only" scratch space.** A CMake custom target (`generate_headers`, wired via `add_dependencies()` into *every* buildable target including the core library and `CatchTestRunner`) reads `dev/fluids/*.json`, `dev/incompressible_liquids/json/*.json`, `dev/mixtures/*.json`, `dev/cubics/*.json`, and `dev/pcsaft/*.json` at **every configure/build**, and regenerates embedded C++ headers (`all_fluids_CBOR.h` via `incbin`, `all_incompressibles_JSON.h`, `mixture_*_JSON.h`, `all_cubics_JSON.h`, `all_pcsaft_JSON.h`) that the backends `#include` directly. None of the combined artifacts (`all_fluids.cbor`, `all_fluids.json`, `all_incompressibles.json`) are committed to git — they are `.gitignore`d and rebuilt from source JSON on every clean build. **Deleting `dev/fluids/` or `dev/incompressible_liquids/json/` breaks the build, not just data regeneration.**

The second key finding: **wrappers depend on the core; the core does not depend on any wrapper.** Every one of the 21 `wrappers/*` directories is gated by its own `COOLPROP_<NAME>_MODULE` CMake option (default `OFF`) and links against the already-built static/shared `CoolProp` library or compiles core sources directly into a platform-specific target — none of them are `add_dependencies()` targets of the core library itself. This means wrapper removal is genuinely low-risk to core functionality, confirming the audit should proceed by evidence rather than by folder-name assumption (as instructed).

The third finding, relevant to BasisThread's specific scope: all four required capability domains map cleanly onto specific, separable subsystems:
- **Water** → `src/Backends/Helmholtz` (general IAPWS-95-class EOS, `dev/fluids/Water.json`) **and** `src/Backends/IF97` (fast IAPWS-IF97 industrial formulation, water-only by construction).
- **Glycols/brines** → `src/Backends/Incompressible` + `dev/incompressible_liquids/json/*.json` (127 files), including `Tfreeze(p,x)` for freezing-point-depression queries.
- **Humid air** → `src/HumidAirProp.cpp` (124 KB, largest root `.cpp`) + `src/Ice.cpp`, layered on top of the Water/Ice EOS.
- **Refrigerants** → ordinary pure-fluid entries in `dev/fluids/*.json` plus blend definitions in `dev/mixtures/predefined_mixtures.json` (R32-based blends, R410A-class blends present) — native Helmholtz mixture support, independent of the optional REFPROP interop shim.

A fifth, harder-to-resolve finding: several backends (**Cubics, PC-SAFT, REFPROP-interop, Tabular, SVDSBTL**) are **unconditionally compiled into the core library** — there is no `COOLPROP_CUBICS_MODULE`-style CMake flag gating them out. They are not required for BasisThread's stated scope, but excising them is a **C++ factory-surgery task** (touching `AbstractState.cpp` / `FactoryOptions.cpp` dispatch), not a folder deletion, and is explicitly out of scope for this audit. They are classified `UNCERTAIN` / `OPTIONAL` below with that caveat, not `REMOVE-CANDIDATE`.

**Recommended immediate action:** proceed to a second, execution-phase task (not part of this audit) that (a) prunes `wrappers/*` except Python, (b) prunes `Web/` and the CoolProp Sphinx doc source, (c) leaves `dev/fluids`, `dev/incompressible_liquids`, `dev/mixtures`, `dev/cubics`, `dev/pcsaft` untouched because they are build-critical, and (d) defers any backend-source excision (Cubics/PC-SAFT/REFPROP/Tabular/SVDSBTL) to a dedicated follow-up with its own test-coverage plan.

---

## 2. Exact Size Table

All sizes exclude `.git` (73.22 MiB / not part of the working tree). "On-disk" = `du -sb` of the working-tree directory. "Tracked" = sum of git blob object sizes for files in that path (differs from on-disk mainly due to CRLF line-ending expansion on checkout).

### 2.1 Top-level areas

| Path | Bytes | MiB | Files |
|---|---:|---:|---:|
| Repository total (excl. `.git`) | 42,052,243 | 40.10 | 1,800 |
| — of which git-tracked blob size | 41,142,276 | 39.24 | 1,800 |
| `dev/` | 21,355,060 | 20.37 | 827 |
| `Web/` | 6,869,339 | 6.55 | 278 |
| `wrappers/` | 5,077,752 | 4.84 | 316 |
| `src/` | 4,288,241 | 4.09 | 136 |
| — of which `src/Tests/` | 929,513 | 0.89 | 33 |
| `doc/` | 1,064,508 | 1.02 | 25 |
| `include/` | 833,774 | 0.80 | 93 |
| `.beads/` (issue-tracker db/export, not library content) | 876,704 | 0.84 | — |
| `docs/` (this session's own AI planning artifacts — see §5.4) | 476,451 | 0.45 | 27 |
| `externals/` | 537,915 | 0.51 | 22 |
| `.github/` | 191,803 | 0.18 | — |
| `cmake/` | 50,737 | 0.05 | — |
| Root config/docs (`CMakeLists.txt`, `*.bib`, `Doxyfile`, `*.md`, `*.txt`, `pyproject.toml`) | ≈276,000 | 0.26 | 10 |

Source-file counts (`.cpp/.h/.hpp/.c/.cc`): `src/` 131, `include/` 92, `dev/` 19, `wrappers/` 34.

### 2.2 `wrappers/` — every first-level directory

| Wrapper | Bytes | MiB | Files | In BasisThread exclusion list? |
|---|---:|---:|---:|---|
| MathCAD | 1,469,752 | 1.40 | 14 | not listed, but engineering-calc-tool niche |
| GUI | 1,315,657 | 1.25 | 51 | not listed, but standalone desktop app |
| Python | 667,373 | 0.64 | 70 | **not excluded** — matches item 14 (programmatic interface) |
| EES | 534,106 | 0.51 | 7 | yes |
| Modelica | 227,705 | 0.22 | 26 | yes |
| SMath | 181,726 | 0.17 | 23 | not listed explicitly, niche calc tool |
| VB.NET | 126,333 | 0.12 | 24 | yes |
| LibreOffice | 146,823 | 0.14 | 14 | yes |
| Excel | 122,757 | 0.12 | 5 | yes |
| Julia | 108,337 | 0.10 | 3 | yes |
| Delphi | 59,240 | 0.06 | 15 | yes |
| Lua | 25,176 | 0.02 | 8 | not listed, niche |
| Fluent | 22,781 | 0.02 | 8 | yes |
| Fortran | 12,135 | 0.01 | 11 | yes |
| MATLAB | 15,597 | 0.01 | 5 | yes |
| Rust | 8,385 | 0.01 | 8 | yes |
| Javascript | 10,551 | 0.01 | 5 | yes |
| Labview | 11,339 | 0.01 | 6 | yes |
| Mathematica | 8,343 | 0.01 | 3 | yes |
| DEB | 3,298 | 0.00 | 8 | packaging infra, not a language wrapper |
| Android | 338 | 0.00 | 2 | yes |
| **Total** | **5,077,752** | **4.84** | **316** | |
| **Total minus Python** | **4,410,379** | **4.21** | **246** | |

`Web/` (6.55 MiB, 278 files) is **not** a wrapper — it is the Sphinx documentation-website source (`conf.py`, `.rst` files, `_static/`, `_templates/`) for the public coolprop.org site. It has no build dependency relationship to the library at all (confirmed: nothing under `src/`, `include/`, or `CMakeLists.txt` references `Web/`).

### 2.3 `dev/` — major subdirectories

| Subdirectory | Bytes | MiB | Files | Build-time role |
|---|---:|---:|---:|---|
| `dev/fluids/` | 17,865,560 | 17.04 | 139 | **required** — source for `all_fluids.cbor` |
| `dev/incompressible_liquids/` (whole dir) | 1,865,092 | 1.78 | 466 | mixed — see split below |
| — of which `dev/incompressible_liquids/json/` | 462,510 | 0.44 | 127 | **required** — source for `all_incompressibles.json` |
| — remainder (`CPIncomp/` fitting code, tests, notebooks, docs) | 1,402,582 | 1.34 | 339 | dev-time only |
| `dev/mixtures/` | 334,874 | 0.32 | 18 | **required** (3 of 18 files: `mixture_binary_pairs.json`, `mixture_departure_functions.json`, `predefined_mixtures.json`, ≈264 KB) |
| `dev/cubics/` | 124,241 | 0.12 | 3 | **required** (`all_cubic_fluids.json`, `cubic_fluids_schema.json`, ≈120 KB) — feeds an unconditionally-compiled backend, see §4 |
| `dev/pcsaft/` | 90,686 | 0.09 | 3 | **required**, same caveat as Cubics |
| `dev/scripts/` | 243,164 | 0.23 | 47 | dev-time (fitting, plotting, packaging helper scripts) |
| `dev/Tickets/` | 106,168 | 0.10 | 27 | dev-time (historical bug-repro scripts, provenance) |
| `dev/TTSE/` | 125,448 | 0.12 | 10 | dev-time (tabular-backend range-tuning scripts) |
| `dev/ci/` | 76,263 | 0.07 | 10 | dev-time (preflight/CI gate scripts — process infra, not library) |
| `dev/cmake/` | 53,132 | 0.05 | 16 | **required** — `FindSharedPtr.cmake` etc., `include()`d by root `CMakeLists.txt` |
| `dev/state_capsule/` | 40,346 | 0.04 | 9 | dev-time (Cython prototyping, see §5.4) |
| `dev/pseudo-pure/` | 30,719 | 0.03 | 8 | dev-time (pseudo-pure EOS fitting) |
| `dev/fitter/` | 21,179 | 0.02 | 8 | dev-time (standalone C++ REFPROP-form fitter, legacy) |
| `dev/environmental_data_from_DTU/` | 40,214 | 0.04 | 2 | dev-time (GWP/ODP data injection source) |
| `dev/stubs/` | 24,237 | 0.02 | 8 | dev-time (Python `.pyi` stub generation/parity check) |
| `dev/pdsim_cimport_contract/` | 22,093 | 0.02 | 6 | dev-time (Cython cimport contract test for an external project, PDSim) |
| `dev/derivations/` | 22,321 | 0.02 | 2 | dev-time (symbolic-math derivation notes) |
| `dev/docker/` | 7,668 | 0.01 | 10 | dev-time (docs/build container definitions) |
| `dev/json_migration_bench/` | 11,659 | 0.01 | 5 | dev-time (historical rapidjson→nlohmann migration benchmark, now complete) |
| `dev/reference/` | 19,027 | 0.02 | 1 | dev-time (single reference script, `HX.py`) |
| `dev/codelite/` | 12,715 | 0.01 | 2 | dev-time (CodeLite IDE project files, legacy) |
| `dev/asan/` | 867 | 0.00 | 3 | dev-time (ASan Docker harness) |
| `dev/linker/` | 75 | 0.00 | 2 | **required** — symbol-hiding `.exp`/`.map` referenced by `cmake/CoolPropJSONVisibility.cmake` |
| `dev/` top-level scripts (`generate_headers.py`, `cbor_min.py`, `package_json.py`, etc.) | ≈120,000 | 0.11 | 16 | **partially required** — `generate_headers.py` + `cbor_min.py` run on every build |

---

## 3. Phase 2 — Functional Dependency Map

### A. Runtime code (compiled into the core library unconditionally)
`src/*.cpp` at root (`CoolProp.cpp`, `CoolPropLib.cpp`, `AbstractState.cpp`, `HumidAirProp.cpp`, `Helmholtz.cpp`, `PolyMath.cpp`, `DataStructures.cpp`, `Solvers.cpp`, `ODEIntegrators.cpp`, `CPfilepaths.cpp`, `MatrixMath.cpp`, `Ice.cpp`, `Configuration.cpp`, `CPnumerics.cpp`, `superancillary.cpp`, `FactoryOptions.cpp`, `CPstrings.cpp`, `CoolPropTools.cpp`, `SchemaValidation.cpp`), all of `src/Backends/**` (Helmholtz, Cubics, Tabular, REFPROP, PCSAFT, Incompressible, SVDSBTL, IF97), `src/Region/`, `src/SBTL/`, `src/SVD/`, `src/l10n/`, and all of `include/CoolProp/**`. Nothing here is conditionally compiled out by a CMake option — the `AbstractState` factory unconditionally links every backend.

### B. Build-time-only
`CMakeLists.txt`, `cmake/`, `dev/cmake/`, `dev/generate_headers.py`, `dev/cbor_min.py`, `dev/linker/*.{exp,map}`, `pyproject.toml` (Python wheel build only).

### C. Data → generated-header pipeline (the load-bearing finding)
`generate_headers.py`'s own `combine_json()` (distinct from, and newer than, the similarly-named but **stale** function in `dev/package_json.py`, which still globs the no-longer-existing `dev/IncompressibleLiquids/` path — see §6 risk) does, on every build:
1. `dev/fluids/*.json` → CBOR-encode → `dev/all_fluids.cbor` → `#include`d via `INCBIN(all_fluids_CBOR, "all_fluids.cbor")` in `src/Backends/Helmholtz/Fluids/FluidLibrary.cpp`. Cached against a per-file hash (`dev/.fluiddepcache`) so unmodified fluids aren't re-hashed every run, but a first build or any changed fluid file forces regeneration.
2. `dev/incompressible_liquids/json/*.json` → `dev/all_incompressibles.json` → packaged into `all_incompressibles_JSON.h` → `#include`d directly (as a `std::string`) in `src/Backends/Incompressible/IncompressibleLibrary.cpp`.
3. `dev/mixtures/{mixture_departure_functions,mixture_binary_pairs,predefined_mixtures}.json`, `dev/cubics/{all_cubic_fluids,cubic_fluids_schema}.json`, `dev/pcsaft/{pcsaft_fluids_schema,all_pcsaft_fluids,mixture_binary_pairs_pcsaft}.json` → each packaged 1:1 into a `*_JSON.h` header, `#include`d by the corresponding loader.

None of `all_fluids.cbor`, `all_fluids.json`, `all_incompressibles.json`, or any generated `*_JSON.h`/`*_CBOR.h` header is git-tracked (all explicitly `.gitignore`d, confirmed at lines 53–85 of `.gitignore`). **They do not exist in a fresh clone and are rebuilt from the source JSON under `dev/` every time.** There is no fallback pre-generated artifact anywhere in the tree.

### D. Scientific/reference/provenance data
`CoolPropBibTeXLibrary.bib` (155 KB, root) — the citation library. Confirmed live: every fluid JSON's `EOS[]` entries carry `BibTeX_EOS`/`BibTeX_CP0` keys (e.g. `Water.json`'s `BibTeX_EOS = "Wagner-JPCRD-2002"`) that key into this file — this is the runtime-adjacent provenance mechanism for item 12 (citations). `CITATION.bib` (root, 587 B) is the citation for the software itself. `dev/DATA_AUDIT.md` (inside `dev/incompressible_liquids/`) is a maintained provenance/audit log for the incompressible fluid data specifically.

### E. Test-only
`src/Tests/` (Catch2 C++ suite, 986 KB / 33 files), `wrappers/Python/pytest/` (104 KB), `dev/incompressible_liquids/test_*.py` (pytest sanity/regression/Chebyshev-entry tests, Python-only, not part of the C++ Catch2 run), `dev/pdsim_cimport_contract/test_pdsim_contract.py`, `dev/stubs/test_stub_parity.py`.

### F. Documentation-only
`Web/` (Sphinx site source), `doc/notebooks/` (1.05 MB of Jupyter usage-demo notebooks), `doc/transport_table/` (9 KB), `Doxyfile`/`DoxygenLayout.xml` (API-doc generation config), `docs/superpowers/` (see §5.4 — confirmed present in `upstream/master` at the baseline commit; this is upstream CoolProp's own AI-agent planning/spec archive, not fork-specific content).

### G. Packaging/release infrastructure
`.github/workflows/*` (per-wrapper CI builders: `gui_builder.yml`, `mathcad_builder.yml`, `libreoffice_builder.yml`, `javascript_builder.yml`, `python_buildwheels.yml`/`python_cibuildwheel.yml`, `windows_installer.yml`, `release_all_files.yml`, `release_get_artifact.yml`), `dev/docker/`, `dev/codelite/`, `MANIFEST.in`, `wrappers/DEB/`.

### H. Independent wrappers
All 21 `wrappers/*` subdirectories (§2.2). Each is gated by its own `COOLPROP_<NAME>_MODULE` CMake option (default `OFF`); each links against the core (static lib, shared lib, or direct source compilation into a platform target) with `add_dependencies(<wrapper-target> generate_headers)` — i.e. wrappers depend on the data-generation step like everything else, but the core library and `generate_headers` have **zero** dependency in the other direction. Several SWIG-mediated bindings (Java/C#/R/Octave/PHP — `COOLPROP_JAVA_MODULE`, `COOLPROP_CSHARP_MODULE`, `COOLPROP_R_MODULE`, `COOLPROP_OCTAVE_MODULE`, `COOLPROP_PHP_MODULE`) are driven straight from `CMakeLists.txt` + SWIG `.i` interface files without a dedicated `wrappers/<Lang>/` directory, so they don't appear in the `wrappers/` size table but exist as CMake-option surface area; all default `OFF`.

### Third-party dependency picture
- **Vendored in-repo** (`externals/`): `miniz-3.1.1` (512 KB — zip compression, used by `src/Backends/Tabular/TabularBackends.cpp` and `src/SBTL/SVDSurfaceSerializer.cpp` for compressed table/surrogate-surface serialization) and `incbin` (65 KB header-only, used to embed `all_fluids.cbor` into the binary at link time).
- **Fetched via CMake `FetchContent` at configure time** (not vendored, requires network access during a clean build): Catch2 (test target only), `nlohmann::json` (core — `include/CoolProp/detail/json.h` is a thin internal wrapper around it, not the library itself), `valijson` (JSON-schema validation, used by `SchemaValidation.cpp`), Boost headers (`boost_headers_SOURCE_DIR` — core; `boost/math/tools/toms748_solve.hpp` is used directly in `superancillary.cpp`, `AbstractState.cpp`, `HelmholtzEOSMixtureBackend.cpp`, `FlashRoutines.cpp`, and the SBTL/SVDSBTL files), and `fmt` (optional, `NO_FMTLIB` can disable it).
- `.gitmodules` is a 0-byte file — **no git submodules are in use**; everything above is either vendored or `FetchContent`-fetched.

---

## 4. Phase 3 — Engineering Capability Map

| Capability | Primary implementation | Data source | Tests | Citation/provenance |
|---|---|---|---|---|
| **Water (general)** | `src/Backends/Helmholtz/**` via `AbstractState` factory | `dev/fluids/Water.json` (also `HeavyWater.json`) — keys `EOS`, `ANCILLARIES`, `TRANSPORT`, `STATES`, `INFO` | `src/Tests/` Helmholtz-tagged cases | `BibTeX_EOS: "Wagner-JPCRD-2002"` → `CoolPropBibTeXLibrary.bib` |
| **Water (IAPWS-IF97, fast/industrial)** | `src/Backends/IF97/IF97Backend.{cpp,h}` — "the fluid this backend represents (always Water by definition)" | self-contained analytic correlation, no JSON dependency | Catch2, backend-specific | IAPWS-IF97 standard (cited in header) |
| **Ice / freezing (pure water)** | `src/Ice.cpp` (143 lines) + `include/CoolProp/Ice.h` | embedded coefficients | limited | IAPWS Ice Ih release |
| **Helmholtz (multiparameter) EOS** | `src/Backends/Helmholtz/` — `HelmholtzEOSMixtureBackend.cpp` (224 KB), `FlashRoutines.cpp` (264 KB), `VLERoutines.cpp` (152 KB), `MixtureDerivatives.cpp` (104 KB), `TransportRoutines.cpp` (68 KB), `PhaseEnvelopeRoutines.cpp` (40 KB), `MixtureParameters.cpp` (40 KB), `ReducingFunctions.cpp` (36 KB), `MeltingCaloric.cpp` (12 KB) | `dev/fluids/*.json` (139 pure fluids), `dev/mixtures/*.json` | `[SBTL]`/Helmholtz-tagged Catch2 cases | per-fluid `BibTeX_EOS`/`BibTeX_CP0` |
| **Propylene glycol / water** | `src/Backends/Incompressible/` | `dev/incompressible_liquids/json/` — confirmed present (e.g. `PG` / `MPG`-style secondary-fluid entries alongside `AEG`, `AKF`, `AL`, `AN`, `APG`, `AS10`…`AS55` and many more, 127 files total) | `dev/incompressible_liquids/test_*.py` (pytest, dev-time) | `dev/incompressible_liquids/DATA_AUDIT.md`, `README.md` |
| **Ethylene glycol / water** | same backend as above | same data directory (`MEG`/`EG`-class entries in the same 127-file set) | same | same |
| **Other incompressible heat-transfer fluids (brines, oils, etc.)** | same backend | same data directory — the full 127-fluid catalog (brines, glycols, oils, secondary refrigerants) | same | same |
| **Humid air / psychrometrics** | `src/HumidAirProp.cpp` (124 KB — largest root `.cpp`) + `include/CoolProp/HumidAirProp.h` | built on the Water/Ice EOS plus a dry-air correlation; no separate JSON data file — coefficients embedded in source | Catch2 humid-air-tagged cases | ASHRAE/Hyland-Wexler-class references cited in source comments |
| **Refrigerants (pure)** | ordinary `dev/fluids/*.json` entries — R-prefixed HFCs/HFOs/HCFCs plus ammonia, CO2, propane, isobutane, etc. among the 139 pure fluids | `dev/fluids/*.json` | Helmholtz-tagged Catch2 | per-fluid BibTeX |
| **Refrigerants (blends, e.g. R410A-class)** | `src/Backends/Helmholtz/MixtureParameters.cpp` (native mixing rules) | `dev/mixtures/predefined_mixtures.json` (35 KB, confirmed R32-based blend entries), `dev/mixtures/mixture_binary_pairs.json` (218 KB, binary interaction parameters), `dev/mixtures/inject_ASHRAE_2026.py` (ASHRAE refrigerant-designation curation script) | mixture-tagged Catch2 | `dev/mixtures/Bell2016*.txt`, `KunzWagner2012_Table*.txt` |
| **REFPROP interop (optional, external)** | `src/Backends/REFPROP/` (188 KB) — defers calculation to the proprietary NIST REFPROP library if present; not gated by a CMake option, always compiled, functionally inert without the external REFPROP install | none in-repo (calls out to REFPROP at runtime) | `[refprop]`-tagged Catch2 (run locally per `CLAUDE.md`, not CI-gated) | N/A (third-party proprietary software) |
| **Saturation calculations** | `src/Backends/Helmholtz/VLERoutines.cpp`, `PhaseEnvelopeRoutines.cpp`, ancillary curves in each fluid JSON's `ANCILLARIES` key, `include/CoolProp/superancillary/` + `src/superancillary.cpp` (fast initial-guess correlations — confirmed referenced by `Water.json`'s `EOS[0].SUPERANCILLARY` key) | fluid JSON | Catch2 | per-fluid |
| **Transport properties (viscosity, conductivity)** | `src/Backends/Helmholtz/TransportRoutines.cpp`; each fluid JSON's `TRANSPORT.viscosity` / `TRANSPORT.conductivity` blocks (confirmed present in `Water.json`); Incompressible backend has its own transport-property polynomials | fluid JSON / incompressible JSON | Catch2 | per-fluid |
| **Mixtures (general)** | `src/Backends/Helmholtz/MixtureDerivatives.cpp`, `MixtureParameters.cpp`, `ReducingFunctions.cpp` | `dev/mixtures/*.json` | Catch2 | Kunz-Wagner GERG-2008-class references (`dev/mixtures/KunzWagner2012_Table*.txt`) |
| **Cubic EOS (Peng-Robinson/SRK-class)** | `src/Backends/Cubics/` (240 KB) — **unconditionally compiled**, no CMake gate | `dev/cubics/all_cubic_fluids.json` (120 KB), `cubic_fluids_schema.json` | Catch2, cubics-tagged | schema-validated via `dev/validate_fluid_schemas.py` |
| **PC-SAFT** | `src/Backends/PCSAFT/` (181 KB) — **unconditionally compiled**, no CMake gate | `dev/pcsaft/all_pcsaft_fluids.json` (65 KB), `pcsaft_fluids_schema.json`, `mixture_binary_pairs_pcsaft.json` | Catch2, PCSAFT-tagged | — |
| **Flash calculations** | `src/Backends/Helmholtz/FlashRoutines.cpp` (264 KB, largest single Helmholtz file) | n/a (algorithmic) | Catch2 | — |
| **Phase envelopes** | `src/Backends/Helmholtz/PhaseEnvelopeRoutines.cpp` (40 KB) | n/a | Catch2 | — |
| **Freezing/melting information** | Incompressible backend: `IncompressibleFluid::Tfreeze(p, x)` (`src/Backends/Incompressible/IncompressibleFluid.cpp:377`) for glycol/brine solutions; Helmholtz backend: `MeltingCaloric.cpp` (12 KB) for pure-fluid solid-liquid coexistence curves | incompressible JSON (`T_freeze` coefficients) / fluid JSON | Catch2 | per-fluid |
| **Tabular/fast-interpolation (TTSE/bicubic)** | `src/Backends/Tabular/` (216 KB) — **unconditionally compiled**, no CMake gate; uses `miniz` for compressed table storage | generated at runtime from Helmholtz calls (not a static data file) | Catch2, tabular-tagged | — |
| **SVDSBTL (surrogate-surface backend)** | `src/Backends/SVDSBTL/` (112 KB) + `src/SBTL/` (184 KB) + `src/SVD/` (12 KB) + `src/Region/` (56 KB) + `include/CoolProp/{sbtl,svd,region}/` — **unconditionally compiled**, no CMake gate; actively under development (3 of the last 5 commits at HEAD touch this system — Chebyshev caloric backend, fitting pipeline, C++ registration). These commits are part of upstream `CoolProp/CoolProp`'s own history at the baseline commit, not local fork-only commits — see §6.8 / §6.9 for the distinction between upstream recent development and local branch divergence. | generated SVD surface tables (`COOLPROP_BUILD_SVD_TABLES` CMake option) | `[SBTL]`, `[SVDSBTL]`, `[SVDComponents]`, `[region]` Catch2 tags — this is the umbrella tag set `CLAUDE.md` explicitly calls out as required test scope for changes in this area | — |
| **Minimal programmatic interface (item 14)** | `src/CoolPropLib.cpp` (60 KB, C API — `PropsSI`, `AbstractState_*` handle-based API) + `include/CoolProp/CoolPropLib.h`; `src/CoolProp.cpp` (60 KB, C++ convenience layer) | n/a | Catch2 | — |

---

## 5. Phase 4 — Classification

### 5.1 Top-level classification table

| Area | Classification | Rationale |
|---|---|---|
| `src/*.cpp` (root) + `include/CoolProp/*.h` (root) | **KEEP** | Core infra, C/C++ API surface, HumidAir, Ice — required for every stated BasisThread capability |
| `src/Backends/Helmholtz/` | **KEEP** | Core EOS engine for water, refrigerants, pure fluids, mixtures, flash, phase envelope, transport |
| `src/Backends/IF97/` | **KEEP** | Fast water/steam formulation, explicitly water-only |
| `src/Backends/Incompressible/` | **KEEP** | Glycols, brines, freezing point — direct match to items 2, 3, 4, 11 |
| `include/CoolProp/superancillary/`, `src/superancillary.cpp` | **KEEP** | Load-bearing dependency of the Helmholtz backend's saturation solver |
| `src/Backends/Cubics/`, `src/Backends/PCSAFT/` | **UNCERTAIN** | Not needed for stated scope, but unconditionally compiled — removal requires `AbstractState`/`FactoryOptions` surgery, not a folder delete. See §6. |
| `src/Backends/REFPROP/` | **UNCERTAIN** | Interop shim to proprietary external software BasisThread will not deploy with; same unconditional-compilation caveat as above |
| `src/Backends/Tabular/`, `src/SBTL/`, `src/SVD/`, `src/Region/`, `src/Backends/SVDSBTL/`, `include/CoolProp/{sbtl,svd,region}/` | **UNCERTAIN** | Performance/surrogate-model layer, not required for baseline correctness, but is clearly a live, actively-developed investment upstream (see recent commit history at HEAD, which matches `upstream/master` exactly — §6.9) — flagged for a product decision, not classified as removable by this audit |
| `dev/fluids/` | **KEEP** | Build-critical: sole source of `all_fluids.cbor` |
| `dev/incompressible_liquids/json/` | **KEEP** | Build-critical: sole source of `all_incompressibles.json` |
| `dev/incompressible_liquids/` (remainder: `CPIncomp/`, tests, notebooks, `README.md`, `DATA_AUDIT.md`) | **KEEP-VALIDATION** | Fitting pipeline + provenance audit for the KEEP data above; needed to regenerate/extend/validate incompressible fluid data, not needed at build time |
| `dev/mixtures/`, `dev/cubics/`, `dev/pcsaft/` (the 8 build-consumed JSON files) | **KEEP** | Build-critical inputs, feed generated headers |
| `dev/mixtures/`, `dev/cubics/`, `dev/pcsaft/` (remainder: fitting/generation scripts, schema files not consumed at build) | **KEEP-VALIDATION** | Regeneration tooling and provenance for the above |
| `dev/cmake/`, `dev/linker/` | **KEEP** | `include()`d by the root `CMakeLists.txt`; build breaks without them |
| `dev/generate_headers.py`, `dev/cbor_min.py` | **KEEP** | Executed by CMake on every build |
| `dev/package_json.py` | **UNCERTAIN** | Contains a stale/likely-broken `combine_json()` referencing a directory (`dev/IncompressibleLiquids/`) that no longer exists in this tree; other functions in the same file (`inject_surface_tension_*`, `inject_ancillaries`, `inject_environmental_data`) still appear live. Needs a maintainer decision, not deletion. |
| `dev/ci/` | **KEEP-VALIDATION** | Preflight/CI gate scripts — process quality infrastructure BasisThread will likely want to keep running |
| `dev/Tickets/`, `dev/pseudo-pure/`, `dev/derivations/`, `dev/reference/`, `dev/fitter/`, `dev/environmental_data_from_DTU/`, `dev/scripts/`, `dev/TTSE/` | **KEEP-VALIDATION** | Historical provenance, data-regeneration and fitting tooling for retained scientific data |
| `dev/docker/`, `dev/codelite/`, `dev/asan/`, `dev/stubs/`, `dev/pdsim_cimport_contract/`, `dev/state_capsule/`, `dev/json_migration_bench/` | **REMOVE-CANDIDATE** | Legacy/completed engineering experiments (IDE project files, a since-completed JSON-library migration benchmark, a Cython interop-contract test for an unrelated external project, ASan container harness) — 0.114 MB total, no build/runtime coupling found |
| `CoolPropBibTeXLibrary.bib`, `CITATION.bib` | **KEEP** | Direct runtime-referenced provenance mechanism (item 12) |
| `externals/miniz-3.1.1/`, `externals/incbin/` | **THIRD-PARTY** | Vendored, in active use by Tabular/SBTL and the fluid-data embedding path respectively; retain license files with them |
| `wrappers/Python/` | **KEEP** (or **OPTIONAL** at minimum) | Explicitly matches item 14; not on BasisThread's exclusion list |
| `wrappers/{EES,Modelica,VB.NET,LibreOffice,Excel,Julia,Delphi,Fluent,Fortran,MATLAB,Rust,Javascript,Labview,Mathematica,Android}` | **REMOVE-CANDIDATE** | Explicitly named in BasisThread's exclusion list; confirmed one-directional dependency on core, zero coupling back |
| `wrappers/{MathCAD,GUI,SMath,Lua,DEB}` | **REMOVE-CANDIDATE** | Not explicitly named, but same one-directional-dependency profile as the excluded set and no plausible BasisThread use case (desktop GUI app, niche engineering-calc tools, Debian packaging) |
| `Web/` | **REMOVE-CANDIDATE** | Public documentation website source, zero coupling to library code |
| `doc/notebooks/` | **OPTIONAL** | Usage-demonstration material, valuable as onboarding reference but not required |
| `doc/transport_table/` | **KEEP-VALIDATION** | Small (9 KB), reference table content |
| `docs/superpowers/` | **KEEP-VALIDATION** | Confirmed present in `upstream/master` at the baseline commit (27 files, verified via `git ls-tree -r upstream/master`) — this is upstream CoolProp's own AI-assisted engineering-session archive (specs/plans for the rapidjson→nlohmann migration, superancillary work, SVDSBTL/Chebyshev work, etc.), not fork-specific content. Retain as upstream-provided process/provenance documentation. |
| `.github/workflows/*_builder.yml` for excluded wrappers | **REMOVE-CANDIDATE** | CI infra with no purpose once the corresponding wrapper is removed |
| `.github/workflows/{test_catch2,dev_checks,library_shared,release_*,python_*}.yml` | **KEEP** | Core build/test/release CI |

### 5.2 Note on "do not assume from folder name"
Every wrapper's classification above was derived from the CMake dependency graph (`add_dependencies()` direction, `COOLPROP_<NAME>_MODULE` gating), not from directory naming. No wrapper directory was found to be a build input to the core library, `generate_headers`, or any other wrapper. This confirms the exclusion list is safe to act on in a future execution phase without an additional dependency-tracing pass — with the caveat below.

### 5.3 REFPROP caveat
Per `CLAUDE.md`, REFPROP is installed and used locally by at least one team member, and `[refprop]`-tagged Catch2 tests are expected to run (not skip) locally. `src/Backends/REFPROP/` is unconditionally compiled core-library source, distinct from the wrapper question — it is not a candidate for the same removal path as `wrappers/EES` etc. It is classified `UNCERTAIN` above specifically because removing it changes core-library compile surface, and because the project has an existing local workflow depending on `[refprop]` tests continuing to run.

### 5.4 Note on `docs/superpowers/`
**Correction (post-publication verification):** an earlier version of this report incorrectly characterized this directory as fork-specific, local-only content. It is **not**. `git ls-tree -r upstream/master --name-only` against the baseline commit (`e47acc2ecd70436a8051ae14f0219ef5e2d96eab`) confirms all 27 files under `docs/superpowers/` are present in `upstream/master` — this directory is part of upstream `CoolProp/CoolProp` itself at the baseline commit, not something this branch or fork added. It contains AI-agent-assisted planning documents (e.g. `2026-06-04-rapidjson-to-nlohmann-phase0-scaffolding.md`, `2026-06-07-superancillary-deleak-nlohmann.md`, `2026-06-01-svdsbtl-hx-demo-notebook.md`) that reflect upstream CoolProp's own recent development process, not this team's. It is included here for completeness of the size inventory; its retention is a team-process decision independent of the CoolProp trimming question, and does not indicate local fork divergence — see §6.9.

---

## 6. Dependency Findings (Risk-Relevant Details)

1. **No pre-generated fallback artifact exists.** `all_fluids.cbor`, `all_fluids.json`, `all_incompressibles.json`, and every `*_JSON.h`/`*_CBOR.h` header are `.gitignore`d and produced fresh by `dev/generate_headers.py` on every build. A trimming pass that removes `dev/fluids/` or `dev/incompressible_liquids/json/` without first freezing a generated artifact into the tree (or changing the build to consume one) will break `cmake --build` outright, not just data regeneration.
2. **`dev/package_json.py`'s `combine_json()` appears to reference a stale path** (`dev/IncompressibleLiquids/`, capitalized, non-existent in this tree — the live directory is `dev/incompressible_liquids/json/`, lowercase). The function that actually runs during builds is a *different*, newer `combine_json()` defined directly in `dev/generate_headers.py`, which correctly targets `dev/incompressible_liquids/json/`. `package_json.py` retains other apparently-live functions (`inject_surface_tension_2014/2012`, `inject_ancillaries`, `inject_environmental_data`) that mutate `dev/fluids/*.json` in place. This script was not modified or fixed as part of this audit (out of scope) but should be flagged to whoever owns fluid-data curation.
3. **Five backends have no CMake off-switch.** Cubics, PC-SAFT, REFPROP-interop, Tabular, and SVDSBTL/SBTL/SVD/Region are compiled into the core library unconditionally. There is no `-DCOOLPROP_CUBICS_MODULE=OFF`-style flag to test whether the codebase tolerates their absence. Any future removal requires source changes to `AbstractState`'s backend factory and `FactoryOptions.cpp`, plus a corresponding CMake source-list edit, plus running the currently-passing test suite to confirm nothing else silently depended on them (e.g. Tabular backend building on top of Helmholtz results is a plausible hidden dependency — not confirmed or refuted by this audit).
4. **`docs/superpowers/` confirms SVDSBTL is an active, non-trivial, in-flight investment** (multiple recent commits, dedicated design docs) — but this is investment by whoever maintains `upstream/master` (the design docs are part of the upstream baseline itself, §5.4/§6.9), not evidence of local, fork-only activity on this branch. Treating `src/Backends/SVDSBTL/` + its support code as a routine "optional performance backend" to prune would very likely conflict with ongoing (upstream) work; this audit does not recommend that path without an explicit decision from whoever owns that effort — which may mean coordinating with upstream, not just this team.
5. **Third-party dependencies are fetched over the network at configure time** (Catch2, `nlohmann::json`, `valijson`, Boost headers, optionally `fmt`) via CMake `FetchContent` — none are vendored except `miniz` and `incbin`. A trimmed/offline BasisThread build environment needs to account for this (vendor them, or guarantee network access during CI/build).
6. **Wrapper CI workflows are wrapper-specific** (`gui_builder.yml`, `mathcad_builder.yml`, `libreoffice_builder.yml`, `javascript_builder.yml`, `java_builder.yml`, `python_buildwheels.yml`/`python_cibuildwheel.yml`, `windows_installer.yml`). Removing a wrapper without removing its workflow file leaves a CI job that will fail on missing sources; removing the workflow file without removing the wrapper silently drops CI coverage. These need to be trimmed together, not independently.
7. **Licensing/provenance obligations travel with `externals/`.** `miniz-3.1.1` and `incbin` each carry their own license terms; if the trimmed core retains the Tabular/SBTL backends (which use `miniz`) or the fluid-data embedding path (which uses `incbin`), both vendored directories and their license files must be retained intact.
8. **Future upstream merges.** Verified via `git rev-parse HEAD` / `git rev-parse upstream/master`: local `HEAD` and `upstream/master` currently resolve to the **identical commit** (`e47acc2ecd70436a8051ae14f0219ef5e2d96eab`), and `git log --left-right --count upstream/master...HEAD` reports zero commits on either side — **this branch has not locally diverged from upstream at all as of this audit.** SVDSBTL, the Chebyshev caloric backend, the rapidjson→nlohmann migration, and JSON-symbol-visibility hardening are recent **upstream** development already present at the baseline commit, not local fork-specific work (see §6.9). Any wrapper/doc/dev-tooling removal this team performs going forward would be the first local divergence from upstream, and narrows the surface area that a future `git merge`/`git cherry-pick` from upstream would touch (generally reduces merge-conflict risk). Backend-level removal (Cubics/PCSAFT/REFPROP/Tabular) would diverge more sharply from upstream's `AbstractState` factory shape and make future upstream Helmholtz/flash-routine merges more conflict-prone. This is a real trade-off to weigh, not just a risk to mitigate — but it is a trade-off about *future* local changes, not a correction of *existing* divergence.

9. **Provenance correction and verification record.** An earlier version of this report stated that this branch "has already diverged from upstream `CoolProp/CoolProp` with fork-specific work" and that `docs/superpowers/` was "not part of upstream CoolProp" / "this fork's own" content. Both statements were incorrect and have been corrected in §5.4, §4 (SVDSBTL row), and §5.1 (`docs/superpowers/` row) above. Verification performed:
   - `git rev-parse HEAD` → `e47acc2ecd70436a8051ae14f0219ef5e2d96eab`
   - `git rev-parse upstream/master` → `e47acc2ecd70436a8051ae14f0219ef5e2d96eab` (identical to HEAD)
   - `git log --left-right --count upstream/master...HEAD` → no output (empty symmetric difference; 0 ahead, 0 behind)
   - `git ls-tree -r upstream/master --name-only | grep '^docs/superpowers'` → 27 files, confirming `docs/superpowers/` is present in the upstream tree at the baseline commit
   - `git status` → clean except the untracked report file itself
   The correct statement is: **as of this audit, `bt-core-trim` is not merely "based on" upstream — it points at the exact same commit as `upstream/master`.** There is no local fork divergence to describe yet. SVDSBTL and the other recently-touched systems are upstream CoolProp's own current development, inherited by this branch because this branch has not yet made any commits of its own.

---

## 7. Phase 5 — Proposed Trimmed Repository Structure & Size Estimates

These are **repository working-tree content estimates** (what to keep in the git tree), not compiled-binary sizes. All backend source directories flagged `UNCERTAIN` in §5 (Cubics, PC-SAFT, REFPROP, Tabular, SVDSBTL/SBTL/SVD/Region) are **retained in every tier below**, because their removal requires code changes this audit did not make and explicitly was not authorized to make. Where a tier description says "minimal runtime core," it means "the smallest set of *directories*, given the current, unmodified factory wiring" — not a hypothetical post-refactor size.

| Tier | Contents (incremental) | Cumulative size |
|---|---|---:|
| **1. Minimal runtime core** | `src/` minus `src/Tests/` (3.20 MiB) + `include/` (0.80 MiB) + `externals/` (0.51 MiB) + build-critical `dev/` data: `dev/fluids/` (17.04 MiB) + `dev/incompressible_liquids/json/` (0.44 MiB) + build-consumed subset of `dev/mixtures/`+`dev/cubics/`+`dev/pcsaft/` (≈0.46 MiB) | **≈22.45 MiB** |
| **2. + required build files** | `CMakeLists.txt` (0.11 MiB) + `cmake/` (0.05 MiB) + `dev/cmake/` (0.05 MiB) + `dev/generate_headers.py`+`dev/cbor_min.py` (≈0.03 MiB) + `dev/linker/` (~0 MiB) | **≈22.69 MiB** |
| **3. + engineering tests** | `src/Tests/` (0.89 MiB) | **≈23.58 MiB** |
| **4. + scientific/reference/regeneration material** *(likely BasisThread target)* | `CoolPropBibTeXLibrary.bib`+`CITATION.bib` (0.15 MiB) + `dev/ci/` (0.08 MiB) + remainder of `dev/incompressible_liquids/`, `dev/mixtures/`, `dev/cubics/`, `dev/pcsaft/` (≈1.41 MiB) + `dev/{pseudo-pure,derivations,reference,fitter,environmental_data_from_DTU,Tickets,scripts}/` (≈0.47 MiB) | **≈25.68 MiB** |
| **5. Everything else (deletion candidates)** | `wrappers/*` minus Python (4.21 MiB) + `Web/` (6.55 MiB) + `doc/notebooks/` (1.05 MiB) + legacy `dev/` dev-ops experiments: `docker/codelite/asan/stubs/pdsim_cimport_contract/state_capsule/json_migration_bench` (0.11 MiB) + wrapper-specific CI workflow files (small) | **≈11.9 MiB** (remaining ≈2.5 MiB is `wrappers/Python`, `.beads/`, `.github/` core workflows, `.claude/`, `.semgrep/`, `docs/superpowers/`, `doc/transport_table/`, and root config files, none proposed for deletion) |

**Net effect if Tier 4 is adopted and Tier 5 fully removed:** repository shrinks from 40.10 MiB to roughly **25.7 MiB** of retained content (≈36% reduction), with `dev/fluids/` (17.04 MiB) remaining by far the largest component — it cannot shrink further without dropping pure-fluid coverage BasisThread might need, and it is not "bloat," it is the scientific database.

If, in a later phase, the team decides to also excise the `UNCERTAIN` backends (Cubics/PC-SAFT/REFPROP/Tabular/SVDSBTL) via actual code changes, the additional removable *source* is small in absolute terms — `src/Backends/{Cubics,PCSAFT,REFPROP,Tabular,SVDSBTL}` totals **937 KB**, plus `src/SBTL`+`src/SVD`+`src/Region` (**252 KB**) and their `dev/{cubics,pcsaft}` data (**≈0.21 MiB**) — under 1.4 MiB combined. **This is not where the repository's size lives**; the case for removing those backends, if made, should rest on reducing AbstractState surface area and maintenance burden, not on disk footprint.

---

## 8. Phase 6 — Risks and Unresolved Questions

**Risks (see §6 for full detail; summarized here against the specific Phase 6 checklist):**
- Generated headers/data whose source lives under `dev/`: **confirmed and detailed in §6.1** — highest-severity finding of this audit.
- Hidden wrapper dependencies: none found; dependency direction is uniformly wrapper→core.
- Loss of upstream build/test coverage: removing wrapper CI workflows drops upstream-style coverage for those platforms — acceptable given BasisThread's scope, but should be a deliberate decision, not a side effect.
- Licensing notices: `miniz`/`incbin` vendored-license obligations travel with any retained code that uses them (§6.7).
- Scientific citations: mechanism identified and confirmed live (§3.D, §4); retaining `CoolPropBibTeXLibrary.bib` is necessary, not optional, if PropsSI-style citation lookups matter to BasisThread.
- Regression-test coverage: `src/Tests/` tag scoping (`[SBTL]`/`[SVDSBTL]`/`[SVDComponents]`/`[region]` etc., per `CLAUDE.md` and `dev/ci/preflight.sh`) implies some tests are backend-specific; a future backend-removal pass must first inventory which tags would be orphaned.
- Licensing/citations: covered above.
- Future upstream merges: trade-off noted in §6.8 — wrapper/doc trimming reduces merge friction, backend trimming increases it.
- Generated source files: `all_fluids_CBOR.h`, `all_incompressibles_JSON.h`, `mixture_*_JSON.h`, `all_cubics_JSON.h`, `all_pcsaft_JSON.h` — all `.gitignore`d, all regenerated from `dev/` data on every build (§6.1).
- Scripts required to update fluid models: `dev/generate_headers.py` (live, required), `dev/package_json.py` (partially stale, §6.2), `dev/incompressible_liquids/all_incompressibles.py` + `CPIncomp/` (live fitting pipeline).
- Features that appear unused but support another retained capability: `include/CoolProp/superancillary/` initially looks like a standalone/optional module but is a direct dependency of the Water EOS's saturation solver (`EOS[0].SUPERANCILLARY` field, confirmed) — **do not remove**. `src/Backends/Tabular/` and `src/SBTL/` both depend on the vendored `miniz` — removing one without checking the other would silently break compression for whichever remains.

**Unresolved questions requiring a decision from the BasisThread/CoolProp team (not resolvable by static audit alone):**
1. Should `src/Backends/{Cubics,PCSAFT,REFPROP,Tabular}` be excised via code changes in a follow-up task, or retained indefinitely as unconditionally-compiled but functionally-unused surface area? (§5, §7 — small size impact, real maintenance-burden impact.)
2. Is `src/Backends/SVDSBTL` (and its `SBTL`/`SVD`/`Region` support code) in scope for BasisThread at all, given it is clearly a current, active investment on this branch? This audit takes no position and did not attempt to answer it.
3. Does BasisThread want the Python wrapper (`wrappers/Python/`) retained as a first-class deliverable, or only the C API (`CoolPropLib.h`) with Python bindings to be built separately/later? Affects whether Tier 4/5 boundary should move `wrappers/Python/` between OPTIONAL and KEEP.
4. Is `dev/package_json.py`'s stale `combine_json()` (§6.2) worth fixing/removing, or is it simply dead code nobody has hit because `generate_headers.py`'s own newer implementation shadows it in the actual build path?
5. Does the team want a committed, pre-generated fallback for `all_fluids.cbor`/`all_incompressibles.json` (to decouple "can this repo build" from "is `dev/fluids/` present and correct"), or is requiring `dev/fluids/` + a Python interpreter at every build acceptable long-term?

---

## 9. Recommended Next Action

Do **not** proceed directly to deletion. The recommended next step is a scoped, separately-authorized execution task that:
1. Removes the confirmed `REMOVE-CANDIDATE` wrappers (§5.1) together with their corresponding `.github/workflows/*_builder.yml` files, in one commit, verified by a full `./dev/ci/preflight.sh` pass afterward (the core library and its tests have zero dependency on any wrapper, so this should be a clean, low-risk removal).
2. Removes `Web/` and evaluates `doc/notebooks/` retention with whoever owns onboarding/documentation.
3. Removes the seven `REMOVE-CANDIDATE` legacy `dev/` subdirectories (§5.1) after a final confirmation that `dev/json_migration_bench/` and `dev/pdsim_cimport_contract/` are indeed unreferenced by any current CI job (this audit found no reference, but their names suggest completed/external-project work worth a second look before deletion).
4. Explicitly does **not** touch `dev/fluids/`, `dev/incompressible_liquids/`, `dev/mixtures/`, `dev/cubics/`, `dev/pcsaft/`, or any `src/Backends/*` directory, pending the unresolved questions in §8.
5. Leaves the `UNCERTAIN` backend-removal question (Cubics/PC-SAFT/REFPROP/Tabular/SVDSBTL) as a distinct, later-scoped task with its own test-coverage and factory-refactor plan — this is a code change, not a trim.

---

*End of audit. No source files were modified, deleted, renamed, committed, or pushed during this analysis. Verification follows in the next tool call.*
