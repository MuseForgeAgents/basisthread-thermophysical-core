#ifndef GERGDATA_H_
#define GERGDATA_H_

// Coefficient tables for the GERG-2004 and GERG-2008 equations of state.
//
// Transcribed from teqp (https://github.com/usnistgov/teqp),
// include/teqp/models/GERG/GERG.hpp, which is the reference implementation
// this backend is validated against.  Original data: Kunz, Klimeck, Wagner,
// Jaeschke, "The GERG-2004 Wide-Range Equation of State for Natural Gases and
// Other Mixtures", GERG TM15 (2007), and Kunz & Wagner, J. Chem. Eng. Data 57
// (2012) 3032-3091.
//
// This header is PRIVATE to the GERG backend.  It is not installed and must
// not be included from include/CoolProp/.

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "CoolProp/Exceptions.h"
#include "CoolProp/detail/strings.h"

namespace CoolProp {
namespace GERG {

/// Which GERG model year this backend instance represents.
enum class GERGModel
{
    GERG_2004,
    GERG_2008
};

struct PureInfo
{
    double rhoc_molm3;  ///< Reducing density, mol/m^3
    double Tc_K;        ///< Reducing temperature, K
    double M_kgmol;     ///< Molar mass, kg/mol
};

namespace detail {

/// Table A3.5, GERG-2004 monograph.  Tabulated in mol/dm^3, K, kg/kmol;
/// converted to mol/m^3, K, kg/mol on the way out of get_pure_info.
/// Transcribed from teqp GERG.hpp:438-457 (GERG2004::get_pure_info data_map).
inline const std::map<std::string, PureInfo>& pure_info_2004() {
    static const std::map<std::string, PureInfo> data = {
      {"methane", {10.139342719, 190.564000000, 16.042460}},
      {"nitrogen", {11.183900000, 126.192000000, 28.013400}},
      {"carbondioxide", {10.624978698, 304.128200000, 44.009500}},
      {"ethane", {6.870854540, 305.322000000, 30.069040}},
      {"propane", {5.000043088, 369.825000000, 44.095620}},
      {"n-butane", {3.920016792, 425.125000000, 58.122200}},
      {"isobutane", {3.860142940, 407.817000000, 58.122200}},
      {"n-pentane", {3.215577588, 469.700000000, 72.148780}},
      {"isopentane", {3.271018581, 460.350000000, 72.148780}},
      {"n-hexane", {2.705877875, 507.820000000, 86.175360}},
      {"n-heptane", {2.315324434, 540.130000000, 100.201940}},
      {"n-octane", {2.056404127, 569.320000000, 114.228520}},
      {"hydrogen", {14.940000000, 33.190000000, 2.015880}},
      {"oxygen", {13.630000000, 154.595000000, 31.998800}},
      {"carbonmonoxide", {10.850000000, 132.800000000, 28.010100}},
      {"water", {17.873716090, 647.096000000, 18.015280}},
      {"helium", {17.399000000, 5.195300000, 4.002602}},
      {"argon", {13.407429659, 150.687, 39.948000}}};
    return data;
}

/// Entries that GERG-2008 changes or adds relative to GERG-2004.  Everything
/// else falls through to pure_info_2004().
/// Transcribed from teqp GERG.hpp:980-985 (GERG2008::get_pure_info data_map).
inline const std::map<std::string, PureInfo>& pure_info_2008_overrides() {
    static const std::map<std::string, PureInfo> data = {
      {"carbonmonoxide", {10.85, 132.86, 28.010100}},  // changed from GERG-2004
      {"isopentane", {3.271, 460.35, 72.148780}},      // changed from GERG-2004
      {"n-nonane", {1.81, 594.55, 128.2551}},          // new in GERG-2008
      {"n-decane", {1.64, 617.7, 142.28168}},          // new in GERG-2008
      {"hydrogensulfide", {10.19, 373.1, 34.08088}}    // new in GERG-2008
    };
    return data;
}

/// CAS number -> GERG component name.  Used by resolve_component so that
/// CoolProp aliases (CO2, R744, 124-38-9, ...) reach the right component.
/// This table does not exist in teqp; CAS numbers verified against this
/// CoolProp build's fluid library (see task-2-report.md for the verification
/// transcript).
inline const std::map<std::string, std::string>& cas_to_gerg() {
    static const std::map<std::string, std::string> data = {
      {"74-82-8", "methane"},        {"7727-37-9", "nitrogen"},        {"124-38-9", "carbondioxide"},
      {"74-84-0", "ethane"},         {"74-98-6", "propane"},           {"106-97-8", "n-butane"},
      {"75-28-5", "isobutane"},      {"109-66-0", "n-pentane"},        {"78-78-4", "isopentane"},
      {"110-54-3", "n-hexane"},      {"142-82-5", "n-heptane"},        {"111-65-9", "n-octane"},
      {"1333-74-0", "hydrogen"},     {"7782-44-7", "oxygen"},          {"630-08-0", "carbonmonoxide"},
      {"7732-18-5", "water"},        {"7440-59-7", "helium"},          {"7440-37-1", "argon"},
      {"7783-06-4", "hydrogensulfide"}, {"111-84-2", "n-nonane"},      {"124-18-5", "n-decane"}};
    return data;
}

}  // namespace detail

/// GERG2004::component_names, teqp GERG.hpp:431.
inline const std::vector<std::string>& component_names(GERGModel model) {
    static const std::vector<std::string> names_2004 = {
      "methane", "nitrogen", "carbondioxide", "ethane", "propane", "n-butane", "isobutane", "n-pentane", "isopentane",
      "n-hexane", "n-heptane", "n-octane", "hydrogen", "oxygen", "carbonmonoxide", "water", "helium", "argon"};
    // GERG2008::component_names, teqp GERG.hpp:973, is names_2004 plus these three.
    static const std::vector<std::string> names_2008 = [] {
        std::vector<std::string> v = names_2004;
        v.insert(v.end(), {"hydrogensulfide", "n-nonane", "n-decane"});
        return v;
    }();
    return (model == GERGModel::GERG_2004) ? names_2004 : names_2008;
}

inline PureInfo get_pure_info(GERGModel model, const std::string& gerg_name) {
    PureInfo data{};
    bool found = false;
    if (model == GERGModel::GERG_2008) {
        const auto& ov = detail::pure_info_2008_overrides();
        auto it = ov.find(gerg_name);
        if (it != ov.end()) {
            data = it->second;
            found = true;
        }
    }
    if (!found) {
        // GERG-2004 does not contain the three fluids added in GERG-2008.
        const auto& names = component_names(model);
        if (std::find(names.begin(), names.end(), gerg_name) == names.end()) {
            throw ValueError(format("[%s] is not a component of this GERG model", gerg_name.c_str()));
        }
        const auto& base = detail::pure_info_2004();
        auto it = base.find(gerg_name);
        if (it == base.end()) {
            throw ValueError(format("Unable to load GERG pure info for [%s]", gerg_name.c_str()));
        }
        data = it->second;
    }
    data.rhoc_molm3 *= 1000;  // mol/dm^3 -> mol/m^3
    data.M_kgmol /= 1000;     // kg/kmol -> kg/mol
    return data;
}

/// Resolve a CoolProp fluid name/alias/CAS to the corresponding GERG
/// component name, throwing ValueError if it cannot be resolved or if it
/// falls outside the given model's published component set.  Defined in
/// GERGBackend.cpp because it needs get_fluid_param_string(), which would
/// otherwise pull all of CoolProp.h into this data-only header.
std::string resolve_component(GERGModel model, const std::string& user_name);

}  // namespace GERG
}  // namespace CoolProp
#endif /* GERGDATA_H_ */
