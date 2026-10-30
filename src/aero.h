/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: banda aérea de HF. La banda aérea de VHF (118-137 MHz) queda fuera de lo que recibe
 *  el X6100; en HF están las rutas oceánicas y remotas (MWARA, en USB) y los VOLMET (partes
 *  meteorológicos para aviación, continuos o a horario).
 *  Frecuencias: listas de HFUnderground (MWARA) y DX Info Centre (VOLMET), en kHz de portadora.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char    *code;        /* "SAM", "VOLMET" */
    const char    *name;        /* "South America" (se traduce con tr) */
    uint8_t        area;        /* 1 Europa/África/Medio Oriente, 2 Américas, 3 Asia/Pacífico, 0 todas */
    const uint16_t *khz;        /* terminada en 0 */
} aero_family_t;

int                  aero_families(void);
const aero_family_t *aero_family(int i);

#ifdef __cplusplus
}
#endif
