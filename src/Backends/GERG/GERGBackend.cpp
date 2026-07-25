#include "GERGBackend.h"

#include <algorithm>
#include <cmath>
#include <utility>

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

// Ideal-gas coefficient tables, GERG-2004 monograph Table A3.1.
//
// Stored exactly as teqp stores them: {n0[1..7], theta0[4..7]}, i.e. 7 n and
// 4 theta with no padding, so the literals line up one-for-one with teqp
// GERG.hpp:470-487 (GERG-2004) and :1135-1141 (GERG-2008).  The padding to
// length 8 that makes the monograph's 1-based indices work is applied in
// pad_alphaig() below, just as teqp does on the way out of its accessor.
//
// n0[1] and n0[2] as tabulated here are the PUBLISHED integration constants.
// get_alphaig_coeffs discards them; see recalc_integration_constants.

using RawAlphaig = std::pair<std::vector<double>, std::vector<double>>;

/// teqp GERG.hpp:470-487 (GERG2004::get_alphaig_coeffs dict).
const std::map<std::string, RawAlphaig>& alphaig_2004() {
    static const std::map<std::string, RawAlphaig> data = {
      {"methane",
       {{19.597538587, -83.959667892, 3.000880, 0.763150, 0.00460, 8.744320, -4.469210000}, {4.306474465, 0.936220902, 5.577233895, 5.722644361}}},
      {"nitrogen", {{11.083437707, -22.202102428, 2.500310, 0.137320, -0.14660, 0.900660, 0}, {5.251822620, -5.393067706, 13.788988208, 0}}},
      {"carbondioxide",
       {{11.925182741, -16.118762264, 2.500020, 2.044520, -1.060440, 2.033660, 0.013930000}, {3.022758166, -2.844425476, 1.589964364, 1.121596090}}},
      {"ethane",
       {{24.675465518, -77.425313760, 3.002630, 4.339390, 1.237220, 13.19740, -6.019890000}, {1.831882406, 0.731306621, 3.378007481, 3.508721939}}},
      {"propane",
       {{31.602934734, -84.463284382, 3.029390, 6.605690, 3.1970, 19.19210, -8.372670000}, {1.297521801, 0.543210978, 2.583146083, 2.777773271}}},
      {"n-butane",
       {{20.884168790, -91.638478026, 3.339440, 9.448930, 6.894060, 24.46180, 14.782400000}, {1.101487798, 0.431957660, 4.502440459, 2.124516319}}},
      {"isobutane",
       {{20.413751434, -94.467620036, 3.067140, 8.975750, 5.251560, 25.14230, 16.138800000}, {1.074673199, 0.485556021, 4.671261865, 2.191583480}}},
      {"n-pentane", {{14.536635738, -89.919548319, 3.0, 8.950430, 21.8360, 33.40320, 0}, {0.380391739, 1.789520971, 3.777411113, 0}}},
      {"isopentane", {{15.449937973, -101.298172792, 3.0, 11.76180, 20.11010, 33.16880, 0}, {0.635392636, 1.977271641, 4.169371131, 0}}},
      {"n-hexane", {{14.345993081, -96.165722367, 3.0, 11.69770, 26.81420, 38.61640, 0}, {0.359036667, 1.691951873, 3.596924107, 0}}},
      {"n-heptane", {{15.063809621, -97.345252349, 3.0, 13.72660, 30.47070, 43.55610, 0}, {0.314348398, 1.548136560, 3.259326458, 0}}},
      {"n-octane", {{15.864709639, -97.370667555, 3.0, 15.68650, 33.80290, 48.17310, 0}, {0.279143540, 1.431644769, 2.973845992, 0}}},
      {"hydrogen",
       {{13.796474934, -175.864487294, 1.479060, 0.958060, 0.454440, 1.560390, -1.375600000},
        {6.891654113, 9.847634830, 49.765290750, 50.367279301}}},
      {"oxygen", {{10.001874708, -14.996095135, 2.501460, 1.075580, 1.013340, 0, 0}, {14.461722565, 7.223325463, 0, 0}}},
      {"carbonmonoxide", {{10.814500335, -19.843695435, 2.500550, 1.028650, 0.004930, 0, 0}, {11.675075301, 5.305158133, 0, 0}}},
      {"water", {{8.203553050, -11.996306443, 3.003920, 0.010590, 0.987630, 3.069040, 0}, {0.415386589, 1.763895929, 3.874803739, 0}}},
      {"helium", {{13.628441975, -143.470759602, 1.5, 0, 0, 0, 0}, {0, 0, 0, 0}}},
      {"argon", {{8.316662546, -4.946502600, 1.50, 0, 0, 0, 0}, {0, 0, 0, 0}}}};
    return data;
}

/// Entries GERG-2008 changes or adds relative to GERG-2004; everything else
/// falls through to alphaig_2004().  teqp GERG.hpp:1135-1141.
const std::map<std::string, RawAlphaig>& alphaig_2008_overrides() {
    static const std::map<std::string, RawAlphaig> data = {
      {"carbonmonoxide",
       {{10.813340744, -19.834733959, 2.50055, 1.02865, 0.00493, 0, 0}, {11.669802800, 5.302762306, 0, 0}}},  // changed in GERG-2008
      {"isopentane",
       {{15.449907693, -101.298172792, 3.0, 11.76180, 20.11010, 33.16880, 0}, {0.635392636, 1.977271641, 4.169371131, 0}}},  // changed in GERG-2008
      {"n-nonane", {{16.313913248, -102.160247463, 3.0, 18.02410, 38.12350, 53.34150, 0}, {0.263819696, 1.370586158, 2.848860483, 0}}},
      {"n-decane", {{15.870791919, -108.858547525, 3.0, 21.00690, 43.49310, 58.36570, 0}, {0.267034159, 1.353835195, 2.833479035, 0}}},
      {"hydrogensulfide", {{9.336197742, -16.266508995, 3.0, 3.11942, 1.00243, 0, 0}, {4.914580541, 2.270653980, 0, 0}}}};
    return data;
}

/// Zero-pad the raw {7 n, 4 theta} tables up to the monograph's 1-based
/// indexing: n0[1..7], theta0[4..7].  teqp GERG.hpp:497-506.
AlphaigCoeffs pad_alphaig(const std::string& gerg_name, const RawAlphaig& raw) {
    if (raw.first.size() != 7) {
        throw ValueError(format("[%s] does not have 7 n coefficients in ideal gas", gerg_name.c_str()));
    }
    if (raw.second.size() != 4) {
        throw ValueError(format("[%s] does not have 4 theta coefficients in ideal gas", gerg_name.c_str()));
    }
    AlphaigCoeffs c;
    c.n0 = raw.first;
    c.n0.insert(c.n0.begin(), 0.0);  // 0-pad so that indexing matches GERG-2004
    c.theta0 = raw.second;
    c.theta0.insert(c.theta0.begin(), 4, 0.0);  // 0-pad so that indexing matches GERG-2004
    return c;
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

std::pair<double, double> recalc_integration_constants(const AlphaigCoeffs& c, double T0, double Tci, double rho0, double rhoci, double Rstar_R) {
    // Faithful port of teqp GERG.hpp:49-74.  teqp builds two 3-element rows
    // {coefficient of n0[1], coefficient of n0[2], everything else} for the
    // reduced ideal-gas Helmholtz energy Aig00 and its tau-derivative Aig10,
    // then solves the resulting 2x2 system with Eigen.  A 2x2 does not need
    // Eigen, so it is written out by hand below; the row construction is kept
    // literally identical so the two implementations stay diffable.
    const double th = Tci / T0;
    auto sinh_term = [&](std::size_t i) { return (c.n0[i] != 0) ? c.n0[i] * std::log(std::abs(std::sinh(c.theta0[i] * th))) : 0.0; };
    auto cosh_term = [&](std::size_t i) { return (c.n0[i] != 0) ? c.n0[i] * std::log(std::abs(std::cosh(c.theta0[i] * th))) : 0.0; };
    auto sinh_dterm = [&](std::size_t i) { return (c.n0[i] != 0) ? c.n0[i] * c.theta0[i] * th / std::tanh(c.theta0[i] * th) : 0.0; };

    const double a00_0 = Rstar_R;
    const double a00_1 = Rstar_R * th;
    const double a00_2 = std::log(rho0 / rhoci) + Rstar_R * (c.n0[3] * std::log(th) + sinh_term(4) + sinh_term(6) - cosh_term(5) - cosh_term(7));

    // NOTE: teqp guards the two sinh terms with `!= 0` but deliberately does
    // NOT guard the two cosh terms (GERG.hpp:60-61).  Where n0[5] or n0[7] is
    // zero the unguarded term evaluates to zero anyway, so the behaviour is
    // identical; the asymmetry is preserved so the two sources stay diffable.
    const double a10_0 = 0.0;
    const double a10_1 = Rstar_R * th;
    const double a10_2 = Rstar_R
                         * (c.n0[3] + sinh_dterm(4) + sinh_dterm(6) - c.n0[5] * c.theta0[5] * th * std::tanh(c.theta0[5] * th)
                            - c.n0[7] * c.theta0[7] * th * std::tanh(c.theta0[7] * th));

    // Row 0: h0/(R*T0) = 1 + Aig10 = 0, so Aig10 = -1.
    // Row 1: s0/R = Aig10 - Aig00 = 0.
    const double A00 = a10_0, A01 = a10_1, b0 = -1.0 - a10_2;
    const double A10 = a10_0 - a00_0, A11 = a10_1 - a00_1, b1 = -a10_2 + a00_2;

    const double det = A00 * A11 - A01 * A10;
    if (std::abs(det) < 1e-300) {
        throw ValueError("GERG ideal-gas integration constants: singular 2x2 system");
    }
    const double n1 = (b0 * A11 - A01 * b1) / det;
    const double n2 = (A00 * b1 - b0 * A10) / det;
    return {n1, n2};
}

AlphaigCoeffs get_alphaig_coeffs(GERGModel model, const std::string& gerg_name) {
    // Throws if gerg_name is not a component of this model, and gives us the
    // reducing Tc/rhoc that the integration constants are solved against.
    const PureInfo info = get_pure_info(model, gerg_name);

    AlphaigCoeffs c;
    bool found = false;
    if (model == GERGModel::GERG_2008) {
        const auto& ov = alphaig_2008_overrides();
        auto it = ov.find(gerg_name);
        if (it != ov.end()) {
            c = pad_alphaig(gerg_name, it->second);
            found = true;
        }
    }
    if (!found) {
        const auto& base = alphaig_2004();
        auto it = base.find(gerg_name);
        if (it == base.end()) {
            // Unreachable with the tables as shipped: get_pure_info above has
            // already rejected anything outside component_names(model), and
            // the two ideal-gas tables cover every name in it.  Kept as a
            // drift guard -- if a component is ever added to component_names
            // without an ideal-gas row, this fires (the "padded monograph
            // shape" test sweeps every component of both models, so it fires
            // in the test suite rather than in user code).
            throw ValueError(format("Unable to load GERG ideal-gas coefficients for [%s]", gerg_name.c_str()));
        }
        c = pad_alphaig(gerg_name, it->second);
    }

    // Discard the published integration constants and re-solve them so that
    // h = s = 0 for the IDEAL GAS at 298.15 K and 101325 Pa.  teqp
    // GERG.hpp:370-382.  Note that rho0 uses R, not R*.
    const double T0 = 298.15;    // K
    const double p0 = 101325.0;  // Pa
    const double rho0 = p0 / (R_GERG * T0);
    const auto n12 = recalc_integration_constants(c, T0, info.Tc_K, rho0, info.rhoc_molm3, RSTAR_GERG / R_GERG);
    c.n0[1] = n12.first;
    c.n0[2] = n12.second;
    return c;
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
