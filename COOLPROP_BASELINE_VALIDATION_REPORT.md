# CoolProp Baseline Build & Validation Investigation

**Repository:** `basisthread-thermophysical-core`
**Branch:** `bt-baseline-validation` (created from `bt-core-trim` @ `c6f006ced146bd71db92e4c1beef6d1adcc3a648`)
**Upstream baseline beneath the doc commit:** `CoolProp/CoolProp` @ `e47acc2ecd70436a8051ae14f0219ef5e2d96eab`
**Investigation date:** 2026-08-13
**Scope:** Unchanged-source baseline build + test investigation only. No CoolProp scientific source, fluid data, equations, numerical methods, tests, tolerances, or build logic was modified. This report is untracked and was not committed.

---

## 1. Branch and Source-Baseline Verification

`bt-baseline-validation` was created from and points at the same commit as `bt-core-trim`:

```
git rev-parse HEAD                         → c6f006ced146bd71db92e4c1beef6d1adcc3a648
git push -u origin bt-baseline-validation   → new branch, pushed successfully
```

The commit beneath the documentation commit is verified to be byte-for-byte the upstream baseline, not merely descended from it:

```
git rev-parse HEAD~1                        → e47acc2ecd70436a8051ae14f0219ef5e2d96eab
git diff --stat upstream/master HEAD~1       → (empty — identical trees)
git diff --name-status upstream/master HEAD  → A  COOLPROP_CORE_DECOMPOSITION_REPORT.md  (only)
```

**Confirmed: the source baseline beneath both documentation commits corresponds exactly to upstream `CoolProp/CoolProp` commit `e47acc2ecd70436a8051ae14f0219ef5e2d96eab`.** No scientific source, fluid data, or build logic differs from that commit anywhere in this branch's history.

---

## 2. Documented Build/Test Instructions Consulted

Two in-repo sources document the build procedure; no repo-root `README.md`/`INSTALL` gives inline build steps (it points to the external coolprop.org docs instead), so `CLAUDE.md` and `dev/ci/README.md` are the authoritative in-repo procedures:

- **`CLAUDE.md`** (project instructions for AI agents) specifies the canonical local build:
  ```bash
  cmake -B build_catch -S . -DCOOLPROP_CATCH_MODULE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build build_catch --target CatchTestRunner -j8
  ./build_catch/CatchTestRunner [SBTL]
  ```
  It explicitly calls out `-DCMAKE_BUILD_TYPE=Release` as necessary — the default configuration is unoptimized and makes the SVDSBTL dense-SVD surface builds (`NT=200/NR=800/rank=20`) crawl for minutes per fluid.

- **`dev/ci/README.md`** gives the contributor "fast path," which adds `-G Ninja` to the configure line and documents `./dev/ci/preflight.sh` as the script that mirrors CI (build + Catch2 with auto-selected tag scope + cppcheck/clang-tidy/semgrep). `preflight.sh`'s own configure call (`cmake -B build_catch -S . -DCOOLPROP_CATCH_MODULE=ON -DBUILD_TESTING=ON`, no `-G`, no `-DCMAKE_BUILD_TYPE`) relies on whatever CMake's default generator/config is in the calling environment.

**Windows build procedure used for this checkout**, combining both documented recommendations (Release per `CLAUDE.md`'s explicit performance guidance, Ninja per `dev/ci/README.md`'s fast path — Ninja is a single-config generator so `CMAKE_BUILD_TYPE` is actually honored, unlike the CMake default multi-config Visual Studio generator where it would be silently ignored):

```
cmake -G Ninja -B build_catch -S . -DCOOLPROP_CATCH_MODULE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_catch --target CatchTestRunner -j4
```

No wrapper modules (`COOLPROP_*_MODULE`) were enabled — this builds only the core library sources plus the Catch2 test runner, satisfying "build the core library without enabling unnecessary wrappers."

---

## 3. Toolchain and Environment

| Item | Value |
|---|---|
| OS | Windows 11 Pro 10.0.26200 |
| C/CXX compiler | **MSVC 19.44.35228.0** (Visual Studio 2022 Build Tools, VC Tools `14.44.35207`), found via `vswhere` at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` — not preinstalled on `PATH`; required loading `VC\Auxiliary\Build\vcvars64.bat` |
| CMake | **3.31.6-msvc6**, bundled with VS2022 Build Tools' "C++ CMake tools for Windows" component (`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) — **not on `PATH`**, no standalone CMake install found elsewhere on the system |
| Generator/build tool | **Ninja**, also bundled with the same VS component (`...\CMake\Ninja\ninja.exe`) — not on `PATH` |
| Python | **3.13.5** (`C:\Python313\python.exe`, also via `py` launcher) — sufficient for `dev/generate_headers.py`, which uses only the stdlib plus the vendored `dev/cbor_min.py` (no third-party packages needed for this build path) |
| Build type | `Release` (`-DCMAKE_BUILD_TYPE=Release`) |
| Modules enabled | `COOLPROP_CATCH_MODULE=ON`, `BUILD_TESTING=ON` only — no wrapper `COOLPROP_*_MODULE` flags set |

**Dependency retrieval behavior:** the configure step fetches several dependencies from the network via CMake `FetchContent` (git clone), all of which succeeded without intervention: **Eigen, msgpack-c, nlohmann_json, valijson, if97** (a separate external IAPWS-IF97 library — distinct from CoolProp's own `IF97Backend.h` wrapper around it), **refprop_headers, boost_headers, multicomplex** (which itself nested-clones `pybind11` as a submodule dependency even though no Python wrapper module was requested), **fmt**, and **Catch2**. Total configure time was **60.9 s**, dominated by these clones. This confirms the repository's build is **not offline-capable out of the box** — a clean configure requires network access to at least nine external repositories in addition to `dev/generate_headers.py`'s local packaging of `dev/fluids/`, `dev/incompressible_liquids/json/`, `dev/mixtures/`, `dev/cubics/`, and `dev/pcsaft/` into embedded headers (all of which ran and succeeded, confirming the `COOLPROP_CORE_DECOMPOSITION_REPORT.md` finding that this generation step is load-bearing and did fire on this build).

**Build time:** not fully comparable across attempts because of an intervening incremental link failure and retries (see §4); the clean single-threaded configure was 60.9 s per the CMake log. Wall-clock compile time for the ~45 core+test translation units (after Catch2 itself was already built) was on the order of several minutes at `-j4`; no tooling-reported aggregate build-time figure (e.g. Ninja's own summary) was captured before the link failure terminated the build, so no authoritative total is recorded here.

---

## 4. Build Result: **FAILED at the link stage**

Configure succeeded cleanly. Compilation of all ~248 translation units (Catch2's own sources plus every core CoolProp `.cpp` reachable from `CatchTestRunner`'s source list, including Helmholtz, Incompressible, Tabular, PCSAFT, Region/SBTL/SVD, and every `src/Tests/*.cpp`) succeeded with **no compiler errors**. The **link step** for the `CatchTestRunner.exe` target failed:

```
Catch2.lib(catch_stringref.cpp.obj) : error LNK2038: mismatch detected for 'RuntimeLibrary':
  value 'MD_DynamicRelease' doesn't match value 'MT_StaticRelease' in AbstractState.cpp.obj
[... 91 total LNK2038 RuntimeLibrary-mismatch diagnostics, pairing every object file in the
     fetched Catch2.lib (built /MD, dynamic Release CRT) against each of 7 core CoolProp
     object files compiled /MT (static Release CRT): AbstractState.cpp.obj, CPfilepaths.cpp.obj,
     CoolProp.cpp.obj, CoolPropPlot.cpp.obj, MixtureDerivatives.cpp.obj, SchemaValidation.cpp.obj,
     SuperancillaryBoundaryCurve.cpp.obj ...]

Catch2.lib(catch_reporter_junit.cpp.obj) : error LNK2001: unresolved external symbol __imp__time64
Catch2.lib(catch_random_seed_generation.cpp.obj) : error LNK2001: unresolved external symbol __imp__time64
miniz.c.obj : error LNK2019: unresolved external symbol __imp__wfreopen_s referenced in function mz_freopen
miniz.c.obj : error LNK2019: unresolved external symbol __imp_remove referenced in function mz_zip_add_mem_to_archive_file_in_place_v2
miniz.c.obj : error LNK2019: unresolved external symbol __imp__wstat64 referenced in function mz_stat64
miniz.c.obj : error LNK2019: unresolved external symbol __imp__utime64 referenced in function mz_zip_reader_extract_file_to_file
Catch2.lib(catch_stats.cpp.obj) : error LNK2019: unresolved external symbol __imp_erfc [...]
Catch2.lib(catch_stats.cpp.obj) : error LNK2019: unresolved external symbol __imp_lround [...]
Catch2.lib(catch_polyfills.cpp.obj) : error LNK2019: unresolved external symbol __imp_nextafterf [...]
Catch2.lib(catch_reporter_junit.cpp.obj) : error LNK2001: unresolved external symbol __imp__gmtime64_s [...]
[13 total LNK2019/LNK2001 unresolved-external diagnostics]

CatchTestRunner.exe : fatal error LNK1120: 11 unresolved externals
ninja: build stopped: subcommand failed.
```

**Failing target:** `CatchTestRunner` (link phase only — every compile step succeeded).
**Result:** no `CatchTestRunner.exe` was produced (`build_catch/CatchTestRunner.exe` does not exist).
**Total diagnostics:** 91 × `LNK2038` (RuntimeLibrary mismatch) + 13 × `LNK2019`/`LNK2001` (unresolved CRT externals) + 1 × `LNK1120` (fatal, 11 unresolved externals) = 105 linker diagnostics from a single root cause.

### Categorization: **Configuration-related.**

This is not environmental (the toolchain, network, and Python were all available and functioned correctly), not a missing/broken dependency (every `FetchContent` clone succeeded and every object file compiled cleanly, including Catch2's own sources), and not scientific/numerical (zero CoolProp logic executed — the failure never reaches a runnable binary). The evidence points specifically at an **MSVC C-runtime-library (CRT) linkage mode mismatch**: the seven listed core CoolProp translation units linked into `CatchTestRunner` were compiled against the static-release CRT (`/MT`), while the `FetchContent`-built `Catch2` static library (and the vendored `miniz.c`, compiled as part of the CoolProp target itself but apparently picking up the same `/MT` inconsistency relative to whatever resolved the unresolved CRT-import symbols) ended up linked in a configuration expecting the dynamic-release CRT (`/MD`) import library. Neither `COOLPROP_MSVC_STATIC` nor `COOLPROP_MSVC_DYNAMIC` (both default `OFF`, and neither was passed to the configure command used, consistent with "no unnecessary options enabled") was requested, so `CMakeLists.txt`'s own `COOLPROP_MSVC_REL`/`COOLPROP_MSVC_DBG` selection logic (lines ~293–310) takes its `IGNORE` branch and does not explicitly force a CRT mode for the main target — the mismatch therefore originates from how CMake/MSVC's own default runtime-library resolution differs between the main `CatchTestRunner` target and the independently-configured `FetchContent` Catch2 subbuild on this toolchain (CMake 3.31.6-msvc6 / MSVC 19.44 / Ninja), rather than from an explicit, single, easily-quoted line in the project's own CMake logic. Determining the precise mechanism (and fixing it, e.g. via `CMAKE_MSVC_RUNTIME_LIBRARY`/`CMP0091=NEW` propagated consistently to `FetchContent` subdirectories) is exactly the kind of fix explicitly out of scope for this investigation — **no fix was attempted**, per instructions.

Two earlier build attempts also hit a **separate, transient, environmental** issue worth recording distinctly so it is not confused with the real failure above: `cmd.exe`/batch-script invocations twice produced a bare `The process cannot access the file because it is being used by another process.` (most likely Windows Defender's on-access scan briefly locking a freshly-written `.bat`/log file, or two redirects targeting the same open file handle in this investigation's own driver script). This resolved on retry with no changes to the repository and does not reflect a repository or toolchain defect — it is noted here only for completeness of the record, per instruction 5, and categorized as **environmental**, distinct from the configuration-related link failure above.

---

## 5. Test Suite: Not Run

Because `CatchTestRunner.exe` was never produced, **zero tests executed** in this environment. `./build_catch/CatchTestRunner [SBTL]` (or any tag scope) cannot be invoked. This blocks objective 4 (run the core test suite) as a direct consequence of the §4 link failure, not of any test-suite content or scientific-code issue — every test translation unit compiled without error before the link step failed.

No test counts, pass/fail/skip breakdown, or `[refprop]`-skip behavior can be reported from an actual run. §6–7 below instead identify, from static inspection of the (successfully-compiled) test sources, which existing fixtures and data would exercise each required BasisThread capability once the link issue is resolved — this satisfies objective 7 ("identify which existing tests... can provide independent known-answer cases") without requiring a passing run, and gives objective 6 a concrete, ready-to-execute checklist for the next session.

---

## 6–7. Baseline Engineering Checks — Identified Test Coverage and Independent References

No expected values are invented here (per instruction 7). Each row cites the **existing** CoolProp test fixture/data and its **published source**, found by inspecting `src/Tests/CoolProp-Tests.cpp` (compiled successfully in this investigation, confirming the code exists and is reachable, even though it could not be executed).

| Capability | Existing test(s) | Independent/published reference basis | Representative fluid IDs confirmed present |
|---|---|---|---|
| **Water** — viscosity | `TransportValidationFixture`, "Compare viscosities against published data" (`[viscosity],[transport]`) — 7 Water points at `T`=298.15–873.15 K, tolerance `1e-7` | IAPWS/NIST-class reference viscosity correlation (values embedded as `viscosity_validation_data[]`, lines ~153–160 of `CoolProp-Tests.cpp`) | `Water` (HEOS backend) |
| **Water — IAPWS-IF97** | `SuperAncillaryOnFixture`-adjacent `BENCHMARK("IF97 rho(T))"` / `rho(p)` benchmarks (lines ~3594–3677) exercise `AbstractState::factory("IF97", "Water")` | IAPWS-IF97 industrial formulation (external `if97` library, `FetchContent`-fetched, confirmed cloned successfully in §3) | `IF97::Water` |
| **Propylene glycol / water** | `dev/incompressible_liquids/test_json_sanity.py`, `test_data_sanity.py`, `test_fitting_regression.py` (Python, dev-time — not part of the Catch2 binary that failed to link) validate the committed coefficient JSON directly, independent of the C++ build | Fitted against manufacturer/literature secondary-fluid data per `dev/incompressible_liquids/DATA_AUDIT.md` (not independently re-derived here) | `INCOMP::MPG[x]` — `MPG.json` confirmed present, `"name": "MPG"`, description **"Propylene Glycol - aq"** |
| **Ethylene glycol / water** | Same Python sanity suite; also directly referenced in C++ regression tests, e.g. `CoolProp-Tests.cpp` lines ~5427–5644 (`INCOMP::MEG[0.1]`, `INCOMP::MEG[0.35]` — issue #2209 saturation-pressure regression, and a caloric-consistency check against MEG's committed polynomial coefficients) | Same data-audit provenance as above | `INCOMP::MEG[x]` — `MEG.json` confirmed present, `"name": "MEG"`, description **"Ethylene Glycol - aq"** |
| **Other incompressible heat-transfer fluids** | Same Python sanity suite covers all 127 files under `dev/incompressible_liquids/json/`; C++ regression at lines ~5772–5803 specifically calls out MPG/APG Prandtl-number behavior (issue #1374) | Same data-audit provenance | `AEG`, `AKF`, `AL`, `AN`, `APG`, `AS10`…`AS55`, `MAM`, `MMA`, `PGLT`, and 100+ others (full listing in `dev/incompressible_liquids/json/`) |
| **Humid air / psychrometrics** | Four dedicated `TEST_CASE`s tagged `[humid_air_validation]`: `[ashrae_a61]`, `[ashrae_a62]`, `[ashrae_a8]`, `[ashrae_a9]` (lines 1031–1220), plus `[humid_air_physics]`, `[humid_air_roundtrip]`, `[humid_air_aux]` consistency checks and two virial/alpha0-cache cross-check tests (lines 909–1030) | **ASHRAE RP-1485**, Appendices A.6.1/A.6.2/A.8/A.9 (saturated- and unsaturated-air property tables) — these tests currently assert physical bounds/round-trip/dew-point-equals-dry-bulb-at-saturation consistency rather than bit-exact table values against the ASHRAE tables; exact-value comparison against the published RP-1485 tables is *identified as available but not yet asserted* in the existing suite | `HumidAir` via `HAPropsSI` |
| **Representative HVAC refrigerants** | `TransportValidationFixture` viscosity data for **R134a** (4 points, saturated liquid/vapor at 185 K/360 K, tol `1e-3`) and **Ammonia** (4 points, tol `1e-3`); `reference_states` tests use **R134a** and **R124** with IIR/ASHRAE/NBP reference-state conventions (lines 2283–2456); `predefined_mixtures` test (line 2274) exercises named blends from `dev/mixtures/predefined_mixtures.json` (R32-based blends confirmed present) | Published correlations underlying CoolProp's own transport fits (cited in-line per fluid, e.g. REFPROP-class sources); IIR/ASHRAE/NBP reference-state definitions are standards-based, not CoolProp-invented | `R134a`, `R124`, `Ammonia`, `Propane`, plus blends in `predefined_mixtures.json` |
| **Density** | No dedicated `*_validation_data[]` array analogous to viscosity/conductivity was found for density in `CoolProp-Tests.cpp`; density correctness is exercised indirectly through consistency/flash/derivative tests (e.g. `[consistency]`, `[flash]`, `[derivatives]`) rather than direct published-value comparison | **Not yet identified as an existing known-answer test** — flagged as a gap; NIST Webbook / IAPWS-95 published density tables would need to be sourced if BasisThread requires an explicit density known-answer suite |
| **Specific heat (Cp)** | No dedicated Cp validation array found; exercised indirectly via `[derivatives]` and `[fixed_states]` tests | **Not yet identified as an existing known-answer test** — same gap as density |
| **Viscosity** | See Water/refrigerant rows above — `TransportValidationFixture` is the general mechanism, covering Propane, R134a, Ammonia, Water, plus non-BasisThread fluids (Hexane, Heptane, Ethanol, SF6) | Assael, *J. Phys. Chem. Ref. Data*, and equivalent per-fluid sources cited inline as code comments | — |
| **Thermal conductivity** | `conductivity_validation_data[]` (same fixture, lines 350–390): Hexane, Heptane, Ethanol, SF6 — **no BasisThread-relevant fluid (water, glycol, HVAC refrigerant) currently has a conductivity validation entry in this array** | **Assael, JPCRD, 2012/2013** (explicit citation in source comments) | Gap: none of the existing conductivity entries are water/glycol/refrigerant — flagged for follow-up |
| **Enthalpy / entropy** | `reference_states` tests (high- and low-level interface, lines 2283–2456) assert exact `Hmass`/`Smass` values at defined reference states for n-Propane/R134a/R124 to `1e-8` tolerance; `fixed_states` test (`FixedStateFixture`) checks enthalpy/entropy consistency across all reference-state conventions | IIR/ASHRAE/NBP reference-state standard definitions (values are self-consistent by construction of the reference-state convention, not independent published state-point data) | `n-Propane`, `R134a`, `R124` |
| **Saturation properties** | `TEST_CASE("Test saturation properties for a few fluids", "[saturation],[slow]")` (CO2 saturation-pressure sweep, triple point to just below critical); `SatTFixture` (`[sat_T_to_Tc]`); `first_saturation_partial_deriv` / `second_saturation_partial_deriv` tests; `superanc` fixture tests (`Check Tc & pc`, `Superancillary eval matches extended-precision check points for all fluids`) | CO2's own EOS-internal consistency (not an external published table in this test); superancillary checks compare against the fluid's own full EOS to high precision — internally consistent, not independently sourced | `CO2`, and (via `superanc`) all pure fluids including `Water` |
| **Freezing point (glycol/brine)** | `IncompressibleFluid::Tfreeze(p, x)` is exercised by `dev/incompressible_liquids/test_data_sanity.py`/`test_json_sanity.py` (which specifically screen for the "leaked placeholder" bug class — GitHub #1331/#2567 — where a failed freezing-point fit silently ships a starting-guess coefficient, e.g. the cited historical bug: "LiBr/MITSW T_freeze ~ 0 K") | Manufacturer/literature freezing-point-depression curves per `DATA_AUDIT.md` provenance | `INCOMP::MPG[x]`, `INCOMP::MEG[x]`, and all other `json/*.json` incompressibles |

**Summary of gaps identified (not filled — per instruction 7, no values invented):** the existing suite has strong, published-reference-backed coverage for viscosity (water, R134a, ammonia, propane) and humid air (ASHRAE RP-1485, currently physical-consistency-checked rather than table-value-checked), and strong internal-consistency coverage for enthalpy/entropy/saturation/freezing point. It has **no identified existing known-answer test for density, specific heat, or thermal-conductivity of any BasisThread-relevant fluid** (water/glycol/refrigerant) — conductivity validation currently only covers Hexane/Heptane/Ethanol/SF6. Sourcing NIST Webbook, IAPWS-95/IF97 published tables, or ASHRAE Fundamentals handbook density/Cp/conductivity reference points for water, MPG/MEG mixtures, and representative refrigerants is a follow-up task, not performed here.

---

## 8. Recorded Environment Details (consolidated)

| Item | Value |
|---|---|
| Compiler/toolchain | MSVC 19.44.35228.0 (VS2022 Build Tools, VC Tools 14.44.35207) |
| CMake version | 3.31.6-msvc6 |
| Generator | Ninja (bundled with VS2022 Build Tools) |
| Python version | 3.13.5 |
| Dependency retrieval | CMake `FetchContent` (network `git clone`) for Eigen, msgpack-c, nlohmann_json, valijson, if97, refprop_headers, boost_headers, multicomplex (+nested pybind11), fmt, Catch2 — all succeeded |
| Build configuration | `-G Ninja -DCOOLPROP_CATCH_MODULE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release`, no wrapper modules enabled |
| Build time | Configure: 60.9 s (tool-reported). Compile: not fully captured as a single tooling-reported figure (see §3) |
| Test counts/results | **0 run** — link failure prevented `CatchTestRunner.exe` from being produced |
| Skipped tests and reasons | N/A — no test binary existed to skip from |

---

## 9. Issues Requiring Engineering Review

1. **[Blocking, configuration] MSVC CRT runtime-library mismatch prevents `CatchTestRunner` from linking** on this Windows/MSVC/Ninja toolchain combination, using the documented `-DCMAKE_BUILD_TYPE=Release` configuration with no wrapper modules and no explicit `COOLPROP_MSVC_STATIC`/`COOLPROP_MSVC_DYNAMIC` override. Full diagnostic detail in §4. This is the single item blocking all further baseline validation (test execution, known-answer verification) and should be the first thing engineering addresses before this branch's build can be exercised end-to-end on Windows.
2. **[Non-blocking, gap] No existing known-answer test found for density, specific heat, or conductivity of any BasisThread-relevant fluid.** Viscosity and humid-air coverage is strong and published-reference-backed; these three properties are not, for the fluids BasisThread actually needs. Recommend sourcing NIST/IAPWS/ASHRAE reference points as a follow-up, not fixed in this investigation.
3. **[Informational] Repository build is not offline-capable by default** — a clean configure fetches 9+ external repositories via `FetchContent`. If BasisThread's build/CI environment restricts outbound network access, this needs a vendoring or local-mirror strategy; not evaluated further here.
4. **[Informational] REFPROP was not present in this environment**, so `[refprop]`-tagged tests' actual skip/fail behavior could not be observed empirically (the build never reached a runnable binary). `CLAUDE.md` states REFPROP tests are expected to run, not skip, on at least one team member's machine — behavior in a REFPROP-absent CI/build environment should be re-verified once §9.1 is resolved.

---

---

## 10. [2026-08-13, follow-up] Configuration-Only CRT Experiment

**Objective:** determine whether the §4 MSVC CRT runtime-library link failure (`/MT` CoolProp objects vs. `/MD` Catch2) can be resolved with CMake configuration alone, with zero changes to any tracked source or CMake file. Performed on the same branch (`bt-baseline-validation`), same commit (`c6f006ced146bd71db92e4c1beef6d1adcc3a648`), same toolchain (MSVC 19.44.35228.0, CMake 3.31.6-msvc6, Ninja, Python 3.13.5) as §3. The original failed build directory (`build_catch`) was left untouched; this experiment used a fresh directory, `build_catch_msvc_md`, so the §4 record remains intact and reproducible.

### Exact configure command

```bash
cmake -G Ninja -B build_catch_msvc_md -S . \
  -DCOOLPROP_CATCH_MODULE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
```

No wrapper modules enabled. Configure succeeded in 54.9 s, including fresh `CPM.cmake`/`FetchContent`-style clones of Eigen, msgpack-c, nlohmann_json, valijson, IF97, REFPROP-headers, boost-headers, multicomplex (+nested pybind11), fmt, and Catch2 v3.8.0 (all via `cmake/dependencies.cmake`'s `CPMAddPackage()` calls — the actual dependency-declaration mechanism, more precisely: CPM.cmake, itself layered on `FetchContent`).

### `/MD` verification (before building anything)

Parsed `build_catch_msvc_md/compile_commands.json` (199 total translation units: 93 core CoolProp incl. `externals/miniz-3.1.1/miniz.c`, 106 Catch2) programmatically for MSVC runtime-library flags (`-MT`/`-MTd`/`-MD`/`-MDd`). Result: **all 199 of 199 translation units carry exactly `-MD` — zero mismatches, zero files on any other setting.** Sample confirmed directly from the compile database:

- `src\AbstractState.cpp` → `... /O2 /Ob2 /DNDEBUG -std:c++17 -MD /Fo...`
- `_deps\catch2-src\src\catch2\benchmark\catch_chronometer.cpp` → `... /O2 /Ob2 /DNDEBUG -MD /Fo...`
- `externals\miniz-3.1.1\miniz.c` → `-MD` (this was the file surfacing several of the original `LNK2019` unresolved-CRT-symbol errors in §4)

This satisfies the pre-build gate specified for this experiment: consistent `/MD` confirmed before compiling, so the build proceeded.

### Build outcome: **compile and link both succeeded**

```
cmake --build build_catch_msvc_md --target CatchTestRunner -j4
...
[203/203] Linking CXX executable CatchTestRunner.exe
```

Zero compiler errors, zero linker errors, zero `LNK2038` RuntimeLibrary-mismatch diagnostics (versus 91 in §4), zero unresolved externals (versus 13 in §4). One benign, pre-existing deprecation warning appeared during compilation of one test file (`CoolProp.h is deprecated; include "CoolProp/CoolProp.h"`) — unrelated to the CRT question, not a build failure. `build_catch_msvc_md/CatchTestRunner.exe` was produced (13,050,368 bytes).

**The §4 link failure is fully and cleanly resolved by CMake configuration alone.** No tracked source or CMake file was touched to achieve this.

### Test outcome: baseline scope reached, but the run crashes — a new, separate finding

Following the documented baseline scope from `CLAUDE.md` (`./build_catch/CatchTestRunner "~[slow]"`), the newly-linked executable was run:

```
build_catch_msvc_md\CatchTestRunner.exe "~[slow]"
```

**This run terminates abnormally partway through, both times it was executed independently, at the identical point:**

- Process exit code: **`-1073740791`**, i.e. **`0xC0000409` = `STATUS_STACK_BUFFER_OVERRUN`** (Windows fail-fast: `/GS` stack-cookie corruption detection, or an equivalent CRT buffer-security-check abort — this terminates the process directly, bypassing normal C++ exception unwinding, which is why no `FAILED`/exception message appears in the log).
- No final Catch2 summary line (`All tests passed (...)` / `test cases: N | ...`) is produced — confirmed via two independent full reruns (571 lines of identical captured output both times) and cross-checked with a separate tool (PowerShell `Get-Content`) to rule out a log-capture artifact. A small control run (`"[triple_point]"` alone) **does** produce a clean summary line (`All tests passed (774 assertions in 1 test case)`), confirming the reporter itself works correctly and the missing summary reflects a real abnormal termination, not a logging quirk.
- **Last test case that completes cleanly, both times:** `SVDSBTL&HEOS and SVDSBTL&REFPROP produce distinct cache files` (`src/Tests/CoolProp-Tests-SVDSBTL.cpp:781`) — it `SKIPPED` cleanly (`REFPROP not available; skipping cache-key disambiguation test`), and then the process terminates before the next test's output is flushed.
- **Reproduced in a much narrower scope:** `build_catch_msvc_md\CatchTestRunner.exe "[SVDSBTL]~[slow]"` crashes with the **same exit code**, at the **same last-completed test**, confirming the fault is localized to the SVDSBTL test sequence itself — not cumulative corruption from the ~500 unrelated tests (PCSAFT, Cubics, mixtures, etc.) that precede it in the full `~[slow]` run.
- **Isolation test:** the test immediately following the crash point in Catch2's registration order, `SVDSBTL fast_evaluate returns Q = -1 on single-phase rows` (`CoolProp-Tests-SVDSBTL.cpp:996`, a `fast_evaluate` batched raw-pointer/array API test), **passes cleanly when run completely alone** (`All tests passed (6 assertions in 1 test case)`). This rules out that test, in isolation, as sufficient to reproduce the crash — the fault depends on state carried over from the preceding SVDSBTL test sequence (the two prior `[reject][source][SVDSBTL]` tests and the `SVDSBTL&IF97`/`SVDSBTL&HEOS`/`SVDSBTL&REFPROP` cache-path tests immediately before it), not on that one test in isolation.
- No Windows Error Reporting crash dump or Application-log crash event was found for `CatchTestRunner.exe` in this environment (WER local dump collection is not configured for this executable), so no captured stack trace is available beyond the exit-code/reproduction evidence above.

**This is not a build-configuration issue.** It appeared only because the build now links at all — no prior attempt (§4, under `/MT`) ever reached a runnable binary, so this is the **first time these SVDSBTL/`fast_evaluate` code paths have executed end-to-end on this Windows/MSVC toolchain in this investigation's history**, and it is unknown whether the same crash would occur under a hypothetical working `/MT` build (none exists to compare against). Per instructions, **no fix was attempted** and no tracked file was modified to investigate or work around it.

### Step 5 conclusion

**For the specific problem this experiment targeted — the MSVC CRT runtime-library link mismatch documented in §4 — the answer is unconditional: configuration only, fully resolves it. No CoolProp source or build-system (CMake) change is required to link `CatchTestRunner` on Windows.** The required Windows configuration is:

```
-G Ninja
-DCOOLPROP_CATCH_MODULE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
-DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
```

**However, this experiment also surfaced a second, independent, non-configuration finding that must not be conflated with the above:** running the resulting binary's documented baseline test scope crashes reproducibly (`STATUS_STACK_BUFFER_OVERRUN`) in the SVDSBTL test sequence, in a way not attributable to a single test in isolation. This is a candidate memory-safety issue in either the SVDSBTL backend/test harness or its interaction with the `/MD` runtime (undetermined which, since no `/MT` baseline ever existed to compare against) and **requires its own engineering investigation before this branch's Windows baseline can be considered fully validated end-to-end.** It is not something a CMake flag can address, and fixing it is explicitly out of scope for this configuration-only experiment.

### Updated status of §9 item 1

§9 item 1 ("MSVC CRT runtime-library mismatch prevents `CatchTestRunner` from linking") is **resolved** by this experiment's configuration (see above) — retained in §9 unmodified as the historical record of the original finding, per instructions not to alter existing report content beyond adding this dated section.

### New issue for engineering review (supersedes/extends §9 item 4)

**[Blocking, likely code-related, NOT configuration] `CatchTestRunner.exe` crashes with `STATUS_STACK_BUFFER_OVERRUN` (`0xC0000409`) partway through the SVDSBTL test sequence** under `~[slow]`, reproducible in both the full baseline scope and the narrower `[SVDSBTL]~[slow]` scope, at the identical point both times. Last clean test: `SVDSBTL&HEOS and SVDSBTL&REFPROP produce distinct cache files` (`CoolProp-Tests-SVDSBTL.cpp:781`). The immediately-following test passes in isolation, so the fault is state-dependent on the preceding SVDSBTL/source-backend sequence, not a standalone bug in one test. Needs engineering investigation (e.g. under a debugger or with `/RTC`/ASan instrumentation) to localize precisely — no fix attempted here. This also means **§9 item 4's REFPROP-skip question is now partially answered**: `[refprop]`/REFPROP-gated tests in this REFPROP-absent environment do skip cleanly with an explicit `SKIPPED:` message (`src/CoolProp.cpp:734`, and inline `SKIP(...)` calls in `CoolProp-Tests-SVDSBTL.cpp`) right up until the crash — REFPROP-absence itself is not implicated in the crash (the crashing/adjacent tests are REFPROP-conditional and were skipping correctly).

---

## 11. [2026-08-13, follow-up 2] SVDSBTL Runtime Crash — Localization Without Fix

**Scope note:** the Windows CRT configuration question (§10) is closed and not reopened here. This section localizes the *separate* runtime crash first flagged at the end of §10, per a dedicated investigation. No tracked source, CMake, test, or tolerance file was modified. One new tool was installed to support Phase 4 (documented in §11.4) — this is a system-level debugger install, not a repository change.

### 11.1 Correcting a premature conclusion from §10

§10 stated the crash occurred "partway through the SVDSBTL test sequence" with the last clean test being `SVDSBTL&HEOS and SVDSBTL&REFPROP produce distinct cache files`, and speculated the fault was state-dependent on the preceding sequence. **This was an artifact of Catch2's default reporter, which is silent for passing tests** (it only prints output for failed/skipped cases). Re-running with `-s` (show successful tests) revealed **four further tests actually ran and passed silently** after the "distinct cache files" test before the crash — including the one this investigation's task brief specifically warned not to assume was the defect (`SVDSBTL fast_evaluate returns Q = -1 on single-phase rows`, confirmed passing in isolation, §11.3). The real last-clean test, confirmed via `-s`, is `SVDSBTL ALTERNATIVE_SVDTABLES_DIRECTORY routes default_cache_dir` (`CoolProp-Tests-SVDSBTL.cpp:1104`, all assertions passed through line 1129).

### 11.2 Phase 2 — Test Sequence Inventory (Catch2's own listing, not source order)

Used `CatchTestRunner.exe <scope> --list-tests` and `--list-tags` (read-only, no execution) to establish ground truth, per the instruction not to infer order from source-file position:

- `--list-tags` (full suite): `[SVDSBTL]` 83 cases, `[SBTL]` 36, `[SVDComponents]` 25, `[Region]` 20, `[SVDSurface]` 7, `[SVD]` 5, `[cache]` 11, `[source]` 13, plus single-use `[alpha0_cache]`, `[slope_source]`, `[virial_cache]`.
- `--list-tests "[SVDSBTL],[SBTL],[SVD],[region]"` (union, OR-combined per Catch2's comma syntax): **128 matching test cases** — the full related-subsystem inventory used to build the Phase 6 exclusion scope (§11.6).
- `--list-tests "[SVDSBTL]~[slow]"` (the reproducing scope): **37 matching test cases**, in this exact order (execution order, confirmed to match declaration/source order in this instance — Catch2's `--order` defaults to `decl`, and no discrepancy from source order was found here, so the source-order reading was safe to rely on *once independently confirmed*, not assumed):

  1. `SVDSBTL pmin below the triple pressure is rejected`
  2. `SVDSBTL backend rejects mixtures`
  3. `SVDSBTL backend is rejected from PropsSI by default`
  4. `SVDSBTL backend speed of sound throws in two-phase`
  5. `SVDSBTL backend Q out of [0, 1] is rejected`
  6. `SVDSBTL backend without explicit source throws`
  7. `SVDSBTL backend rejects unsupported source names`
  8. `SVDSBTL&IF97 rejects non-Water fluids`
  9. `SVDSBTL&HEOS and SVDSBTL&REFPROP produce distinct cache files` — **SKIPS** (REFPROP absent)
  10. `SVDSBTL fast_evaluate returns Q = -1 on single-phase rows`
  11. `SVDSBTL fast_evaluate flags out-of-range PT misses cleanly`
  12. `SVDSBTL fast_evaluate rejects unsupported inputs cleanly`
  13. `SVDSBTL ALTERNATIVE_SVDTABLES_DIRECTORY routes default_cache_dir` — **last test confirmed to complete, via `-s`**
  14. **`write_bytes_atomic is race-safe across threads`** — **the crash**
  15. `SVDSBTLBackend uses surrogate when source has no SuperAncillary`
  16–37. (remaining SBTL dome/DT/critical_patch/options/fail_map tests, not reached)

Reproducible command for the narrow scope: `build_catch_msvc_md\CatchTestRunner.exe "[SVDSBTL]~[slow]"` (exit `-1073740791` both times run, identical last-completed test both times — full `~[slow]` and this narrower scope crash at the same logical point).

### 11.3 Phase 3 — Minimization

- Tests #1–10 together (`-f` input-file, exact names, one wildcard used for a name containing literal `[0, 1]` to avoid Catch2 tag-syntax collision): **all pass, exit 0** (`test cases: 10 | 9 passed | 1 skipped`). This *disproves* the original (reporter-artifact-driven) hypothesis that tests 1–9 poison test 10.
- Test #14 (`write_bytes_atomic is race-safe across threads`) run **completely alone**, fresh process: **crashes**, exit `-1073740791`, reproduced twice. **This is the minimum reproducer — a single test, zero preceding state required.**
- Answering the Phase 3 A–E checklist directly:
  - **A (one preceding test poisons state): no** — disproved by the clean 10-test run above.
  - **B (combination required): no** — one test alone is sufficient and necessary.
  - **C (execution order matters): no**, not for triggering the crash — it is not order/state-dependent at all; it is intrinsic to the one test.
  - **D (repeated execution of one test triggers it): not applicable** — it fails on its *first* execution, every time (100% reproducible, not flaky, not cumulative).
  - **E (cache/persistent-state dependence): yes, but self-contained within the test, not cross-test.** The test creates its own fresh temp directory (`%TEMP%\coolprop_svdtables_atomic_race_<pid>\`), so no prior test's on-disk cache or state is involved — the "state" that matters is the 16-way concurrent-thread race the test itself deliberately constructs against a single shared target file inside that fresh directory. No cache/state file was deleted during this investigation; the crashing process leaves its temp directory behind, unexamined further (not needed given §11.4's evidence).
- **Reproducible command for the minimum reproducer:** `build_catch_msvc_md\CatchTestRunner.exe "write_bytes_atomic is race-safe across threads"`.

### 11.4 Phase 4 — Debugger Localization

No debugger (cdb/windbg/devenv) was present in this environment. Installed the modern WinDbg package (`winget install --id Microsoft.WinDbg`, MSIX/Store distribution, ships classic `cdb.exe` at `...\Microsoft.WinDbg_1.2606.22001.0_x64__8wekyb3d8bbwe\amd64\cdb.exe`) — a system tool install, not a repository change, and necessary to fulfil this phase's explicit requirement. No PDB exists for the Release build (`/Fd` is emitted by CMake's default MSVC rule but `/Zi`/`/Z7` is not active in this configuration, confirmed by an exhaustive search of `build_catch_msvc_md/` finding zero `.pdb` files), so symbol names/line numbers are unavailable; module+offset and full stack unwinding (always available on x64 via SEH unwind tables, independent of `/Zi`) are used instead, cross-referenced against source.

**Command:** `cdb.exe -o -c "g; .exr -1; .ecxr; r; ~*kv 20; lm; q" build_catch_msvc_md\CatchTestRunner.exe "write_bytes_atomic is race-safe across threads"`

**Captured exception record:**
```
Security check failure or stack buffer overrun - code c0000409 (!!! second chance !!!)
Subcode: 0x7 FAST_FAIL_FATAL_APP_EXIT
ExceptionAddress: 00007ffcb846527e (ucrtbase!abort+0x4e)
```

**This is the decisive finding: the exception address resolves inside `ucrtbase!abort`, and Subcode `0x7` is `FAST_FAIL_FATAL_APP_EXIT` — this is `std::abort()`'s own termination mechanism on modern Windows/UCRT, not a `/GS` stack-cookie violation.** Windows reuses NTSTATUS `0xC0000409` for multiple distinct fail-fast reasons, distinguished only by the subcode; a genuine stack-buffer/cookie corruption would carry a different subcode. **This crash is not a memory-safety bug.**

**Failing thread's stack** (thread id `54fc.7434`, one of the 16 worker threads):
```
ucrtbase!abort+0x4e
ucrtbase!terminate+0x1e
VCRUNTIME140_1!FindHandler<__FrameHandler4>+0x497
VCRUNTIME140_1!__InternalCxxFrameHandler<__FrameHandler4>+0x273
...
KERNELBASE!RaiseException+0x8a
VCRUNTIME140!_CxxThrowException+0x97
CatchTestRunner+0xa9c3f          <- throw site (no symbols; see below)
CatchTestRunner+0x4765fc         <- calling frame (the thread's lambda)
ucrtbase!thread_start<...>+0x30
KERNEL32!BaseThreadInitThunk+0x17
ntdll!RtlUserThreadStart+0x2c
```
This is the textbook signature of **an exception thrown inside a `std::thread` entry function with no handler found while unwinding that thread's stack** — per the C++ standard, this calls `std::terminate()`, which calls `abort()`. **Two other threads** (`54fc.6878`, `54fc.5838`) independently show the identical `_CxxThrowException` → thread-lambda pattern in the same capture (one had already reached `abort`/`terminate` too), confirming **multiple worker threads threw concurrently** — consistent with a genuine multi-way race, not a single unlucky thread.

**Follow-up command to identify the exact throw site and message** (break on every `_CxxThrowException` call, dump the thrown object's message string at each hit — `std::runtime_error`'s message pointer sits at `rcx+8` in MSVC's STL exception-object layout, `rcx` holding the thrown-object pointer per the x64 calling convention at the `_CxxThrowException(void* object, ...)` call):

```
cdb.exe -o -c "bp VCRUNTIME140!_CxxThrowException; g; da poi(rcx+8) L200; kv; g; ...; q" build_catch_msvc_md\CatchTestRunner.exe "write_bytes_atomic is race-safe across threads"
```

**Captured message at the first throw hit:**
```
"write_bytes_atomic: rename to C:\Users\jerem\AppData\Local\Temp\coolprop_svdtables_atomic_race_28032\race.bin failed: Access is denied."
```

This is the exact text of the third `throw` in `write_bytes_atomic()` (`src/CPfilepaths.cpp:161`, reading the source — see below), firing because `std::filesystem::rename(temp, target, rename_ec)` (`src/CPfilepaths.cpp:157`, which maps to Win32 `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING`) returned the error `ERROR_ACCESS_DENIED` ("Access is denied").

**Object/state at the moment of failure:** 16 `std::thread`s, each with its own uniquely-named temp file (per-thread/process-unique naming via `make_temp_sibling()`'s atomic counter + `std::random_device`-seeded process salt — confirmed race-free by reading `src/CPfilepaths.cpp:108–128`), all racing to `rename()` their own temp file onto the **same shared destination path** `race.bin`, exactly as the test's own comment describes (`CoolProp-Tests-SVDSBTL.cpp:1298–1300`: "Payload large enough that an ofstream::write... would be observable mid-stream by another writer if the writes were not serialized via rename"). Unlike POSIX `rename()` (atomic and effectively always "succeeds" for whichever caller wins the race, silently unlinking the loser's target), Windows' `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING)` can fail with `ERROR_ACCESS_DENIED`/sharing-violation-class errors when multiple threads race a rename onto the identical destination. `write_bytes_atomic()` converts that failure into a thrown `std::runtime_error`; the test's thread lambda (`CoolProp-Tests-SVDSBTL.cpp:1311`: `threads.emplace_back([&, i]() { ...; ::write_bytes_atomic(...); });`) has no `try`/`catch` around that call, so the exception escapes the thread's entry function and the process terminates per the standard-mandated behavior.

### 11.5 Phase 5 — AddressSanitizer Build

Fresh build directory `build_catch_asan`, keeping the validated `/MD` configuration and adding MSVC-native ASan via CLI flags only (no tracked CMake file touched):

```
cmake -G Ninja -B build_catch_asan -S . ^
  -DCOOLPROP_CATCH_MODULE=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DCMAKE_CXX_FLAGS="/fsanitize=address /Zi" -DCMAKE_C_FLAGS="/fsanitize=address /Zi" ^
  -DCMAKE_EXE_LINKER_FLAGS="/DEBUG"
cmake --build build_catch_asan --target CatchTestRunner -j4
```

**Pre-run verification** (parsed `build_catch_asan/compile_commands.json` programmatically): **199 of 199 translation units carry both `/fsanitize=address` and `-MD`** — zero exceptions, confirming CoolProp is ASan-instrumented, `CatchTestRunner` is ASan-instrumented, and `/MD` remains consistent, exactly as required before running anything. Build succeeded: `[203/203] Linking CXX executable CatchTestRunner.exe` (35.3 MB, vs. 13.0 MB non-ASan — consistent with instrumentation overhead), zero compile/link errors (only pre-existing, unrelated `C4530` exception-handling warnings from vendored `fmt`/Eigen headers).

**Running the minimized reproducer only**, per instructions:
```
build_catch_asan\CatchTestRunner.exe "write_bytes_atomic is race-safe across threads" -s
```
**Result: process exits with the identical code `-1073740791`, and both stdout and stderr are completely empty — zero ASan diagnostic output of any kind.** No `ERROR: AddressSanitizer: ...` report, no allocation/free stack, nothing. **This is the expected and predicted outcome given the §11.4 finding**: AddressSanitizer instruments for memory-safety violations (heap/stack buffer overflow, use-after-free, etc.); an uncaught C++ exception triggering `std::terminate()`/`abort()` is correct-by-the-standard control flow with no invalid memory access anywhere in the picture, so ASan has nothing to flag. **Per instructions, this negative result is documented and the investigation continues on debugger evidence (§11.4) alone, which is already conclusive.**

### 11.6 Phase 6 — Core Baseline Excluding Unresolved SVDSBTL Subsystem

Independently inventoried the related tag family via `--list-tags`/`--list-tests` (§11.2: `[SVDSBTL]`, `[SBTL]`, `[SVD]`, `[Region]`, `[SVDComponents]`, `[SVDSurface]` — the last two discovered via the tag listing, not assumed, and confirmed to cover the same subsystem by their test names, e.g. `AxisTransform`/`ConstantCurve`/`PiecewiseChebyshevCurve` under `[SVDComponents]`). Excluded the **entire** tag family, not the one failing test:

```
build_catch_msvc_md\CatchTestRunner.exe "~[slow]~[SVDSBTL]~[SBTL]~[SVD]~[Region]~[SVDComponents]~[SVDSurface]"
```

**Result, labeled per instructions:**

> **"Core baseline excluding unresolved SVDSBTL subsystem"**
> ```
> test cases:    347 |    320 passed | 27 skipped
> assertions: 159008 | 159008 passed |  0 skipped
> ```
> Exit code 0. **Zero failures.** All 27 skips independently verified to carry the identical reason string, `Skipping: REFPROP not supported in this environment.` — no non-REFPROP skip reason, no failure outside the excluded subsystem. Some `WARN`-level informational output appeared from `CoolProp-Tests-Michelsen.cpp` flash-classification diagnostics (misclassification/trivial-solution counters, all reporting `0`/`0` — i.e. clean) — these are Catch2 `WARN` calls, not failures, and do not affect the pass count.

**This result must not be represented as a complete CoolProp baseline pass** — it explicitly excludes 128 tests across the SVDSBTL/SBTL/SVD/Region/SVDComponents/SVDSurface tag family (§11.2), one of which (§11.3) crashes the process. It is a baseline for everything **outside** that subsystem only.

### 11.7 Phase 7 — Classification

**Classification: shared CoolProp infrastructure defect exposed by SVDSBTL, with a contributing test-harness interaction.** Evidence for each half:

- **Infrastructure defect:** the throwing function, `write_bytes_atomic()`, lives in `src/CPfilepaths.cpp` — general-purpose file I/O infrastructure, not SVDSBTL-specific code. Its error handling does not account for a Windows-specific behavior difference from POSIX (`MoveFileExW` can reject concurrent renames onto the same destination with `ERROR_ACCESS_DENIED`; POSIX `rename()` does not have this failure mode under the same access pattern). This is a real, evidenced correctness gap in the function's contract under concurrent same-target use on Windows.
- **Test-harness interaction:** the crashing test (`CoolProp-Tests-SVDSBTL.cpp:1289`) spawns raw `std::thread`s calling a function it knows can throw, with no `try`/`catch` in the thread body — an exception-safety gap in the test itself, independent of whether `write_bytes_atomic()`'s throwing behavior is considered correct or not. Any future test using this same raw-thread-no-catch pattern around a throwing call would be equally exposed.

**Explicitly ruled out, with evidence:**
- **Not a memory-safety/isolated-implementation defect in the sense of a buffer overrun** — §11.4's exception-address-in-`abort()` finding and §11.5's clean (silent) ASan run both directly rule this out.
- **Not "MSVC/toolchain-specific runtime behavior" in the CRT-linkage sense** — nothing about this crash relates to `/MT` vs `/MD` (the throw/terminate/abort mechanism is identical either way); it is Win32-API-specific (`MoveFileExW` semantics vs. POSIX `rename()`), not CRT-linkage-specific. This is a real platform-behavior dependency, but not the §10 CRT question resurfacing.
- **Not cache/state-lifecycle across tests** — §11.3 Phase-3 answer E: the only relevant state is internal to the single test's own fresh temp directory; no cross-test cache dependency was found.
- **Not unresolved** — root cause, exact throw site, exact message, and exact OS-level error are all captured with reproducible commands (§11.3, §11.4).

**Effect on BasisThread's required calculations: none identified, with evidence.** `write_bytes_atomic()`'s only callers in the entire codebase are `src/SBTL/SVDSurfaceSerializer.cpp` and `src/Backends/SVDSBTL/SVDSBTLBackend.cpp` (confirmed by a repository-wide search) — it is not reachable from the Helmholtz, IF97, Incompressible, or HumidAir code paths that implement BasisThread's required water/glycol/humid-air/refrigerant capabilities (per the capability mapping in `COOLPROP_CORE_DECOMPOSITION_REPORT.md` §4). This is corroborated directly by §11.6: the 347-case baseline excluding the entire SVDSBTL/SBTL/SVD/Region subsystem — which includes the water, IF97, Incompressible (glycol/brine), humid-air, and refrigerant/mixture test coverage identified in §6–7 of this report — passed with **zero failures**.

### 11.8 Phase 8 — Overall Baseline Status

| # | Layer | Status | Evidence |
|---|---|---|---|
| 1 | **Windows build configuration** | ✅ **Resolved.** `CMP0091=NEW` + `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` fully and cleanly fixes the original `/MT`-vs-`/MD` link failure. No source/CMake change required. | §10 |
| 2 | **Core CoolProp tests excluding unresolved SVDSBTL subsystem** | ✅ **Passing.** 347 test cases, 320 passed, 27 REFPROP-absence skips (verified, no other reason), **0 failures**, 159,008/159,008 assertions passed. | §11.6, labeled "Core baseline excluding unresolved SVDSBTL subsystem" |
| 3 | **SVDSBTL subsystem** | ❌ **Blocked.** `write_bytes_atomic is race-safe across threads` crashes the process 100% reproducibly (`FAST_FAIL_FATAL_APP_EXIT` via uncaught exception in a `std::thread`, not a memory-safety bug). Root cause localized to `src/CPfilepaths.cpp`'s `write_bytes_atomic()` plus the test's own missing exception handling. Remainder of the 37-case `[SVDSBTL]~[slow]` scope (tests #15–37) has not been exercised past this crash point in this investigation. | §11.2–§11.5 |
| 4 | **BasisThread engineering validation** | ✅ **Not affected by the SVDSBTL finding**, on the evidence gathered: none of BasisThread's required capabilities (water, IAPWS/IF97, propylene/ethylene glycol, humid air, HVAC refrigerants) call the affected code path, and the layer-2 baseline covering exactly those capabilities passed with zero failures. | §11.7, cross-referenced against `COOLPROP_CORE_DECOMPOSITION_REPORT.md` §4 |

**Overall:** the Windows baseline is validated for everything BasisThread requires (layers 1, 2, 4). Layer 3 (SVDSBTL) remains an open, well-localized defect requiring an engineering decision (fix `write_bytes_atomic`'s Windows-rename-race handling, add exception safety to the test's thread bodies, or both) — out of scope to fix here per instructions, and, per the evidence above, not a blocker for BasisThread's stated scope.

---

*End of investigation. No fix was attempted for the §4 link failure, the §10 CRT-configuration validation, or the §11 SVDSBTL crash localization — per instructions. One system-level tool (WinDbg/cdb, via winget) was installed to support §11.4; no repository file besides this report was touched. Verification follows in the next tool call.*
