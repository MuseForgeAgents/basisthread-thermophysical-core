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

namespace {

// Pure-fluid residual coefficient tables.
//
// Transcribed from teqp (https://github.com/usnistgov/teqp),
// include/teqp/models/GERG/GERG.hpp: GERG2004::get_pure_coeffs (lines
// 511-591) and GERG2008::get_pure_coeffs (lines 1105-1131).  Preserving
// teqp's own split -- one shared 12-term t/d/c/l set for most fluids, one
// shared 24-term set for methane/nitrogen/ethane, and fully independent
// tables for carbon dioxide, hydrogen, water, and helium -- keeps this file
// diffable by eye against the source.

/// Shared exponent set: t, d, c, l (n differs per fluid).
struct SharedExponents
{
    std::vector<double> t, d, c, l;
};

/// 12-term set shared (GERG-2004) by propane, n-butane, isobutane,
/// n-pentane, isopentane, n-hexane, n-heptane, n-octane, oxygen, carbon
/// monoxide, and argon; also used by every fluid GERG-2008 overrides or adds
/// within this same family (carbon monoxide, isopentane, hydrogen sulfide,
/// n-nonane, n-decane).  teqp GERG.hpp:537-540 and :1122-1125.
const SharedExponents& main12_exponents() {
    static const SharedExponents e = {{0.250, 1.125, 1.500, 1.375, 0.250, 0.875, 0.625, 1.750, 3.625, 3.625, 14.500, 12.000},
                                      {1, 1, 1, 2, 3, 7, 2, 5, 1, 4, 3, 4},
                                      {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1},
                                      {0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3}};
    return e;
}

/// 24-term set shared by methane, nitrogen, and ethane. teqp GERG.hpp:546-549.
const SharedExponents& mne24_exponents() {
    static const SharedExponents e = {{0.125, 1.125, 0.375, 1.125, 0.625, 1.500, 0.625,  2.625,  2.750,  2.125,  2.000,  1.750,
                                       4.500, 4.750, 5.000, 4.000, 4.500, 7.500, 14.000, 11.500, 26.000, 28.000, 30.000, 16.000},
                                      {1, 1, 2, 2, 4, 4, 1, 1, 1, 2, 3, 6, 2, 3, 3, 4, 4, 2, 3, 4, 5, 6, 6, 7},
                                      {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                                      {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 6, 6, 6, 6}};
    return e;
}

/// n-only table for the mne24 family, GERG-2004. teqp GERG.hpp:515-517.
const std::map<std::string, std::vector<double>>& n_mne_2004() {
    static const std::map<std::string, std::vector<double>> data = {
      {"methane",
       {0.57335704239162,     -0.16760687523730e1, 0.23405291834916,     -0.21947376343441,    0.16369201404128e-1,  0.15004406389280e-1,
        0.98990489492918e-1,  0.58382770929055,    -0.74786867560390,    0.30033302857974,     0.20985543806568,     -0.18590151133061e-1,
        -0.15782558339049,    0.12716735220791,    -0.32019743894346e-1, -0.68049729364536e-1, 0.24291412853736e-1,  0.51440451639444e-2,
        -0.19084949733532e-1, 0.55229677241291e-2, -0.44197392976085e-2, 0.40061416708429e-1,  -0.33752085907575e-1, -0.25127658213357e-2}},
      {"nitrogen",
       {0.59889711801201,     -0.16941557480731e1, 0.24579736191718,     -0.23722456755175,    0.17954918715141e-1,  0.14592875720215e-1,
        0.10008065936206,     0.73157115385532,    -0.88372272336366,    0.31887660246708,     0.20766491728799,     -0.19379315454158e-1,
        -0.16936641554983,    0.13546846041701,    -0.33066712095307e-1, -0.60690817018557e-1, 0.12797548292871e-1,  0.58743664107299e-2,
        -0.18451951971969e-1, 0.47226622042472e-2, -0.52024079680599e-2, 0.43563505956635e-1,  -0.36251690750939e-1, -0.28974026866543e-2}},
      {"ethane", {0.63596780450714,     -0.17377981785459e1, 0.28914060926272,     -0.33714276845694,   0.22405964699561e-1,  0.15715424886913e-1,
                  0.11450634253745,     0.10612049379745e1,  -0.12855224439423e1,  0.39414630777652,    0.31390924682041,     -0.21592277117247e-1,
                  -0.21723666564905,    -0.28999574439489,   0.42321173025732,     0.46434100259260e-1, -0.13138398329741,    0.11492850364368e-1,
                  -0.33387688429909e-1, 0.15183171583644e-1, -0.47610805647657e-2, 0.46917166277885e-1, -0.39401755804649e-1, -0.32569956247611e-2}}};
    return data;
}

/// n-only table for the main12 family, GERG-2004. teqp GERG.hpp:521-531.
const std::map<std::string, std::vector<double>>& n_main_2004() {
    static const std::map<std::string, std::vector<double>> data = {
      {"propane",
       {0.10403973107358e1, -0.28318404081403e1, 0.84393809606294, -0.76559591850023e-1, 0.94697373057280e-1, 0.24796475497006e-3, 0.27743760422870,
        -0.43846000648377e-1, -0.26991064784350, -0.69313413089860e-1, -0.29632145981653e-1, 0.14040126751380e-1}},
      {"n-butane",
       {0.10626277411455e1, -0.28620951828350e1, 0.88738233403777, -0.12570581155345, 0.10286308708106, 0.25358040602654e-3, 0.32325200233982,
        -0.37950761057432e-1, -0.32534802014452, -0.79050969051011e-1, -0.20636720547775e-1, 0.57053809334750e-2}},
      {"isobutane",
       {0.10429331589100e1, -0.28184272548892e1, 0.86176232397850, -0.10613619452487, 0.98615749302134e-1, 0.23948208682322e-3, 0.30330004856950,
        -0.41598156135099e-1, -0.29991937470058, -0.80369342764109e-1, -0.29761373251151e-1, 0.13059630303140e-1}},
      {"n-pentane",
       {0.10968643098001e1, -0.29988888298061e1, 0.99516886799212, -0.16170708558539, 0.11334460072775, 0.26760595150748e-3, 0.40979881986931,
        -0.40876423083075e-1, -0.38169482469447, -0.10931956843993, -0.32073223327990e-1, 0.16877016216975e-1}},
      {"isopentane",
       {0.11017531966644e1, -0.30082368531980e1, 0.99411904271336, -0.14008636562629, 0.11193995351286, 0.29548042541230e-3, 0.36370108598133,
        -0.48236083488293e-1, -0.35100280270615, -0.10185043812047, -0.35242601785454e-1, 0.19756797599888e-1}},
      {"n-hexane",
       {0.10553238013661e1, -0.26120615890629e1, 0.76613882967260, -0.29770320622459, 0.11879907733358, 0.27922861062617e-3, 0.46347589844105,
        0.11433196980297e-1, -0.48256968738131, -0.93750558924659e-1, -0.67273247155994e-2, -0.51141583585428e-2}},
      {"n-heptane",
       {0.10543747645262e1, -0.26500681506144e1, 0.81730047827543, -0.30451391253428, 0.12253868710800, 0.27266472743928e-3, 0.49865825681670,
        -0.71432815084176e-3, -0.54236895525450, -0.13801821610756, -0.61595287380011e-2, 0.48602510393022e-3}},
      {"n-octane",
       {0.10722544875633e1, -0.24632951172003e1, 0.65386674054928, -0.36324974085628, 0.12713269626764, 0.30713572777930e-3, 0.52656856987540,
        0.19362862857653e-1, -0.58939426849155, -0.14069963991934, -0.78966330500036e-2, 0.33036597968109e-2}},
      {"oxygen",
       {0.88878286369701, -0.24879433312148e1, 0.59750190775886, 0.96501817061881e-2, 0.71970428712770e-1, 0.22337443000195e-3, 0.18558686391474,
        -0.38129368035760e-1, -0.15352245383006, -0.26726814910919e-1, -0.25675298677127e-1, 0.95714302123668e-2}},
      {"carbonmonoxide",
       {0.92310041400851, -0.24885845205800e1, 0.58095213783396, 0.28859164394654e-1, 0.70256257276544e-1, 0.21687043269488e-3, 0.13758331015182,
        -0.51501116343466e-1, -0.14865357483379, -0.38857100886810e-1, -0.29100433948943e-1, 0.14155684466279e-1}},
      {"argon",
       {0.85095714803969, -0.24003222943480e1, 0.54127841476466, 0.16919770692538e-1, 0.68825965019035e-1, 0.21428032815338e-3, 0.17429895321992,
        -0.33654495604194e-1, -0.13526799857691, -0.16387350791552e-1, -0.24987666851475e-1, 0.88769204815709e-2}}};
    return data;
}

/// Carbon dioxide, GERG-2004: own 22-term set. teqp GERG.hpp:552-559.
PureCoeffs carbondioxide_2004() {
    return {{0.52646564804653,    -0.14995725042592e1,  0.27329786733782,     0.12949500022786,     0.15404088341841,    -0.58186950946814,
             -0.18022494838296,   -0.95389904072812e-1, -0.80486819317679e-2, -0.35547751273090e-1, -0.28079014882405,   -0.82435890081677e-1,
             0.10832427979006e-1, -0.67073993161097e-2, -0.46827907600524e-2, -0.28359911832177e-1, 0.19500174744098e-1, -0.21609137507166,
             0.43772794926972,    -0.22130790113593,    0.15190189957331e-1,  -0.15380948953300e-1},
            {0.000, 1.250, 1.625, 0.375, 0.375,  1.375,  1.125,  1.375,  0.125,  1.625,  3.750,
             3.500, 7.500, 8.000, 6.000, 16.000, 11.000, 24.000, 26.000, 28.000, 24.000, 26.000},
            {1, 1, 2, 3, 3, 3, 4, 5, 6, 6, 1, 4, 1, 1, 3, 3, 4, 5, 5, 5, 5, 5},
            {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 3, 3, 3, 5, 5, 5, 6, 6}};
}

/// Hydrogen, GERG-2004: own 14-term set. teqp GERG.hpp:562-568.
PureCoeffs hydrogen_2004() {
    return {{0.53579928451252e1, -0.62050252530595e1, 0.13830241327086, -0.71397954896129e-1, 0.15474053959733e-1, -0.14976806405771,
             -0.26368723988451e-1, 0.56681303156066e-1, -0.60063958030436e-1, -0.45043942027132, 0.42478840244500, -0.21997640827139e-1,
             -0.10499521374530e-1, -0.28955902866816e-2},
            {0.500, 0.625, 0.375, 0.625, 1.125, 2.625, 0.000, 0.250, 1.375, 4.000, 4.250, 5.000, 8.000, 8.000},
            {1, 1, 2, 2, 4, 1, 5, 5, 5, 1, 1, 2, 5, 1},
            {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 5}};
}

/// Water, GERG-2004: own 16-term set. teqp GERG.hpp:571-577.
PureCoeffs water_2004() {
    return {{0.82728408749586, -0.18602220416584e1, -0.11199009613744e1, 0.15635753976056, 0.87375844859025, -0.36674403715731, 0.53987893432436e-1,
             0.10957690214499e1, 0.53213037828563e-1, 0.13050533930825e-1, -0.41079520434476, 0.14637443344120, -0.55726838623719e-1,
             -0.11201774143800e-1, -0.66062758068099e-2, 0.46918522004538e-2},
            {0.500, 1.250, 1.875, 0.125, 1.500, 1.000, 0.750, 1.500, 0.625, 2.625, 5.000, 4.000, 4.500, 3.000, 4.000, 6.000},
            {1, 1, 1, 2, 2, 3, 4, 1, 5, 5, 1, 2, 4, 4, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 5, 5}};
}

/// Helium, GERG-2004: own 12-term set (distinct from main12_exponents).
/// teqp GERG.hpp:580-586.
PureCoeffs helium_2004() {
    return {{-0.45579024006737, 0.12516390754925e1, -0.15438231650621e1, 0.20467489707221e-1, -0.34476212380781, -0.20858459512787e-1,
             0.16227414711778e-1, -0.57471818200892e-1, 0.19462416430715e-1, -0.33295680123020e-1, -0.10863577372367e-1, -0.22173365245954e-1},
            {0.000, 0.125, 0.750, 1.000, 0.750, 2.625, 0.125, 1.250, 2.000, 1.000, 4.500, 5.000},
            {1, 1, 1, 4, 1, 3, 5, 5, 5, 2, 1, 2},
            {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 3, 3}};
}

/// n-only table for members of the main12 family that GERG-2008 overrides
/// (carbon monoxide, isopentane) or adds (hydrogen sulfide, n-nonane,
/// n-decane).  Fluids not in this table fall through to GERG-2004.
/// teqp GERG.hpp:1110-1116.
const std::map<std::string, std::vector<double>>& n_main_2008_overrides() {
    static const std::map<std::string, std::vector<double>> data = {
      {"carbonmonoxide",
       {0.90554, -0.24515e1, 0.53149, 0.24173e-1, 0.72156e-1, 0.18818e-3, 0.19405, -0.43268e-1, -0.12778, -0.27896e-1, -0.34154e-1, 0.16329e-1}},
      {"isopentane",
       {0.10963e1, -0.30402e1, 0.10317e1, -0.15410, 0.11535, 0.29809e-3, 0.39571, -0.45881e-1, -0.35804, -0.10107, -0.35484e-1, 0.18156e-1}},
      {"hydrogensulfide",
       {0.87641, -0.20367e1, 0.21634, -0.50199e-1, 0.66994e-1, 0.19076e-3, 0.20227, -0.45348e-2, -0.22230, -0.34714e-1, -0.14885e-1, 0.74154e-2}},
      {"n-decane", {0.10461e1, -0.24807e1, 0.74372, -0.52579, 0.15315, 0.32865e-3, 0.84178, 0.55424e-1, -0.73555, -0.18507, -0.20775e-1, 0.12335e-1}},
      {"n-nonane",
       {0.11151e1, -0.27020e1, 0.83416, -0.38828, 0.13760, 0.28185e-3, 0.62037, 0.15847e-1, -0.61726, -0.15043, -0.12982e-1, 0.44325e-2}}};
    return data;
}

}  // namespace

PureCoeffs get_pure_coeffs(GERGModel model, const std::string& gerg_name) {
    if (model == GERGModel::GERG_2008) {
        const auto& overrides = n_main_2008_overrides();
        auto it = overrides.find(gerg_name);
        if (it != overrides.end()) {
            const auto& e = main12_exponents();
            return PureCoeffs{it->second, e.t, e.d, e.c, e.l};
        }
    }
    // Fall through to GERG-2004 (also GERG-2004's own lookup path).
    if (gerg_name == "carbondioxide") {
        return carbondioxide_2004();
    }
    if (gerg_name == "hydrogen") {
        return hydrogen_2004();
    }
    if (gerg_name == "water") {
        return water_2004();
    }
    if (gerg_name == "helium") {
        return helium_2004();
    }
    {
        const auto& mne = n_mne_2004();
        auto it = mne.find(gerg_name);
        if (it != mne.end()) {
            const auto& e = mne24_exponents();
            return PureCoeffs{it->second, e.t, e.d, e.c, e.l};
        }
    }
    {
        const auto& main = n_main_2004();
        auto it = main.find(gerg_name);
        if (it != main.end()) {
            const auto& e = main12_exponents();
            return PureCoeffs{it->second, e.t, e.d, e.c, e.l};
        }
    }
    throw ValueError(format("Unable to load GERG pure residual coefficients for [%s]", gerg_name.c_str()));
}

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
