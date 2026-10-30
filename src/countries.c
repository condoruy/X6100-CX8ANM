/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: países y regiones IARU (countries.h)
 */
#include "countries.h"

#include "i18n.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "countries_data.inc"

static int lang(void) {
    int l = i18n_get_lang();
    return l >= 0 && l < 5 ? l : 0;
}

int countries_count(void) {
    return COUNTRIES_N;
}

int countries_in_region(int region) {
    int n = 0;
    if (!region) return COUNTRIES_N;
    for (int i = 0; i < COUNTRIES_N; i++)
        if (country_tab[i].region == region) n++;
    return n;
}

const country_t *country_sorted(int region, int i) {
    const uint16_t *order = country_order[lang()];
    for (int k = 0; k < COUNTRIES_N; k++) {
        const country_t *c = &country_tab[order[k]];
        if (region && c->region != region) continue;
        if (i-- == 0) return c;
    }
    return NULL;
}

int country_sorted_index(int region, const char *cc) {
    const uint16_t *order = country_order[lang()];
    int             n     = 0;
    if (!cc || !cc[0]) return -1;
    for (int k = 0; k < COUNTRIES_N; k++) {
        const country_t *c = &country_tab[order[k]];
        if (region && c->region != region) continue;
        if (!strcasecmp(c->cc, cc)) return n;
        n++;
    }
    return -1;
}

const country_t *country_find(const char *cc) {
    if (!cc || !cc[0]) return NULL;
    for (int i = 0; i < COUNTRIES_N; i++)
        if (!strcasecmp(country_tab[i].cc, cc)) return &country_tab[i];
    return NULL;
}

const char *country_name(const country_t *c) {
    return c ? c->name[lang()] : "";
}

const char *country_name_cc(const char *cc) {
    return country_name(country_find(cc));
}

const char *region_title(int region) {
    switch (region) {
        case 1: return tr("IARU Region 1");
        case 2: return tr("IARU Region 2");
        case 3: return tr("IARU Region 3");
        default: return "";
    }
}

const char *region_summary(int region) {
    switch (region) {
        case 1: return tr("Europe, Africa, the Middle East, Russia and the former Soviet republics, and Mongolia");
        case 2: return tr("North, Central and South America, the Caribbean, Greenland and Hawaii");
        case 3: return tr("Asia and the Pacific: China, Japan, India, Iran, Pakistan, South-East Asia, Australia, "
                          "New Zealand and the Pacific islands");
        default: return "";
    }
}
