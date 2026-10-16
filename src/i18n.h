/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Idiomas de la interfaz (CX8ANM / Cóndor Tecnológico).
 *
 *  tr("Settings") devuelve el texto en el idioma elegido. El texto original
 *  (inglés) es la clave; lo que no está en la tabla se devuelve tal cual, así
 *  que frecuencias, indicativos y nombres pasan sin cambios.
 *  Varias líneas ("Filter low:\n200 Hz") se traducen línea por línea.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LANG_EN = 0,
    LANG_ES = 1,
    LANG_COUNT
} lang_t;

/* Texto traducido. El resultado de un texto de varias líneas vive un rato
 * (LVGL lo copia enseguida); para guardar el puntero, tr_static() */
const char *tr(const char *s);
const char *tr_static(const char *s);

void   i18n_set_lang(uint8_t lang);
uint8_t i18n_get_lang(void);

#ifdef __cplusplus
}
#endif
