/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  QSO guardados solos (CX8ANM). FT8/FT4 ya se guardaban solos; esto suma
 *  CW, PSK, Olivia y JTTY: cuando usted transmite la despedida (73 o SK) el
 *  QSO va al log de la radio y al log del día (web, Operación > Logs).
 *  Se apaga en Ajustes > General > "Guardar QSO solos".
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ¿Está encendido? (params.auto_log) */
bool auto_log_enabled(void);

/* Texto transmitido en CW (teclado CW, mensajes CW, línea CW): busca el
 * indicativo ("XX1XX DE <mi indicativo>" o el último decodificado) y el RST */
void auto_log_cw_sent(const char *text);

/* Texto transmitido en un modo digital de teclado: con el indicativo DX y los
 * RST de la pantalla del modo (digi_qso). mode: "PSK31", "PSK63", "OLIVIA", "JTTY" */
void auto_log_digi_sent(const char *text, const char *mode);

#ifdef __cplusplus
}
#endif
