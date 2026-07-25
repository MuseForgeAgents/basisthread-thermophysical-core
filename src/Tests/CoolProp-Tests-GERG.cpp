#if defined(ENABLE_CATCH)
#include <catch2/catch_all.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "CoolProp/AbstractState.h"
#include "CoolProp/DataStructures.h"
#include "CoolProp/Exceptions.h"

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

#include "../Backends/GERG/GERGData.h"

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

#endif /* ENABLE_CATCH */
