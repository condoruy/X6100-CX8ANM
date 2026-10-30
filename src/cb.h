/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: canales de la Banda Ciudadana (CB, 11 m)
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CB_BANDS    6       /* A B C D E (C: la oficial, 26.965 a 27.405 MHz) y UK (27/81, 27.60125 a 27.99125) */
#define CB_BAND_UK  5
#define CB_CHANNELS 40
#define CB_BAND_C   2

int32_t     cb_freq(int band, int ch);              /* Hz; ch 1..40 */
const char *cb_band_name(int band);                 /* "A" ... "E", "UK" */
const char *cb_band_desc(int band);                 /* "Official (CEPT/FCC)" (en inglés: tr() en la pantalla) */
bool        cb_in_range(int32_t hz);                /* donde la perilla salta de canal (nunca 10 m) */
bool        cb_channel_of(int32_t hz, int *band, int *ch);
int32_t     cb_step(int32_t hz, int steps);         /* el canal que sigue (o el anterior) */
const char *cb_channel_note(int ch);                /* 9: emergencias, 19: rutas */
const char *cb_name(int32_t hz, char *buf, size_t size);   /* "CB C-19", "CB UK-19" o NULL */
const char *cb_freq_text(int32_t hz, char *buf, size_t size);  /* "27.185" o "27.60125" (MHz) */

#ifdef __cplusplus
}
#endif
