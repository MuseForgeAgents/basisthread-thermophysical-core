#ifndef GERGBACKEND_H_
#define GERGBACKEND_H_

#include <string>
#include <vector>

#include "../Helmholtz/HelmholtzEOSMixtureBackend.h"
#include "CoolProp/CoolPropFluid.h"
#include "CoolProp/DataStructures.h"
#include "GERGData.h"

namespace CoolProp {

/// Which GERG model year this backend instance represents.  Defined in
/// GERGData.h (CoolProp::GERG::GERGModel); aliased here so backend code can
/// keep referring to it as CoolProp::GERGModel.
using GERGModel = GERG::GERGModel;

namespace GERG {

/// Assemble a CoolPropFluid carrying ONLY the published GERG parameters for
/// one component: the residual Helmholtz terms of Table A3.2, the ideal-gas
/// terms of Table A3.1, the reducing state of Table A3.5, and R = 8.314472
/// J/mol/K.  Nothing is taken from CoolProp's own fluid library -- the name is
/// used purely as a table key -- so a GERG backend can never silently mix a
/// CoolProp EOS into a GERG calculation.
///
/// The ideal-gas wiring is the delicate part; see the long comment on the
/// definition in GERGBackend.cpp for the sign, R*/R and Tc-vs-T_red contract.
///
/// @param model      Which GERG model's tables to read
/// @param gerg_name  A GERG component name (as returned by resolve_component)
CoolPropFluid make_gerg_fluid(GERGModel model, const std::string& gerg_name);

}  // namespace GERG

/**
 * \brief Strict GERG-2004 / GERG-2008 backend.
 *
 * Admits only the components published with the selected model (18 for
 * GERG-2004, 21 for GERG-2008) and uses only parameters published with it.
 * See docs/superpowers/specs/2026-07-25-gerg-strict-backend-design.md.
 */
class GERGMixtureBackend : public HelmholtzEOSMixtureBackend
{
   public:
    GERGMixtureBackend(GERGModel model, const std::vector<std::string>& names);
    ~GERGMixtureBackend() override = default;

    std::string backend_name() override {
        return get_backend_string(m_model == GERGModel::GERG_2004 ? GERG2004_BACKEND : GERG2008_BACKEND);
    }

    GERGModel model() const {
        return m_model;
    }

   private:
    GERGModel m_model;
};

} /* namespace CoolProp */
#endif /* GERGBACKEND_H_ */
