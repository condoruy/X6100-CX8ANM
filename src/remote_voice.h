/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: voz desde el navegador (página Remoto+). El servidor web le pasa a la interfaz el audio
 *  del micrófono del teléfono o la PC por un socket UNIX; la radio transmite con ese audio como
 *  con los mensajes de voz grabados (modem + audio_play).
 *
 *  Protocolo (REMOTE_VOICE_SOCK, un cliente a la vez):
 *    el cliente conecta; la interfaz contesta una línea: "OK\n" o "ERR <motivo>\n" (y cierra);
 *    después el cliente manda PCM s16le mono a 16000 Hz. La radio sale al aire cuando juntó
 *    0,3 s de audio y deja de transmitir:
 *      - cuando el cliente cierra (lo que quedaba se termina de mandar),
 *      - si no llega audio por más de VOICE_GAP_MS (se cortó la red: corta en seco),
 *      - a los VOICE_MAX_S segundos,
 *      - si se abre una app, cambia a CW/digital o alguien aprieta el PTT del frente.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REMOTE_VOICE_SOCK  "/tmp/x6100_voice.sock"
#define VOICE_RATE         16000
#define VOICE_GAP_MS       800
#define VOICE_MAX_S        180

void remote_voice_init(void);

/* La voz remota está al aire */
bool remote_voice_on_air(void);

#ifdef __cplusplus
}
#endif
