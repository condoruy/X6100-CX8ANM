/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: Zello (canales de voz por Internet) en el X6100.
 */
#pragma once

#include "dialog.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_zello;

/* El PTT del costado de la radio con la app abierta: habla por Zello (con el micrófono de mano) y no sale
 * al aire. true si la app se quedó con la tecla. */
bool dialog_zello_ptt(bool press);

/* Desde el hilo de audio: mientras se habla por Zello, lo que capta el micrófono va a Zello */
bool dialog_zello_capturing(void);
void dialog_zello_put_mic(size_t nsamples, const int16_t *samples);

#ifdef __cplusplus
}
#endif
