#if defined(ENABLE_CATCH)
#    include <catch2/catch_all.hpp>
#    include <catch2/matchers/catch_matchers_floating_point.hpp>

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

#endif /* ENABLE_CATCH */
