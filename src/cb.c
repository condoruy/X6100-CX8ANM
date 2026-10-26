/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: Banda Ciudadana (CB, 11 m). Los 40 canales de la banda oficial (la "C", 26.965 a
 *  27.405 MHz) y los mismos 40 corridos 450 kHz en las bandas A, B, D y E de los equipos de
 *  exportación. Con el modo canales, la perilla principal salta de canal en canal dentro de la CB.
 */
#include "cb.h"

#include <stdio.h>
#include <stdlib.h>

/* Banda C, canales 1 a 40 (kHz). Ojo: el 23 está después del 24 y 25 (es así en la norma). */
static const int32_t ch_c_khz[CB_CHANNELS] = {
    26965, 26975, 26985, 27005, 27015, 27025, 27035, 27055, 27065, 27075,
    27085, 27105, 27115, 27125, 27135, 27155, 27165, 27175, 27185, 27205,
    27215, 27225, 27255, 27235, 27245, 27265, 27275, 27285, 27295, 27305,
    27315, 27325, 27335, 27345, 27355, 27365, 27375, 27385, 27395, 27405,
};

static const int32_t band_offset_khz[CB_BANDS] = {-900, -450, 0, 450, 900};   /* A B C D E */

int32_t cb_freq(int band, int ch) {
    if (band < 0 || band >= CB_BANDS || ch < 1 || ch > CB_CHANNELS) return 0;
    return (ch_c_khz[ch - 1] + band_offset_khz[band]) * 1000;
}

char cb_band_letter(int band) {
    return band >= 0 && band < CB_BANDS ? (char)('A' + band) : '?';
}

/* Nunca en 10 m (desde 28.000 MHz) ni en la broadcast de 11 m: la perilla salta de canal solo
 * dentro de la banda oficial (26.960 a 27.410 MHz) o si la radio ya está justo en un canal */
#define CB_TOP_HZ 27999000

bool cb_in_range(int32_t hz) {
    int band, ch;
    if (hz >= 26960000 && hz <= 27410000) return true;
    return hz < CB_TOP_HZ && cb_channel_of(hz, &band, &ch);
}

bool cb_channel_of(int32_t hz, int *band, int *ch) {
    for (int b = 0; b < CB_BANDS; b++)
        for (int c = 1; c <= CB_CHANNELS; c++)
            if (abs(hz - cb_freq(b, c)) <= 1000) {
                if (band) *band = b;
                if (ch) *ch = c;
                return true;
            }
    return false;
}

/* Los canales en orden de frecuencia: A1..A40, B1..B40, ... (el 23 va entre el 22 y el 24 por
 * número, no por frecuencia: se recorre por número de canal, como en un equipo de CB) */
int32_t cb_step(int32_t hz, int steps) {
    int band, ch;

    if (!cb_channel_of(hz, &band, &ch)) {
        /* fuera de un canal: el más cercano en el sentido del giro */
        int32_t best = 0;
        for (int b = 0; b < CB_BANDS; b++)
            for (int c = 1; c <= CB_CHANNELS; c++) {
                int32_t f = cb_freq(b, c);
                if (f >= CB_TOP_HZ) continue;
                if (steps > 0 ? f > hz && (!best || f < best) : f < hz && (!best || f > best)) best = f;
            }
        return best ? best : hz;
    }
    int idx = band * CB_CHANNELS + (ch - 1) + steps;
    int max = CB_BANDS * CB_CHANNELS;
    if (idx < 0) idx = 0;
    if (idx >= max) idx = max - 1;
    while (idx > 0 && cb_freq(idx / CB_CHANNELS, idx % CB_CHANNELS + 1) >= CB_TOP_HZ) idx--;   /* no entra en 10 m */
    return cb_freq(idx / CB_CHANNELS, idx % CB_CHANNELS + 1);
}

const char *cb_channel_note(int ch) {
    switch (ch) {
        case 9:  return "Emergencias";
        case 19: return "Rutas y viajeros";
        default: return "";
    }
}

const char *cb_name(int32_t hz, char *buf, size_t size) {
    int band, ch;
    if (!cb_channel_of(hz, &band, &ch)) return NULL;
    snprintf(buf, size, "CB %c-%d", cb_band_letter(band), ch);
    return buf;
}
