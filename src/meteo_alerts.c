/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: alertas meteorológicas de INUMET. Con WiFi, cada 15 minutos x6100-meteo trae las
 *  advertencias vigentes; una nueva para el departamento de la radio (por el GPS o el locator QTH)
 *  sale en un recuadro del color de la alerta. La app Meteo muestra el detalle.
 */
#include "meteo_alerts.h"

#include "cJSON.h"
#include "gps.h"
#include "params/params.h"
#include "qth/qth.h"
#include "scheduler.h"
#include "styles.h"
#include "theme.h"
#include "wifi.h"
#include "i18n.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define HELPER       "/usr/sbin/x6100-meteo"
#define ALERTS_FILE  "/tmp/x6100-meteo-alerts.json"
#define SEEN_DIR     "/mnt/meteo"
#define SEEN_FILE    SEEN_DIR "/vistas"
#define CHECK_S      (15 * 60)
#define POPUP_MS     (60 * 1000)

static time_t      last_check;
static bool        busy;
static lv_obj_t   *popup;
static lv_timer_t *popup_timer;

bool meteo_position(double *lat, double *lon) {
    int age = 0;

    if (gps_last_fix(lat, lon, &age) && age < 3600) return true;
    if (gps_phone_fix(lat, lon, &age) && age < 3600) return true;
    if (params.qth.x[0] && qth_grid_check(params.qth.x)) {
        qth_str_to_pos(params.qth.x, lat, lon);
        return true;
    }
    *lat = -34.896;     /* GF15WC */
    *lon = -56.146;
    return false;
}

uint32_t meteo_level_color(int level) {
    switch (level) {
        case 2:  return 0xffd800;   /* los colores de INUMET */
        case 3:  return 0xff6a00;
        case 4:  return 0xe00000;
        default: return theme_hex(TC_warn);
    }
}

const char *meteo_level_name(int level, const char *kind) {
    switch (level) {
        case 2:  return tr("YELLOW ALERT");
        case 3:  return tr("ORANGE ALERT");
        case 4:  return tr("RED ALERT");
        default: return kind && !strcmp(kind, "aviso") ? tr("WEATHER NOTICE") : tr("WEATHER WARNING");
    }
}

/* "2026-10-26 15:00" o "2026-10-26T15:00..." -> "26/10 15:00" */
void meteo_short_time(const char *in, char *out, size_t size) {
    int y, mo, d, h, mi;
    if (in && sscanf(in, "%d-%d-%d%*c%d:%d", &y, &mo, &d, &h, &mi) == 5)
        snprintf(out, size, "%02d/%02d %02d:%02d", d, mo, h, mi);
    else
        snprintf(out, size, "%s", in ? in : "");
}

/* ---------- ya mostradas (no se repiten) ---------- */

static bool seen_has(const char *key) {
    FILE *f = fopen(SEEN_FILE, "r");
    char  line[256];
    bool  found = false;

    if (!f) return false;
    while (!found && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        found = !strcmp(line, key);
    }
    fclose(f);
    return found;
}

static void seen_add(const char *key) {
    char  lines[60][256];
    int   n = 0;
    FILE *f = fopen(SEEN_FILE, "r");

    if (f) {
        while (n < 59 && fgets(lines[n], sizeof(lines[n]), f)) {
            lines[n][strcspn(lines[n], "\n")] = 0;
            if (lines[n][0]) n++;
        }
        fclose(f);
    }
    mkdir(SEEN_DIR, 0755);
    f = fopen(SEEN_FILE, "w");
    if (!f) return;
    for (int i = n > 49 ? n - 49 : 0; i < n; i++) fprintf(f, "%s\n", lines[i]);
    fprintf(f, "%s\n", key);
    fclose(f);
}

/* ---------- el recuadro ---------- */

static void popup_hide(void) {
    if (popup_timer) {
        lv_timer_del(popup_timer);
        popup_timer = NULL;
    }
    if (popup) {
        lv_obj_del(popup);
        popup = NULL;
    }
}

static void popup_timer_cb(lv_timer_t *t) {
    (void)t;
    popup_timer = NULL;     /* una sola vez: LVGL lo borra */
    if (popup) {
        lv_obj_del(popup);
        popup = NULL;
    }
}

static void popup_show(cJSON *a) {
    int         level = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(a, "level"));
    const char *kind  = cJSON_GetStringValue(cJSON_GetObjectItem(a, "kind"));
    const char *fen   = cJSON_GetStringValue(cJSON_GetObjectItem(a, "fenomeno"));
    const char *zones = cJSON_GetStringValue(cJSON_GetObjectItem(a, "zones"));
    char        from[24], to[24];
    uint32_t    bg  = meteo_level_color(level);
    uint32_t    fg  = level == 4 ? 0xffffff : 0x000000;

    meteo_short_time(cJSON_GetStringValue(cJSON_GetObjectItem(a, "from")), from, sizeof(from));
    meteo_short_time(cJSON_GetStringValue(cJSON_GetObjectItem(a, "to")), to, sizeof(to));

    popup_hide();
    popup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(popup);
    lv_obj_set_pos(popup, 70, 150);
    lv_obj_set_size(popup, 660, 128);
    lv_obj_set_style_bg_color(popup, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(popup, 245, 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(fg), 0);
    lv_obj_set_style_border_width(popup, 2, 0);
    lv_obj_set_style_radius(popup, theme_shape.radius_popup, 0);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(popup);
    lv_obj_set_style_text_font(l, &sony_28, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), 0);
    lv_label_set_text_fmt(l, "%s  -  INUMET", meteo_level_name(level, kind));
    lv_obj_set_pos(l, 14, 8);

    l = lv_label_create(popup);
    lv_obj_set_style_text_font(l, &sony_22, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, 632);
    lv_label_set_text_fmt(l, "%s  -  %s", fen && fen[0] ? fen : tr("Weather"), zones ? zones : "");
    lv_obj_set_pos(l, 14, 44);

    l = lv_label_create(popup);
    lv_obj_set_style_text_font(l, &sony_20, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(fg), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, 632);
    lv_label_set_text_fmt(l, "%s %s  %s %s   -   APP > Meteo", tr("from"), from, tr("to"), to);
    lv_obj_set_pos(l, 14, 80);

    popup_timer = lv_timer_create(popup_timer_cb, POPUP_MS, NULL);
    lv_timer_set_repeat_count(popup_timer, 1);
}

/* Las alertas del archivo: la primera nueva del departamento sale en pantalla */
void meteo_alerts_check_file(void) {
    FILE *f = fopen(ALERTS_FILE, "r");
    char *text;
    long  len;

    if (!f) return;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    text = len > 0 && len < 512 * 1024 ? malloc(len + 1) : NULL;
    if (text) text[fread(text, 1, len, f)] = 0;
    fclose(f);
    if (!text) return;
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) return;

    cJSON *a, *show = NULL;
    cJSON_ArrayForEach(a, cJSON_GetObjectItem(root, "alerts")) {
        if (!cJSON_IsTrue(cJSON_GetObjectItem(a, "mine"))) continue;
        char key[256];
        snprintf(key, sizeof(key), "%d|%s|%s|%s", (int)cJSON_GetNumberValue(cJSON_GetObjectItem(a, "level")),
                 cJSON_GetStringValue(cJSON_GetObjectItem(a, "fenomeno")) ?: "",
                 cJSON_GetStringValue(cJSON_GetObjectItem(a, "from")) ?: "",
                 cJSON_GetStringValue(cJSON_GetObjectItem(a, "to")) ?: "");
        for (char *p = key; *p; p++)
            if (*p == '\n') *p = ' ';
        if (seen_has(key)) continue;
        seen_add(key);
        if (!show || cJSON_GetNumberValue(cJSON_GetObjectItem(a, "level")) >
                     cJSON_GetNumberValue(cJSON_GetObjectItem(show, "level")))
            show = a;       /* la más grave de las nuevas */
    }
    if (show) popup_show(show);
    cJSON_Delete(root);
}

/* ---------- consulta periódica ---------- */

static void check_done(void *arg) {
    (void)arg;
    busy = false;
    meteo_alerts_check_file();
}

static void *check_thread(void *arg) {
    char *cmd = arg;
    FILE *p   = popen(cmd, "r");
    char  line[160];

    free(cmd);
    if (p) {
        while (fgets(line, sizeof(line), p)) {}
        pclose(p);
    }
    scheduler_put_noargs(check_done);
    return NULL;
}

static void timer_cb(lv_timer_t *t) {
    (void)t;
    time_t now = time(NULL);
    double lat, lon;
    char   cmd[160];

    if (busy || !params.meteo_alerts.x || wifi_get_status() != WIFI_CONNECTED) return;
    if (last_check && now - last_check < CHECK_S) return;
    last_check = now;
    meteo_position(&lat, &lon);
    snprintf(cmd, sizeof(cmd), HELPER " alerts %.4f %.4f 2>/dev/null", lat, lon);

    pthread_t th;
    busy = true;
    if (pthread_create(&th, NULL, check_thread, strdup(cmd)) == 0) {
        pthread_detach(th);
    } else {
        busy = false;
    }
}

void meteo_alerts_init(void) {
    lv_timer_create(timer_cb, 60 * 1000, NULL);
}

void meteo_alerts_now(void) {
    last_check = 0;
}
