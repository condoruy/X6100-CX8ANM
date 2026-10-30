/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: asistente del primer encendido. Pide, en orden, el idioma, el indicativo (la
 *  licencia), la región IARU y el país; lo guarda y no vuelve a aparecer. Desde Ajustes se
 *  abre solo un paso (región o país) o el asistente entero otra vez.
 */
#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETUP_ALL = 0,      /* los cuatro pasos */
    SETUP_REGION,       /* solo la región */
    SETUP_COUNTRY,      /* solo el país */
} setup_mode_t;

extern dialog_t *dialog_setup;

/* Abre el asistente (sobre la pantalla principal) */
void dialog_setup_open(setup_mode_t mode);

/* Al arrancar: el asistente, si todavía no se completó. true si se abrió */
bool dialog_setup_first_start(void);

/* Los límites de 160, 80, 40 y 6 m según la región (1..3), en la tabla de bandas */
void region_apply(int region);

/* Elegir país: zona horaria, locator (si no hay) y radio por Internet */
void country_apply(const char *cc);

#ifdef __cplusplus
}
#endif
