/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: alertas meteorológicas de INUMET (x6100-meteo) en pantalla
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void        meteo_alerts_init(void);
void        meteo_alerts_now(void);              /* la próxima vuelta consulta enseguida */
void        meteo_alerts_check_file(void);       /* las alertas de /tmp/x6100-meteo-alerts.json */
bool        meteo_position(double *lat, double *lon);   /* GPS si hay, si no el locator QTH */
uint32_t    meteo_level_color(int level);
const char *meteo_level_name(int level, const char *kind);
void        meteo_short_time(const char *in, char *out, size_t size);

#ifdef __cplusplus
}
#endif
