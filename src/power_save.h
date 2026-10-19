/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - Modo ahorro (CX8ANM)
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Aplica params.power_save: el procesador baja la velocidad cuando no hay trabajo
 * (governor "schedutil"/"ondemand" en vez de "performance") */
void power_save_apply(void);

/* Segundos hasta atenuar la pantalla: con modo ahorro, como mucho POWER_SAVE_DIM_S */
int power_save_dim_timeout(int timeout_s);

/* La banda base frente a la última versión oficial conocida de Xiegu.
 * Devuelve el texto para Ajustes > Info ("V1.1.8 ... (al día)") */
const char *power_save_baseband_text(const char *version);

#ifdef __cplusplus
}
#endif
