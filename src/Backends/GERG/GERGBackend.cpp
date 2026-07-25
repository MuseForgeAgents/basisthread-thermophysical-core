#include "GERGBackend.h"

#include <algorithm>

#include "CoolProp/CoolProp.h"
#include "CoolProp/Exceptions.h"
#include "GERGData.h"

namespace CoolProp {

GERGMixtureBackend::GERGMixtureBackend(GERGModel model, const std::vector<std::string>& names) : m_model(model) {
    (void)names;
    throw NotImplementedError("GERG backend is not yet implemented");
}

namespace GERG {

std::string resolve_component(GERGModel model, const std::string& user_name) {
    // Resolve through CoolProp's normal alias/CAS machinery first, so users
    // can spell components the way they do everywhere else in CoolProp.
    std::string cas;
    try {
        cas = get_fluid_param_string(user_name, "CAS");
    } catch (const std::exception&) {
        throw ValueError(format("[%s] is not a fluid CoolProp recognises, so it cannot be a GERG component", user_name.c_str()));
    }
    const auto& table = detail::cas_to_gerg();
    auto it = table.find(cas);
    if (it == table.end()) {
        throw ValueError(format("[%s] (CAS %s) is not a component of this GERG model", user_name.c_str(), cas.c_str()));
    }
    const std::string& gerg_name = it->second;
    const auto& names = component_names(model);
    if (std::find(names.begin(), names.end(), gerg_name) == names.end()) {
        throw ValueError(format("[%s] is a GERG-2008 component but not a GERG-2004 component", user_name.c_str()));
    }
    return gerg_name;
}

}  // namespace GERG

class GERG2004Generator : public AbstractStateGenerator
{
   public:
    AbstractState* get_AbstractState(const std::vector<std::string>& fluid_names) override {
        return new GERGMixtureBackend(GERGModel::GERG_2004, fluid_names);
    }
};
// This static initialization will cause the generator to register
// NOLINTNEXTLINE(cert-err58-cpp)
static GeneratorInitializer<GERG2004Generator> gerg2004_gen(GERG2004_BACKEND_FAMILY);

class GERG2008Generator : public AbstractStateGenerator
{
   public:
    AbstractState* get_AbstractState(const std::vector<std::string>& fluid_names) override {
        return new GERGMixtureBackend(GERGModel::GERG_2008, fluid_names);
    }
};
// This static initialization will cause the generator to register
// NOLINTNEXTLINE(cert-err58-cpp)
static GeneratorInitializer<GERG2008Generator> gerg2008_gen(GERG2008_BACKEND_FAMILY);

} /* namespace CoolProp */
