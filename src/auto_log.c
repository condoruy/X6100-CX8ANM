/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  QSO guardados solos (ver auto_log.h).
 */

#include "auto_log.h"

#include "cfg/cfg_api.h"
#include "dialog_field.h"
#include "digi_qso.h"
#include "msg.h"
#include "panel.h"
#include "params/params.h"
#include "qso_log.h"
#include "station.h"
#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define DUP_SECONDS (15 * 60)   /* el mismo indicativo, modo y banda: una vez cada 15 min */
#define CW_KEEP     400         /* lo último transmitido en CW, para buscar indicativo y RST */

static char   last_call[32];
static int    last_band;
static char   last_mode[12];
static time_t last_time;

static char   cw_sent[CW_KEEP + 1];
static time_t cw_first;         /* cuándo empezó lo que hay en cw_sent: hora del QSO */

bool auto_log_enabled(void) {
    return params.auto_log.x;
}

/* Un indicativo: letras y números, al menos un número y una letra, con / opcional */
static bool is_call(const char *s, size_t n) {
    bool digit = false, alpha = false;

    if (n < 3 || n > 15) return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (isdigit((unsigned char)c)) digit = true;
        else if (isalpha((unsigned char)c)) alpha = true;
        else if (c != '/') return false;
    }
    return digit && alpha;
}

/* ¿Tiene la palabra 73 o SK (también <SK>)? */
static bool has_signoff(const char *text) {
    const char *p = text;

    while (*p) {
        while (*p && !isalnum((unsigned char)*p)) p++;
        const char *w = p;
        while (*p && isalnum((unsigned char)*p)) p++;
        size_t n = p - w;
        if (n == 2 && (!strncasecmp(w, "73", 2) || !strncasecmp(w, "SK", 2))) return true;
    }
    return false;
}

static void upper(char *s) {
    for (; *s; s++) *s = toupper((unsigned char)*s);
}

/* RST de 3 cifras: 599, 5NN, 579... (N = 9) */
static int rst_value(const char *s, size_t n) {
    if (n != 3 || s[0] < '1' || s[0] > '5') return 0;
    int v = s[0] - '0';
    for (int i = 1; i < 3; i++) {
        char c = toupper((unsigned char)s[i]);
        if (c == 'N') c = '9';
        if (c < '1' || c > '9') return 0;
        v = v * 10 + (c - '0');
    }
    return v;
}

static bool save(const char *call_in, qso_log_mode_t mode, const char *mode_name, int rsts, int rstr, time_t start) {
    char     call[32];
    time_t   now  = time(NULL);
    uint64_t freq = (uint64_t)cparam_i_get(cfg_fg_freq);
    int      band = qso_log_freq_to_band(freq);

    if (!params.callsign.x[0]) {
        msg_update_text_fmt("QSO not logged: set your callsign in Settings");
        return false;
    }
    snprintf(call, sizeof(call), "%s", call_in);
    upper(call);
    if (!strcasecmp(call, params.callsign.x)) return false;
    if (!strcmp(call, last_call) && band == last_band && !strcmp(mode_name, last_mode) && now - last_time < DUP_SECONDS)
        return false;   /* el 73 se manda dos veces: un solo QSO */

    char            *canon = util_canonize_callsign(call, false);
    qso_log_record_t rec   = qso_log_record_create(params.callsign.x, canon ? canon : call, start ? start : now, mode,
                                                   rsts ? rsts : 599, rstr ? rstr : 599, freq, NULL, NULL,
                                                   station_qth(), NULL);
    free(canon);
    qso_log_record_save(rec);
    dialog_field_day_log_mode(&rec, mode_name);

    snprintf(last_call, sizeof(last_call), "%s", call);
    snprintf(last_mode, sizeof(last_mode), "%s", mode_name);
    last_band = band;
    last_time = now;
    msg_update_text_fmt("QSO logged automatically: %s %s", call, mode_name);
    return true;
}

void auto_log_cw_sent(const char *text) {
    size_t have, add;

    if (!text || !*text || !auto_log_enabled()) return;
    if (!cw_sent[0] || time(NULL) - cw_first > 30 * 60) {   /* otro QSO */
        cw_sent[0] = '\0';
        cw_first   = time(NULL);
    }
    have = strlen(cw_sent);
    add  = strlen(text);
    if (have + add + 1 > CW_KEEP) {     /* lo más viejo afuera */
        size_t drop = have + add + 1 - CW_KEEP;
        if (drop >= have) have = 0;
        else {
            memmove(cw_sent, cw_sent + drop, have - drop + 1);
            have -= drop;
        }
        cw_sent[have] = '\0';
    }
    snprintf(cw_sent + have, sizeof(cw_sent) - have, " %.*s", (int)(CW_KEEP - have - 1), text);
    upper(cw_sent);

    if (!has_signoff(text)) return;

    /* El indicativo: el último "XX1XX DE <yo>", si no el último que escribió el decodificador */
    char        call[32] = "";
    char        me[32];
    int         rsts = 0;
    const char *words[96];
    size_t      lens[96], nw = 0;

    snprintf(me, sizeof(me), "%s", params.callsign.x);
    upper(me);
    for (const char *p = cw_sent; *p && nw < 96;) {
        while (*p && !(isalnum((unsigned char)*p) || *p == '/')) p++;
        if (!*p) break;
        words[nw] = p;
        while (*p && (isalnum((unsigned char)*p) || *p == '/')) p++;
        lens[nw] = p - words[nw];
        nw++;
    }
    for (size_t i = 0; i + 2 < nw; i++) {
        if (lens[i + 1] == 2 && !strncmp(words[i + 1], "DE", 2) && lens[i + 2] == strlen(me) &&
            !strncmp(words[i + 2], me, lens[i + 2]) && is_call(words[i], lens[i]) &&
            !(lens[i] == strlen(me) && !strncmp(words[i], me, lens[i])))
            snprintf(call, sizeof(call), "%.*s", (int)lens[i], words[i]);
    }
    if (!call[0]) {
        const char *c = panel_last_call();
        if (c && is_call(c, strlen(c)) && strcasecmp(c, me)) snprintf(call, sizeof(call), "%s", c);
    }
    for (size_t i = 0; i < nw; i++) {
        int v = rst_value(words[i], lens[i]);
        if (v) rsts = v;
    }
    if (!call[0]) {
        if (time(NULL) - last_time < 180) return;   /* el segundo 73 de un QSO ya guardado */
        msg_update_text_fmt("73 sent: no callsign to log (Log QSO)");
        return;
    }
    if (save(call, MODE_CW, "CW", rsts, 0, cw_first)) cw_sent[0] = '\0';
}

void auto_log_digi_sent(const char *text, const char *mode) {
    const char *dx;

    if (!text || !auto_log_enabled() || !has_signoff(text)) return;
    dx = digi_qso_dx();
    if (!dx || !is_call(dx, strlen(dx))) {
        msg_update_text_fmt("73 sent: no DX call to log (set it on the DX call button)");
        return;
    }
    save(dx, MODE_OTHER, mode, atoi(digi_qso_rst_sent()), atoi(digi_qso_rst_rcvd()), 0);
}
