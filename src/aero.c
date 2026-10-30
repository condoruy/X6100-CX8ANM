/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: frecuencias de la banda aérea de HF (aero.h)
 */
#include "aero.h"

#define F(name, ...) static const uint16_t name[] = {__VA_ARGS__, 0}

/* Américas */
F(sam,      2944, 3479, 4669, 5526, 6649, 8855, 10096, 11360, 13297, 17907);
F(sat,      2854, 2935, 3452, 5565, 6535, 8861, 11291, 13315, 13357, 17955);
F(car,      2887, 3455, 5520, 5550, 6577, 6586, 8846, 8918, 11387, 11396, 13297, 17907);
F(nat,      2872, 2962, 2971, 3016, 3476, 4675, 5598, 5616, 5649, 6622, 6628, 8825, 8831, 8864, 8879, 8891,
            8906, 11279, 11309, 11336, 13291, 13306, 17946);
F(cep,      2869, 3413, 4657, 5547, 5574, 6673, 8843, 10057, 11282, 13300, 17904);
F(vol_sam,  2881, 5601, 10024, 10087, 13279);
F(vol_eze,  2881, 5601, 11369);
F(vol_arg,  2899, 2971, 3434, 4657, 4675, 5475, 8825, 8900, 8906);
/* Europa, África, Medio Oriente */
F(eur,      3479, 5661, 6598, 10084, 13288, 17961);
F(vol_snn,  3413, 5505, 8957, 13264);
F(vol_raf,  5450, 11253);
F(afi,      2851, 2878, 3425, 3467, 4657, 5652, 5658, 6559, 6574, 6673, 8894, 8903);
F(vol_afi,  2860, 5499, 10057, 13261);
F(mid,      2992, 3467, 5658, 5667, 6625, 6631, 8918, 8951, 10018, 11375, 13288, 13312, 17961);
F(nca,      3004, 3019, 4678, 5646, 5664, 6592, 10096, 13303, 13315, 17958);
/* Asia y Pacífico */
F(ea,       3016, 3485, 3491, 5655, 5670, 6571, 8897, 10042, 11396, 13297, 13303, 13309, 17907);
F(vol_ea,   3458, 5673, 8849, 13285);
F(sea,      3470, 3485, 5649, 5655, 6556, 8942, 10066, 11396, 13309, 13318, 17907);
F(vol_sea,  2965, 6676, 11387);
F(cwp,      2998, 3455, 4666, 5652, 5661, 6532, 6562, 8903, 10081, 11304, 13300, 17904);
F(vol_pac,  2863, 6679, 8828, 13282);
F(np,       2932, 5628, 6655, 6661, 10048, 11330, 13300, 17904);
F(sp,       3467, 5559, 5643, 8867, 10084, 11327, 13300, 17904);
/* Todas */
F(sar,      3023, 5680);

static const aero_family_t fam[] = {
    {"SAM",    "South America",                          2, sam},
    {"SAT",    "South Atlantic",                         2, sat},
    {"VOLMET", "South America (weather)",                2, vol_sam},
    {"VOLMET", "Ezeiza, Argentina (weather)",            2, vol_eze},
    {"VOLMET", "Argentina, regional (weather)",          2, vol_arg},
    {"CAR",    "Caribbean",                              2, car},
    {"NAT",    "North Atlantic",                         2, nat},
    {"CEP",    "Central East Pacific",                   2, cep},
    {"EUR",    "Europe",                                 1, eur},
    {"VOLMET", "Shannon, Ireland (weather)",             1, vol_snn},
    {"VOLMET", "RAF, United Kingdom (weather)",          1, vol_raf},
    {"AFI",    "Africa",                                 1, afi},
    {"VOLMET", "Africa (weather)",                       1, vol_afi},
    {"MID",    "Middle East",                            1, mid},
    {"NCA",    "North Central Asia",                     1, nca},
    {"EA",     "East Asia",                              3, ea},
    {"VOLMET", "China (weather)",                        3, vol_ea},
    {"SEA",    "South-East Asia",                        3, sea},
    {"VOLMET", "Bangkok, India, Singapore (weather)",    3, vol_sea},
    {"CWP",    "Central West Pacific",                   3, cwp},
    {"VOLMET", "Tokyo, Hong Kong, Auckland (weather)",   3, vol_pac},
    {"NP",     "North Pacific",                          3, np},
    {"SP",     "South Pacific",                          3, sp},
    {"SAR",    "Search and rescue (worldwide)",          0, sar},
};

int aero_families(void) {
    return sizeof(fam) / sizeof(fam[0]);
}

const aero_family_t *aero_family(int i) {
    return i >= 0 && i < aero_families() ? &fam[i] : NULL;
}
