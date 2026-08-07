.. _gerg_backend:

*********************************************
GERG-2004 and GERG-2008 Equations of State
*********************************************

.. contents:: :depth: 2

Introduction
============

CoolProp provides two *strict* implementations of the GERG wide-range equations
of state for natural gases and other mixtures: ``GERG2004``
:cite:`Kunz-BOOK-2007` and ``GERG2008`` :cite:`Kunz-JCED-2012`.  They are
separate backend families, not options on the default multi-fluid backend.

"Strict" means exactly what it says:

* only the components each model publishes are admitted,
* only the parameters published with each model are carried — the pure-fluid
  equations of state, the ideal-gas coefficients, the binary reducing
  parameters, and the departure functions all come from the model's own
  tables rather than from CoolProp's fluid and mixture libraries,
* the gas constant is GERG's :math:`R = 8.314472\ \mathrm{J\,mol^{-1}\,K^{-1}}`,
  not the CODATA value CoolProp normally uses, and
* anything the model does not cover raises an exception instead of quietly
  answering from somewhere else.

A number obtained from the ``GERG2008`` backend is a GERG-2008 number: no
parameter in it is borrowed from another correlation.  The refusals are
model-level rules enforced through the documented API, not a sandbox — see
*The strictness rules are model-level, not C++-level* below.

Usage
=====

Change the backend name, exactly as for the other backends.  In the
:ref:`high-level interface <high_level_api>`::

    PropsSI("Dmolar", "T", 300, "P", 1e6, "GERG2008::METHANE[0.9]&NITROGEN[0.1]")
    # -> 406.9418737 mol/m^3

and in the :ref:`low-level interface <low_level_api>`::

    AS = CP.AbstractState("GERG2008", "Methane&Nitrogen")
    AS.set_mole_fractions([0.9, 0.1])
    AS.update(CP.PT_INPUTS, 1e6, 300)
    AS.rhomolar()   # -> 406.9418737 mol/m^3

Pure fluids work the same way — ``GERG2008::Methane`` is the GERG-2008
pure-methane equation of state, which is *not* the Setzmann-Wagner reference
equation CoolProp's ``HEOS::Methane`` uses.

``GERG2004`` behaves identically apart from its smaller component set and the
handful of parameters GERG-2008 revised.

Components
==========

GERG-2004 defines 18 components; GERG-2008 keeps all 18 and adds three more,
for 21.  GERG-2008 also revises the pure-fluid equation of state for carbon
monoxide and isopentane, so those two give different numbers under the two
models.

Component names resolve through CoolProp's normal alias and CAS machinery, so
``CO2``, ``R744``, ``CarbonDioxide`` and ``124-38-9`` all reach the same GERG
component.  The canonical CoolProp names are listed below.

============================  =========  =========  ==================
CoolProp name                 GERG-2004  GERG-2008  CAS
============================  =========  =========  ==================
``Methane``                   yes        yes        74-82-8
``Nitrogen``                  yes        yes        7727-37-9
``CarbonDioxide``             yes        yes        124-38-9
``Ethane``                    yes        yes        74-84-0
``n-Propane``                 yes        yes        74-98-6
``n-Butane``                  yes        yes        106-97-8
``IsoButane``                 yes        yes        75-28-5
``n-Pentane``                 yes        yes        109-66-0
``Isopentane``                yes        yes [1]_   78-78-4
``n-Hexane``                  yes        yes        110-54-3
``n-Heptane``                 yes        yes        142-82-5
``n-Octane``                  yes        yes        111-65-9
``Hydrogen``                  yes        yes        1333-74-0
``Oxygen``                    yes        yes        7782-44-7
``CarbonMonoxide``            yes        yes [1]_   630-08-0
``Water``                     yes        yes        7732-18-5
``Helium``                    yes        yes        7440-59-7
``Argon``                     yes        yes        7440-37-1
``HydrogenSulfide``           no         yes        7783-06-4
``n-Nonane``                  no         yes        111-84-2
``n-Decane``                  no         yes        124-18-5
============================  =========  =========  ==================

.. [1] GERG-2008 replaces the GERG-2004 pure-fluid equation of state for this
   component.  The component itself is present in both models.

Reducing parameters (:math:`\beta`, :math:`\gamma`) exist for **every** binary
pair — 153 for GERG-2004 and 210 for GERG-2008 — so no pair of components is
unsupported.  Fifteen of the GERG-2008 pairs additionally carry a departure
function; the rest take :math:`F_{ij} = 0` and contribute no departure term.

Requesting a fluid outside the model's set raises ``ValueError``::

    AbstractState("GERG2008", "R134a")
    # ValueError: [R134a] (CAS 811-97-2) is not a component of this GERG model

    AbstractState("GERG2004", "n-Decane")
    # ValueError: [n-Decane] is a GERG-2008 component but not a GERG-2004 component

Range of validity
=================

Kunz & Wagner give two ranges for the mixture model:

===============  ==========================  ===================
Range            Temperature                 Pressure
===============  ==========================  ===================
Normal           90 K to 450 K               :math:`p \le` 35 MPa
Extended         60 K to 700 K               :math:`p \le` 70 MPa
===============  ==========================  ===================

**The backend enforces the extended range**, not the normal one.  Inside the
normal range the model's uncertainties are the ones quoted in the publications;
between the normal and the extended limits the model still evaluates but with
larger (and less well characterised) uncertainty.  Choosing the extended range
as the enforced one means the backend does not refuse to answer questions the
authors consider answerable; it does not mean every answer inside it carries
the headline accuracy.

A temperature outside the enforced range raises ``OutOfRangeError``::

    AS.update(CP.PT_INPUTS, 1e6, 800)
    # OutOfRangeError: Temperature [800 K] is outside the GERG range of validity [60, 700] K

Two details of the enforcement are worth knowing.

**The check is on temperature only.**  Pressure is not checked, deliberately.
The 70 MPa ceiling is an operating-envelope statement about the mixture model,
whereas the underlying single-phase equation of state is legitimately evaluated
at :math:`(T, \rho)` points whose pressure is far outside it — including inside
the two-phase dome, where the single-phase equation returns a very large or
negative pressure by construction.  A pressure check cannot distinguish those
two cases, so there is none.  Like every other property-limit guard in
CoolProp, the temperature check is skipped when the
``DONT_CHECK_PROPERTY_LIMITS`` configuration flag is set.

**For a mixture, the enforced limits are mole-fraction-weighted averages of the
per-component limits.**  This is CoolProp's standard behaviour for
``Tmin``/``Tmax``/``pmax`` on a mixture, and it applies here too.  Each GERG
component carries ``Tmin = min(60 K, T_c)`` — the cap exists because taking 60 K
literally would put ``Tmin`` *above* the reducing temperature for helium
(5.1953 K) and hydrogen (33.19 K), which is self-contradictory.  A consequence
is that a helium-rich mixture reports a ``Tmin`` below 60 K: for
90 % helium / 10 % methane, ``Tmin`` is
:math:`0.9 \times 5.1953 + 0.1 \times 60 = 10.68` K.  That number is an
artefact of the averaging rule, **not** a claim that GERG is valid at 10.68 K.
The authoritative range is the mixture-model range in the table above.

What these backends deliberately do not provide
===============================================

Each of the following is a deliberate refusal, not a gap waiting to be filled
in.  In every case the alternative would be to return a plausible number
computed from something that is not GERG, labelled as GERG.

Transport properties
--------------------

Viscosity, thermal conductivity and surface tension are **not part of either
GERG model**.  CoolProp has correlations for all three, and they are perfectly
good numbers — from a different model.  Returning them through a backend named
``GERG2008`` invites misattribution, so they throw::

    AS.viscosity()
    # NotImplementedError: Transport properties are not part of the
    # GERG-2004/GERG-2008 models. Use the HEOS backend if you want
    # CoolProp's transport correlations.

    AS.surface_tension()
    # NotImplementedError: Surface tension is not part of the
    # GERG-2004/GERG-2008 models.

If you need transport properties for a natural gas, use the ``HEOS`` backend
(or REFPROP) and be explicit in your reporting that the transport numbers come
from a different source than the thermodynamic ones.

Superancillaries
----------------

GERG fluids carry no :doc:`superancillary </coolprop/SuperAncillary>` Chebyshev
expansions.  This one is load-bearing, because the obvious shortcut is a silent
correctness bug: CoolProp's saturation path returns a superancillary value as
*the answer*, not as an iteration guess.  Attaching CoolProp's existing
methane superancillary to a GERG methane fluid would therefore return
Setzmann-Wagner saturation densities labelled GERG-2008, with no warning and no
iteration to correct them.

Genuine superancillaries would have to be fitted against the 23 distinct GERG
pure equations of state.  That is a reasonable follow-on and purely additive,
but it has not been done.  Until it is, pure-fluid saturation goes through the
classical ancillary-seeded VLE solver, which converges to GERG-consistent
values — correct, just slower than the superancillary path.

Mutable binary interaction parameters
-------------------------------------

``set_binary_interaction_double`` and ``set_binary_interaction_string`` throw::

    AS.set_binary_interaction_double(0, 1, "betaT", 1.0)
    # ValueError: GERG binary interaction parameters are fixed by the published
    # model and cannot be modified. A mixture with altered beta/gamma is not GERG.

A mixture whose :math:`\beta` or :math:`\gamma` has been altered is not GERG,
so the setters refuse rather than producing a mutant model that still answers
to the name.  If you want to adjust interaction parameters, that is what the
``HEOS`` backend is for.

Known limitations
=================

These are real and current.  They are stated here rather than discovered later.

Mixture saturation, phase envelopes and VLE flashes do not work
---------------------------------------------------------------

**Pure-fluid** saturation works: ``QT``, ``PQ`` and ``DQ`` inputs on
``GERG2008::Methane`` (and every other GERG pure with a fitted ancillary)
converge normally.

**Mixture** saturation does not.  A ``QT``, ``PQ`` or ``DQ`` flash on a GERG
mixture, and ``build_phase_envelope()`` on a GERG mixture, currently fail::

    AS = CP.AbstractState("GERG2008", "Methane&Ethane")
    AS.set_mole_fractions([0.9, 0.1])
    AS.update(CP.QT_INPUTS, 0.0, 150)
    # ValueError: solver_rho_Tp was unable to find a solution for T=150, p=0,
    # with guess value nan with error: p is not a valid number

    AS.build_phase_envelope("")
    # ValueError: Residual function in secant returned invalid number

The cause is that GERG publishes no acentric factor, so the GERG fluids carry
none.  CoolProp's mixture VLE machinery seeds itself with Wilson K-factors and
an SRK density estimate, both of which read the acentric factor; with it unset
the initial guess is NaN and the solver has nothing to iterate from.  This is a
missing input to the *initial guess*, not a defect in the GERG equation of
state itself — single-phase properties, which do not use that path, are
unaffected and are validated against teqp to 1e-10.

Until this is addressed, use GERG for single-phase mixture properties and for
pure-fluid saturation.  For mixture phase equilibria, use ``HEOS`` or REFPROP.

Compositions with two or more exactly-zero mole fractions
----------------------------------------------------------

The natural way to hand over a natural-gas analysis is the full 21-name
GERG-2008 component list with a mole fraction for each, most of them exactly
zero.  Until this release *every* such composition returned NaN for *every*
property, silently.  It is now partly fixed, and the split matters:

**Works.**  The reducing state and everything that flows from it:
:math:`T_r`, :math:`\rho_r`, :math:`\alpha^0`, :math:`\alpha^r`, :math:`p`,
:math:`\rho`, :math:`c_v`, :math:`c_p`, speed of sound, :math:`h`, :math:`s`,
molar mass.  These are now identical — to the last digit — to the same
composition with the zero components trimmed away.

**Still returns NaN, with no error raised.**  The composition derivatives
:math:`\partial T_r / \partial x_i` and above, and therefore
``fugacity()`` and ``fugacity_coefficient()``::

    AS = CP.AbstractState("GERG2008", "Methane&Nitrogen&Ethane&Propane")
    AS.set_mole_fractions([0.9, 0.1, 0.0, 0.0])
    AS.update(CP.PT_INPUTS, 1e6, 300)
    AS.rhomolar()                 # 406.94...   correct
    AS.fugacity_coefficient(0)    # nan         no error raised

The guard that was added covers the reducing function's ``f_Y_ij`` and its two
first-derivative helpers.  CoolProp's ``XN_DEPENDENT`` composition-derivative
formulation — the one the fugacity API uses — does not call those helpers; it
inlines the same :math:`0/0` expression, and it is still unguarded.  The
trigger is ``x[N-1] == 0`` together with at least one other exactly-zero mole
fraction.

**This is not GERG-specific.**  It is identical on the default ``HEOS``
backend, and predates these backends entirely.  It is tracked as
`GitHub #1677 <https://github.com/CoolProp/CoolProp/issues/1677>`_.

Until it is fixed, if you need fugacities, **trim the zero-mole-fraction
components out of the composition** rather than passing the full component
list.  The trimmed result is exact.

Phase envelopes and flashes fail on such compositions too, but for the
separate reason described in the previous section — they fail for GERG
mixtures generally, zeros or not.  For reference:

* a ``HEOS`` phase envelope throws
  ``Unable to calculate at least 4 points in phase envelope; quitting``,
* a ``GERG2008`` phase envelope or ``PQ`` flash throws
  ``Residual function in secant returned invalid number``.

Saturation states below the enforced ``Tmin``
----------------------------------------------

Seven components have a fitted saturation ancillary whose low-temperature end
lies *below* the enforced ``Tmin``: methane (57.17 K vs 60 K), nitrogen
(37.86 K), oxygen (46.41 K), carbon monoxide (39.86 K), argon (45.24 K),
hydrogen (9.96 K vs 33.19 K) and helium (1.56 K vs 5.20 K).
``get_state("triple_liquid")`` — and, from C++, ``calc_Tmin_sat()`` /
``calc_pmin_sat()`` on the Helmholtz backend — therefore report a state that
``update()`` will refuse to evaluate::

    AS = CP.AbstractState("GERG2008", "Methane")
    AS.update(CP.QT_INPUTS, 0.0, 58.0)
    # OutOfRangeError: Temperature [58 K] is outside the GERG range of validity [60, 700] K

The ancillary data below ``Tmin`` is real and was traced with teqp; it is
simply not reachable through the public API, because the model's own range of
validity stops first.

Properties GERG does not define at all
---------------------------------------

GERG publishes no acentric factor and no triple point.  ``acentric_factor()``
throws ``NotImplementedError`` rather than returning the internal sentinel.
(``PropsSI("acentric", ...)`` still returns ``inf``, because ``PropsSI``
converts every exception into ``_HUGE`` plus an ``errstring``; the throw is
visible through the low-level interface.)  ``Ttriple()`` returns 0 and
``get_state("triple_liquid")`` returns the low-temperature end of the fitted
saturation curve under a name CoolProp inherited — neither is a GERG triple
point, because there is no such thing in these models.

``set_reference_stateS`` is not available
------------------------------------------

It throws ``NotImplementedError`` on these backends::

    CP.set_reference_state("GERG2008::Methane", "NBP")
    # NotImplementedError: set_reference_stateS is not implemented for the
    # GERG2008 backend. ...

CoolProp applies a reference-state change by writing an offset into the global
fluid-library entry for the fluid, and the GERG backends do not read that
library.  Before this was made explicit the call was a **silent no-op** — it
returned without error and without effect, and did not even validate the
reference-state string.  See *Reference state* below for what to do instead.

The strictness rules are model-level, not C++-level
----------------------------------------------------

The throws listed under *What these backends deliberately do not provide*
cover the documented API.  They are not a sandbox.  From C++ it remains
possible to reach past them — for example ``Reducing->set_binary_interaction_double(...)``
on the reducing-function object, direct assignment into
``residual_helmholtz->Excess.F[i][j]``, or ``update_DmolarT_direct()``, which
bypasses the range check by design because it is what the backend uses to
build its own fluids.  These are documented in ``GERGBackend.h``.  The
strictness rules exist to stop a *plausible mistake*, not a determined one.

Tabular backends wrapping GERG
-------------------------------

``BICUBIC&GERG2008`` and ``TTSE&GERG2008`` are **not supported**.  They do not
fail loudly, which is the problem: the table build completes without error and
persists a cache under ``~/.CoolProp/Tables/GERG2008Backend(...)``, and then
every lookup inside the model's own range is rejected::

    AS = CP.AbstractState("BICUBIC&GERG2008", "Methane")
    AS.update(CP.PT_INPUTS, 1e6, 300)
    # ValueError: inputs are not in range, p=1e+06 Pa, T=300 K

Delete the cache directory if you have created one.  Use the ``GERG2008``
backend directly.

Helium and hydrogen have no reachable saturation state
-------------------------------------------------------

Because each component's ``Tmin`` is capped at its own reducing temperature
(see *Range of validity* above), helium and hydrogen end up with
``Tmin == T_c`` — 5.1953 K and 33.19 K respectively.  Their entire subcritical
saturation curve is therefore below the enforced lower temperature limit, and
any saturation call on them raises ``OutOfRangeError``::

    AS = CP.AbstractState("GERG2008", "Helium")
    AS.update(CP.QT_INPUTS, 0.0, 4.67577)
    # OutOfRangeError: Temperature [4.67577 K] is outside the GERG range of
    # validity [5.1953, 700] K

This is consistent with the model: GERG's published lower limit is 60 K, so
neither fluid's saturation curve is inside the model's range in the first
place.  Both components exist in GERG because they appear as dilute
constituents of natural gas, not because GERG is a helium or hydrogen
saturation model.

Reference state
---------------

.. warning::

   These backends report :math:`h = 0` and :math:`s = 0` for the **ideal gas**
   at 298.15 K and 101325 Pa, per component.

That is not any of CoolProp's usual reference states (IIR, ASHRAE, NBP), and it
is the *ideal-gas* value at that state, not the real-fluid value.  The
published integration constants are deliberately discarded and recomputed to
satisfy it, exactly as teqp does; without that recomputation :math:`h` and
:math:`s` would not match teqp even though :math:`p`, :math:`c_v` and the speed
of sound did.

The practical consequence: **enthalpy and entropy from a GERG backend differ
from the same property from ``HEOS`` by a large offset.**  For methane at
101325 Pa,

===========  ============================  ============================
:math:`T`    :math:`h_{GERG} - h_{HEOS}`   :math:`s_{GERG} - s_{HEOS}`
===========  ============================  ============================
300 K        -14614.05 J/mol               -107.1158 J/mol/K
400 K        -14614.26 J/mol               -107.1164 J/mol/K
500 K        -14613.81 J/mol               -107.1154 J/mol/K
===========  ============================  ============================

This is correct and it matches teqp.  The offset is essentially constant — it
varies by about 3 parts in :math:`10^5` across the table above, which is the
genuine difference between the two models' ideal-gas and residual parts, not a
reference-state effect.

**Any cross-check of a GERG backend against ``HEOS`` must compare *differences*
in** :math:`h` **and** :math:`s` **, never absolute values.**  Comparing
absolute enthalpies will make a correct implementation look catastrophically
wrong.  ``set_reference_stateS`` is **not** available as an escape hatch here
(see above) — subtract your own offset, or take a single reference point from
each backend and compare everything relative to it.

Relationship to CoolProp's default HEOS mixture model
=====================================================

CoolProp's default multi-fluid (``HEOS``) backend descends from the same
Kunz & Wagner formulation, so it is structurally GERG-shaped.  It is
nevertheless a **different model** and gives different numbers.  Two independent
reasons:

**Pure-fluid equations of state.**  This is the load-bearing one.  ``HEOS``
ships the *reference* equation of state for each fluid — Setzmann-Wagner for
methane, Span-Wagner for carbon dioxide, and so on.  GERG uses its own
shortened technical form, 12 to 24 terms with a largely shared exponent set.
These are different equations producing different numbers.

**Binary interaction parameters.**  All 210 GERG-2008 binary pairs are present
in CoolProp's binary-pair library, and 194 of them are the GERG values.  The
other **16 are later refits** — 15 from Gernert's thesis and 1 from
Tkaczuk et al. (2020) — which is exactly what a general-purpose library should
prefer, and exactly what a backend named ``GERG2008`` must not use.  Twelve of
those 16 shift the mixture reducing temperature by more than 0.03 K at
:math:`z = (0.35, 0.65)`; four leave :math:`\beta`/:math:`\gamma` unchanged but
attach a *different* departure function.

**The gas constant.**  GERG specifies
:math:`R = 8.314472\ \mathrm{J\,mol^{-1}\,K^{-1}}`.  Under CoolProp's default
``NORMALIZE_GAS_CONSTANTS`` configuration the ``HEOS`` backend uses the CODATA
value for mixtures, which rescales :math:`p`, :math:`\alpha^{ig}`, :math:`c_v`
and :math:`w` by about :math:`1.1 \times 10^{-6}`.  The GERG backends override
this.

The scale of the difference is small but systematic.  For
90 % methane / 10 % nitrogen at 300 K and 1 MPa:

======================  ==========================
Backend                 :math:`\rho` [mol/m³]
======================  ==========================
``GERG2008``            406.9418737
``HEOS``                406.9525552
======================  ==========================

— a relative difference of :math:`2.6 \times 10^{-5}`, which is small compared
to a typical custody-transfer tolerance but is not numerical noise.  If you
need GERG numbers, ask for GERG.

Note also that this is *not* the same thing as REFPROP's ``REFPROP_USE_GERG``
flag, which substitutes GERG pure-fluid equations of state inside REFPROP's own
mixture model.  These backends implement GERG whole.

Implementation notes and references
===================================

The reference implementation is `teqp <https://github.com/usnistgov/teqp>`_,
specifically ``include/teqp/models/GERG/GERG.hpp``.  The coefficient tables in
CoolProp's GERG backend are transcribed from it, with a durable verification
script (``dev/gerg/verify_transcription.py``) that checks every table family
against teqp, and the test suite compares against teqp-generated reference
values at relative tolerances of 1e-12 for :math:`\alpha^r` and
:math:`\alpha^{ig}` and 1e-10 for :math:`p`, :math:`c_v` and the speed of
sound.  teqp's own values are in turn checked against the AGA8 reference
implementation.

The design of record for these backends — including the rationale for every
strictness rule above — is
``docs/superpowers/specs/2026-07-25-gerg-strict-backend-design.md`` in the
CoolProp repository.

The models themselves are published in:

* GERG-2004: Kunz, Klimeck, Wagner & Jaeschke, *The GERG-2004 Wide-Range
  Equation of State for Natural Gases and Other Mixtures*, GERG Technical
  Monograph 15, VDI Verlag, 2007 :cite:`Kunz-BOOK-2007`.
* GERG-2008: Kunz & Wagner, *J. Chem. Eng. Data* **57**, 3032-3091, 2012
  :cite:`Kunz-JCED-2012`.
