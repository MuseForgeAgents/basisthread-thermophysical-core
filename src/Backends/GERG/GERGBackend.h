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

    /// Enforce make_gerg_fluid's EOS.limits.Tmin/Tmax (GERGBackend.cpp) on
    /// every update, regardless of input pair.  (Deliberately T only, not
    /// pmax too -- see check_gerg_range_of_validity's definition for why a
    /// pressure check does not belong here.)
    ///
    /// This is NOT redundant with CoolProp's ordinary flash machinery: for a
    /// pure fluid at PT_INPUTS with T above Tmax, FlashRoutines::PT_flash
    /// (FlashRoutines.cpp:298) determines the phase from p
    /// (T_phase_determination_pure_or_pseudopure), solves rho_Tp, and
    /// RETURNS -- nothing in that path compares the resulting T to
    /// EOS.limits.Tmax.  (T below Tmin happens to throw too, via
    /// solver_rho_Tp's liquid branch calling
    /// components[0].ancillaries.rhoL.evaluate(T) on an ancillary GERG fluids
    /// never populate -- an ACCIDENT of the missing ancillary, not a range
    /// check, and one that does not fire for the T-above-Tmax direction at
    /// all.)  Verified empirically while writing task-10's tests: without
    /// this override, `update(PT_INPUTS, 1e5, 900.0)` on GERG2008 methane
    /// (Tmax = 700 K) returns a state instead of throwing.
    ///
    /// `update` is the one AbstractState entry point every input pair
    /// (PT/DmolarT/HmolarP/...) funnels through, so checking _T here after
    /// the base class has finished the flash catches all of them, including
    /// pairs where T is SOLVED FOR rather than given directly.  Honors
    /// DONT_CHECK_PROPERTY_LIMITS, matching every other property-limit guard
    /// in HelmholtzEOSMixtureBackend.cpp (melting-line, Tmax_sat, ...), so
    /// `update` is overridden rather than post_update: the latter is not
    /// virtual (HelmholtzEOSMixtureBackend.h:67), so
    /// HelmholtzEOSMixtureBackend::update's unqualified `post_update()` call
    /// binds to the base implementation regardless of the dynamic type and a
    /// post_update override here would never run.
    void update(CoolProp::input_pairs input_pair, double value1, double value2) override;

    /// GERG-2004/GERG-2008 publish no transport correlations at all -- there is
    /// nothing "GERG" a viscosity number could be checked against, unlike the
    /// residual/ideal-gas/reducing terms above.  HelmholtzEOSMixtureBackend's
    /// inherited calc_viscosity()/calc_conductivity() would instead run
    /// CoolProp's own transport models (fit against different, possibly
    /// non-GERG, reference EOS) and return a plausible-looking number
    /// labelled GERG.  AbstractState::viscosity()/conductivity()/
    /// surface_tension() (AbstractState.cpp:793-813) cache the result but
    /// only ever populate that cache by calling calc_viscosity() etc, so
    /// overriding the calc_* hook is sufficient -- there is no separate
    /// caching path that could return a value without consulting it.
    ///
    /// Signatures copied verbatim from AbstractState.h:221,225,229 /
    /// HelmholtzEOSMixtureBackend.h:564,565,569 (return type, argument list,
    /// and the ABSENCE of a const qualifier all have to match or `override`
    /// silently declares an unrelated non-virtual function instead of
    /// overriding).
    CoolPropDbl calc_viscosity() override {
        throw NotImplementedError(
          "Transport properties are not part of the GERG-2004/GERG-2008 models. Use the HEOS backend if you want CoolProp's transport correlations.");
    }
    CoolPropDbl calc_conductivity() override {
        throw NotImplementedError(
          "Transport properties are not part of the GERG-2004/GERG-2008 models. Use the HEOS backend if you want CoolProp's transport correlations.");
    }
    CoolPropDbl calc_surface_tension() override {
        throw NotImplementedError("Surface tension is not part of the GERG-2004/GERG-2008 models.");
    }

   public:
    /// GERG's betas/gammas and departure functions are the published model,
    /// not a fit that a caller is meant to adjust -- set_mixture_parameters
    /// above exists specifically so this backend does NOT answer from
    /// CoolProp's mutable global BIP library.  Overriding
    /// set_mixture_parameters alone does not close this: AbstractState
    /// declares FOUR public mutator routes to the same underlying data --
    /// index-keyed and CAS-keyed double, index-keyed and CAS-keyed string
    /// (AbstractState.h:950-965) -- and HelmholtzEOSMixtureBackend overrides
    /// only the two INDEX-keyed ones (HelmholtzEOSMixtureBackend.h:217,223).
    /// The two CAS-keyed overloads are therefore inherited straight from
    /// AbstractState's default, which already throws NotImplementedError
    /// unconditionally ("GERG refuses binary-interaction mutation by every
    /// public route" pins that this stays true without a GERG-specific
    /// override of its own -- if HelmholtzEOSMixtureBackend ever grows a
    /// CAS-keyed override, that test starts failing instead of silently
    /// reopening the hole).  Only the two index-keyed overloads need a
    /// GERG-specific override here.
    ///
    /// apply_simple_mixing_rule (HelmholtzEOSMixtureBackend.cpp:383) is a
    /// FIFTH nominal route, but it is not overridden separately: it calls
    /// set_binary_interaction_double(i, j, ...) UNQUALIFIED on `this`, so
    /// virtual dispatch already sends that call to the override below
    /// whenever `this` is a GERGMixtureBackend.
    ///
    /// Known bypass NOT closed by this override, and not closeable at this
    /// layer: `Reducing` (HelmholtzEOSMixtureBackend.h:147) is a PUBLIC
    /// shared_ptr<ReducingFunction>.  A caller with a
    /// GERGMixtureBackend*/HelmholtzEOSMixtureBackend* can do
    /// `heos->Reducing->set_binary_interaction_double(i, j, param, value)`
    /// directly and mutate the GERG2008ReducingFunction in place, bypassing
    /// every guard here entirely.  Closing that would mean making `Reducing`
    /// (and `residual_helmholtz`, `SatL`, `SatV`, and most of this class's
    /// state -- see the `public:` section spanning
    /// HelmholtzEOSMixtureBackend.h:119-704) non-public on the shared base
    /// class, which is out of scope for a single backend.  Documented here
    /// as a known limitation per task-10-brief.md hazard 2, not hidden.
    void set_binary_interaction_double(const std::size_t, const std::size_t, const std::string&, const double) override {
        throw ValueError("GERG binary interaction parameters are fixed by the published model and cannot be modified. A mixture with altered "
                         "beta/gamma is not GERG.");
    }
    void set_binary_interaction_string(const std::size_t, const std::size_t, const std::string&, const std::string&) override {
        throw ValueError("GERG binary interaction parameters are fixed by the published model and cannot be modified.");
    }

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
    /// Throw if the current state's T is outside make_gerg_fluid's published
    /// range.  See the `update` override above for why this exists as a
    /// check here rather than relying on the inherited flash machinery, and
    /// for why it is T-only.
    void check_gerg_range_of_validity();

    /// Build directly from already-assembled GERG CoolPropFluid objects.
    /// Used by set_components/get_copy to make the linked states; private
    /// because a caller outside this class has no way to obtain GERG fluids
    /// that were not built by make_gerg_fluid.
    GERGMixtureBackend(GERGModel model, const std::vector<CoolPropFluid>& fluids, bool generate_SatL_and_SatV);

    GERGModel m_model;
};

} /* namespace CoolProp */
#endif /* GERGBACKEND_H_ */
