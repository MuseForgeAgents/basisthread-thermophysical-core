#if defined(ENABLE_CATCH)
#    include <catch2/catch_all.hpp>
#    include <catch2/matchers/catch_matchers_floating_point.hpp>

#    include <cmath>

#    include "CoolProp/AbstractState.h"
#    include "CoolProp/DataStructures.h"
#    include "CoolProp/Exceptions.h"

using namespace CoolProp;

TEST_CASE("GERG backend families are registered", "[GERG]") {
    backend_families f1 = INVALID_BACKEND_FAMILY, f2 = INVALID_BACKEND_FAMILY;

    extract_backend_families("GERG2004", f1, f2);
    CHECK(f1 == GERG2004_BACKEND_FAMILY);

    extract_backend_families("GERG2008", f1, f2);
    CHECK(f1 == GERG2008_BACKEND_FAMILY);
}

TEST_CASE("GERG factory reaches the GERG backend", "[GERG]") {
    // Task 1 ships a skeleton whose constructor throws NotImplementedError.
    // The point of this test is that we get THAT exception, proving dispatch
    // works, rather than a ValueError about an unknown backend.
    CHECK_THROWS_AS(AbstractState::factory("GERG2008", std::vector<std::string>{"Methane"}), NotImplementedError);
    CHECK_THROWS_AS(AbstractState::factory("GERG2004", std::vector<std::string>{"Methane"}), NotImplementedError);
}

#    include "../Backends/GERG/GERGData.h"

using namespace CoolProp::GERG;

TEST_CASE("GERG component sets have the published sizes", "[GERG]") {
    CHECK(component_names(GERGModel::GERG_2004).size() == 18);
    CHECK(component_names(GERGModel::GERG_2008).size() == 21);
}

TEST_CASE("GERG pure info matches the monograph", "[GERG]") {
    // Table A3.5, GERG-2004 monograph. Tabulated in mol/dm^3 and kg/kmol;
    // our accessor returns mol/m^3 and kg/mol.
    auto methane = get_pure_info(GERGModel::GERG_2004, "methane");
    CHECK_THAT(methane.rhoc_molm3, Catch::Matchers::WithinRel(10.139342719e3, 1e-14));
    CHECK_THAT(methane.Tc_K, Catch::Matchers::WithinRel(190.564, 1e-14));
    CHECK_THAT(methane.M_kgmol, Catch::Matchers::WithinRel(16.042460e-3, 1e-14));

    // GERG-2008 changes carbon monoxide and isopentane relative to GERG-2004.
    CHECK_THAT(get_pure_info(GERGModel::GERG_2004, "carbonmonoxide").Tc_K, Catch::Matchers::WithinRel(132.800, 1e-14));
    CHECK_THAT(get_pure_info(GERGModel::GERG_2008, "carbonmonoxide").Tc_K, Catch::Matchers::WithinRel(132.860, 1e-14));

    // Added in GERG-2008 only.
    CHECK_THROWS_AS(get_pure_info(GERGModel::GERG_2004, "n-decane"), CoolProp::ValueError);
    CHECK_NOTHROW(get_pure_info(GERGModel::GERG_2008, "n-decane"));
}

TEST_CASE("GERG component resolution accepts CoolProp names and aliases", "[GERG]") {
    CHECK(resolve_component(GERGModel::GERG_2008, "Methane") == "methane");
    CHECK(resolve_component(GERGModel::GERG_2008, "METHANE") == "methane");
    CHECK(resolve_component(GERGModel::GERG_2008, "CO2") == "carbondioxide");
    CHECK(resolve_component(GERGModel::GERG_2008, "124-38-9") == "carbondioxide");
    CHECK(resolve_component(GERGModel::GERG_2008, "n-Butane") == "n-butane");

    // Known to CoolProp, outside the GERG set.
    CHECK_THROWS_AS(resolve_component(GERGModel::GERG_2008, "R134a"), CoolProp::ValueError);
    // In GERG-2008 but not GERG-2004.
    CHECK_THROWS_AS(resolve_component(GERGModel::GERG_2004, "HydrogenSulfide"), CoolProp::ValueError);
    CHECK_NOTHROW(resolve_component(GERGModel::GERG_2008, "HydrogenSulfide"));
    // Not a fluid at all.
    CHECK_THROWS_AS(resolve_component(GERGModel::GERG_2008, "NOT A FLUID"), CoolProp::ValueError);
}

TEST_CASE("GERG pure residual coefficients are internally consistent", "[GERG]") {
    for (auto model : {GERGModel::GERG_2004, GERGModel::GERG_2008}) {
        for (const auto& name : component_names(model)) {
            CAPTURE(name);
            auto pc = get_pure_coeffs(model, name);
            REQUIRE(pc.n.size() > 0);
            CHECK(pc.t.size() == pc.n.size());
            CHECK(pc.d.size() == pc.n.size());
            CHECK(pc.c.size() == pc.n.size());
            CHECK(pc.l.size() == pc.n.size());
            // c_i is 1 exactly when l_i > 0
            for (std::size_t i = 0; i < pc.n.size(); ++i) {
                CHECK(pc.c[i] == ((pc.l[i] > 0) ? 1.0 : 0.0));
            }
        }
    }
}

TEST_CASE("GERG pure residual coefficients match the monograph", "[GERG]") {
    // Methane, Table A3.2 (24 terms), first and last n.
    auto ch4 = get_pure_coeffs(GERGModel::GERG_2004, "methane");
    REQUIRE(ch4.n.size() == 24);
    CHECK_THAT(ch4.n[0], Catch::Matchers::WithinRel(0.57335704239162, 1e-14));

    // The generalised 12-term set shared by most fluids.
    auto c3h8 = get_pure_coeffs(GERGModel::GERG_2004, "propane");
    REQUIRE(c3h8.n.size() == 12);
    CHECK(c3h8.t == std::vector<double>{0.250, 1.125, 1.500, 1.375, 0.250, 0.875, 0.625, 1.750, 3.625, 3.625, 14.500, 12.000});
    CHECK(c3h8.d == std::vector<double>{1, 1, 1, 2, 3, 7, 2, 5, 1, 4, 3, 4});
    CHECK(c3h8.l == std::vector<double>{0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3});

    // GERG-2008 changed carbon monoxide and isopentane.
    CHECK(get_pure_coeffs(GERGModel::GERG_2004, "carbonmonoxide").n != get_pure_coeffs(GERGModel::GERG_2008, "carbonmonoxide").n);
    CHECK_THAT(get_pure_coeffs(GERGModel::GERG_2008, "carbonmonoxide").n[0], Catch::Matchers::WithinRel(0.90554, 1e-14));

    // Unchanged fluids fall through identically.
    CHECK(get_pure_coeffs(GERGModel::GERG_2004, "methane").n == get_pure_coeffs(GERGModel::GERG_2008, "methane").n);
}

namespace {

/// The reduced ideal-gas Helmholtz energy of one pure component, written out
/// exactly as teqp's GERG200XAlphaig::alphaig_pure does (GERG.hpp:395-410).
/// Note that the R*/R ratio multiplies the WHOLE bracket, and that the two
/// cosh terms are SUBTRACTED while the coefficients themselves are stored
/// positive.
double gerg_alphaig_pure(const AlphaigCoeffs& c, const PureInfo& info, double T, double rho) {
    const double x = info.Tc_K / T;
    double s = c.n0[1] + c.n0[2] * x + c.n0[3] * std::log(x);
    if (c.theta0[4] != 0) {
        s += c.n0[4] * std::log(std::abs(std::sinh(c.theta0[4] * x)));
    }
    if (c.theta0[6] != 0) {
        s += c.n0[6] * std::log(std::abs(std::sinh(c.theta0[6] * x)));
    }
    if (c.theta0[5] != 0) {
        s -= c.n0[5] * std::log(std::abs(std::cosh(c.theta0[5] * x)));
    }
    if (c.theta0[7] != 0) {
        s -= c.n0[7] * std::log(std::abs(std::cosh(c.theta0[7] * x)));
    }
    return std::log(rho / info.rhoc_molm3) + (RSTAR_GERG / R_GERG) * s;
}

}  // namespace

TEST_CASE("GERG ideal-gas coefficient tables have the padded monograph shape", "[GERG]") {
    for (auto model : {GERGModel::GERG_2004, GERGModel::GERG_2008}) {
        for (const auto& name : component_names(model)) {
            CAPTURE(name);
            auto c = get_alphaig_coeffs(model, name);
            REQUIRE(c.n0.size() == 8);
            REQUIRE(c.theta0.size() == 8);
            // Padding so that the monograph's 1-based indices work.
            CHECK(c.n0[0] == 0.0);
            for (std::size_t i = 0; i < 4; ++i) {
                CHECK(c.theta0[i] == 0.0);
            }
        }
    }
    // Added in GERG-2008 only.
    CHECK_THROWS_AS(get_alphaig_coeffs(GERGModel::GERG_2004, "n-decane"), CoolProp::ValueError);
    CHECK_NOTHROW(get_alphaig_coeffs(GERGModel::GERG_2008, "n-decane"));
}

TEST_CASE("GERG ideal-gas theta values match the monograph", "[GERG]") {
    // Table A3.1: methane. n0[1] and n0[2] are recomputed, so only n0[3..7]
    // and theta0[4..7] are comparable with the published table.
    auto c = get_alphaig_coeffs(GERGModel::GERG_2004, "methane");
    CHECK_THAT(c.n0[3], Catch::Matchers::WithinRel(3.000880, 1e-14));
    CHECK_THAT(c.n0[4], Catch::Matchers::WithinRel(0.763150, 1e-14));
    CHECK_THAT(c.n0[7], Catch::Matchers::WithinRel(-4.469210000, 1e-14));
    CHECK_THAT(c.theta0[4], Catch::Matchers::WithinRel(4.306474465, 1e-14));
    CHECK_THAT(c.theta0[7], Catch::Matchers::WithinRel(5.722644361, 1e-14));

    // Sign convention: the two cosh coefficients are stored POSITIVE as
    // published, with the minus sign living in the evaluating expression.
    // Task 8 hands these straight to IdealHelmholtzGERG2004Cosh, whose all()
    // accumulates -n[i]*log(|cosh(...)|) internally.
    auto h2o = get_alphaig_coeffs(GERGModel::GERG_2004, "water");
    CHECK(h2o.n0[5] > 0.0);
    CHECK_THAT(h2o.n0[5], Catch::Matchers::WithinRel(0.987630, 1e-14));

    // GERG-2008 changed carbon monoxide and isopentane and added n-decane.
    CHECK(get_alphaig_coeffs(GERGModel::GERG_2004, "carbonmonoxide").theta0 != get_alphaig_coeffs(GERGModel::GERG_2008, "carbonmonoxide").theta0);
    CHECK_THAT(get_alphaig_coeffs(GERGModel::GERG_2008, "carbonmonoxide").theta0[4], Catch::Matchers::WithinRel(11.669802800, 1e-14));
    // Unchanged fluids fall through identically.
    CHECK(get_alphaig_coeffs(GERGModel::GERG_2004, "methane").n0 == get_alphaig_coeffs(GERGModel::GERG_2008, "methane").n0);
}

TEST_CASE("GERG ideal-gas integration constants zero h and s at the reference state", "[GERG]") {
    // teqp GERG.hpp:376-379 -- h = s = 0 for the IDEAL GAS at 298.15 K and
    // 101325 Pa, with rho0 = p0/(R*T0) (R, not R*).
    const double T0 = 298.15, p0 = 101325.0;
    const double rho0 = p0 / (R_GERG * T0);

    for (auto model : {GERGModel::GERG_2004, GERGModel::GERG_2008}) {
        for (const auto& name : component_names(model)) {
            CAPTURE(name);
            auto info = get_pure_info(model, name);
            auto c = get_alphaig_coeffs(model, name);

            // Aig10 = tau*d(alphaig)/d(tau) = -T*d(alphaig)/dT, which is
            // independent of the reducing temperature used to form tau.
            const double dT = 1e-5 * T0;
            double dalpha_dT = (gerg_alphaig_pure(c, info, T0 + dT, rho0) - gerg_alphaig_pure(c, info, T0 - dT, rho0)) / (2 * dT);
            double Aig10 = -T0 * dalpha_dT;

            double h_over_RT = 1 + Aig10;                                    // ideal gas: h/(RT) = 1 + Aig10
            double s_over_R = Aig10 - gerg_alphaig_pure(c, info, T0, rho0);  // s/R = Aig10 - Aig00

            CHECK_THAT(h_over_RT, Catch::Matchers::WithinAbs(0.0, 1e-8));
            CHECK_THAT(s_over_R, Catch::Matchers::WithinAbs(0.0, 1e-8));
        }
    }
}

TEST_CASE("GERG recomputed integration constants match an independent solve", "[GERG]") {
    // A finite difference can hide a small systematic error, so the values
    // below come from a completely independent implementation: a NumPy
    // script that rebuilds teqp's 2x2 system from scratch, solves it with
    // numpy.linalg.solve (LU with partial pivoting, not Cramer's rule) and
    // verifies h and s vanish using ANALYTIC derivatives.  See
    // task-4-report.md for the transcript.
    //
    // Corroboration that the reference state and the R*/R placement are both
    // right: for the two components whose ideal-gas constants GERG-2008
    // regenerated (carbon monoxide and hydrogen sulfide) this recomputation
    // reproduces the PUBLISHED n0[1] and n0[2] to ~1e-9, i.e. to the full
    // precision printed in the monograph.
    struct Expect
    {
        GERGModel model;
        const char* name;
        double n1, n2;
    };
    const Expect cases[] = {
      {GERGModel::GERG_2004, "methane", 19.597508817430203, -83.95966789022567},
      {GERGModel::GERG_2004, "water", 8.216535516334572, -12.002441239189446},
      {GERGModel::GERG_2004, "hydrogen", 13.79644339318705, -175.86448729341214},
      {GERGModel::GERG_2004, "argon", 8.316631499886697, -4.946502600476912},
      {GERGModel::GERG_2004, "carbonmonoxide", 10.814470255578003, -19.843695434544927},
      {GERGModel::GERG_2008, "carbonmonoxide", 10.813340744153283, -19.834733958634743},
      {GERGModel::GERG_2008, "n-decane", 50.35302335379572, -120.01206647981711},
    };
    for (const auto& e : cases) {
        CAPTURE(e.name);
        auto c = get_alphaig_coeffs(e.model, e.name);
        CHECK_THAT(c.n0[1], Catch::Matchers::WithinRel(e.n1, 1e-12));
        CHECK_THAT(c.n0[2], Catch::Matchers::WithinRel(e.n2, 1e-12));
    }

    // The published integration constants really are discarded: for the
    // heavier alkanes the monograph's own n0[1] refers to a different
    // reference state and differs by ~26 (not by roundoff).
    CHECK(std::abs(get_alphaig_coeffs(GERGModel::GERG_2004, "n-octane").n0[1] - 15.864709639) > 1.0);
}

#endif /* ENABLE_CATCH */
