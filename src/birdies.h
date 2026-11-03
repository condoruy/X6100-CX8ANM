/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: birdies (portadoras propias de la radio en frecuencias fijas).
 *  Lista en /mnt/birdies.txt; cuando uno cae dentro del filtro de recepción, el notch (DNF) de la
 *  banda base se pone solo sobre ese tono. "Marcar" mide el tono que se escucha y lo agrega.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BIRDIES_FILE "/mnt/birdies.txt"

void birdies_init(void);

/* Recalcula el notch (frecuencia, modo, filtro o DNF cambiaron) */
void birdies_refresh(void);

/* Desde la pantalla de ajustes */
void birdies_mark(void);            /* mide ~1,5 s de audio y agrega el tono más fuerte del filtro */
void birdies_remove_here(void);     /* quita el birdie que está dentro del filtro */
void birdies_clear(void);
int  birdies_count(void);
bool birdies_active(void);          /* el notch está puesto sobre un birdie ahora */

/* Hilo de audio: mientras se marca, el audio de recepción va acá */
bool birdies_capturing(void);
void birdies_put_audio(size_t nsamples, const int16_t *samples);

#ifdef __cplusplus
}
#endif
