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

    /// Copy this state.  Overridden so that TPD_state / critical_state /
    /// transient_pure_state (HelmholtzEOSMixtureBackend.h:84, 92, 100, which
    /// call get_copy UNqualified) are GERG-typed and therefore reach the
    /// set_mixture_parameters override below.  A plain
    /// HelmholtzEOSMixtureBackend copy of a GERG mixture would run the default
    /// binary-pair lookup instead.
    HelmholtzEOSMixtureBackend* get_copy(bool generate_SatL_and_SatV = true) override;

   protected:
    /// R = 8.314472 J/mol/K, ALWAYS -- for mixtures as well as pure fluids.
    ///
    /// This is not redundant with `EquationOfState::R_u`.
    /// HelmholtzEOSMixtureBackend::calc_gas_constant returns the component's
    /// own R only when the state is pure; for a MIXTURE it returns
    /// `get_config_double(R_U_CODATA)` (8.31446261815324) whenever the global
    /// NORMALIZE_GAS_CONSTANTS config flag is set, which is CoolProp's default
    /// (HelmholtzEOSMixtureBackend.cpp:599-606).  That silently rescales p, the
    /// ideal-gas R_i/R_mix ratios in calc_alpha0_deriv_nocache, c_v and w by
    /// ~1.1e-6 relative -- large enough to be wrong, small enough to look like
    /// a plausible answer -- while leaving alpha^r untouched.  GERG defines its
    /// own R, so the backend must not be at the mercy of a global setting.
    CoolPropDbl calc_gas_constant() override;

    /// Build the reducing function and excess (departure) term from the
    /// published GERG tables ONLY.  Overriding this is the whole point of the
    /// backend: the inherited implementation resolves each binary pair through
    /// CoolProp's global BIP library.  All 210 GERG-2008 pairs are in that
    /// library, but 16 of them carry a LATER refit (15 Gernert-Thesis-2013,
    /// 1 Tkaczuk-JPCRD-2020) instead of the Kunz-JCED-2012 row GERG publishes
    /// -- and the departure function attached to a pair can differ even where
    /// the betas and gammas agree.  Reaching the inherited path would answer
    /// with those, silently, under a backend named GERG2004/GERG2008.
    void set_mixture_parameters() override;

    /// Overridden so the linked SatL/SatV states are GERGMixtureBackend
    /// objects.  The base implementation builds them with an explicitly
    /// qualified `HelmholtzEOSMixtureBackend::get_copy(false)` call
    /// (HelmholtzEOSMixtureBackend.cpp:142, :146), which constructs a
    /// base-class object whose own set_components would run the INHERITED
    /// set_mixture_parameters -- i.e. it would either throw (GERG fluids carry
    /// no CAS) or, worse, silently answer from the global BIP library.
    void set_components(const std::vector<CoolPropFluid>& components, bool generate_SatL_and_SatV = true) override;

   private:
    /// Build directly from already-assembled GERG CoolPropFluid objects.
    /// Used by set_components/get_copy to make the linked states; private
    /// because a caller outside this class has no way to obtain GERG fluids
    /// that were not built by make_gerg_fluid.
    GERGMixtureBackend(GERGModel model, const std::vector<CoolPropFluid>& fluids, bool generate_SatL_and_SatV);

    GERGModel m_model;
};

} /* namespace CoolProp */
#endif /* GERGBACKEND_H_ */
