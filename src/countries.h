/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: países (ISO 3166) y regiones IARU. Los nombres van en el idioma de la interfaz;
 *  cada país trae su región, la zona horaria de su capital (o ciudad principal) y su locator.
 *  Tabla generada por tools/countries/gen.py.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char        cc[3];          /* "UY" */
    uint8_t     region;         /* IARU 1, 2 o 3 */
    const char *name[5];        /* en, es, de, fr, it (lang_t) */
    const char *tz;             /* POSIX, "" si no se sabe */
    char        loc[7];         /* locator de la capital, "" si no se sabe */
} country_t;

int              countries_count(void);
/* El i-ésimo en orden alfabético del idioma actual (region 0: todos; 1..3: solo esa región) */
int              countries_in_region(int region);
const country_t *country_sorted(int region, int i);
int              country_sorted_index(int region, const char *cc);   /* -1 si no está */
const country_t *country_find(const char *cc);
const char      *country_name(const country_t *c);                  /* idioma actual */
const char      *country_name_cc(const char *cc);                    /* "" si no está */

/* Región: título ("Region 2") y quiénes están (ya traducidos) */
const char      *region_title(int region);
const char      *region_summary(int region);

#ifdef __cplusplus
}
#endif
