/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: estación meteorológica. La observación de la estación INUMET más cercana (temperatura,
 *  humedad, rocío, presión y su tendencia, viento, ráfagas, lluvia, visibilidad), lo que agrega
 *  Open-Meteo (estado del cielo, nubes, UV, sol, pronóstico de 5 días) y el pronóstico y las
 *  alertas de INUMET para la zona. Los datos los trae x6100-meteo; se actualiza cada 10 minutos.
 */
#include "dialog_meteo.h"

#include "buttons.h"
#include "cJSON.h"
#include "dialog.h"
#include "events.h"
#include "keyboard.h"
#include "meteo_alerts.h"
#include "msg.h"
#include "radio.h"
#include "scheduler.h"
#include "styles.h"
#include "theme.h"
#include "i18n.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define HELPER     "/usr/sbin/x6100-meteo"
#define DATA_FILE  "/tmp/x6100-meteo.json"
#define REFRESH_S  (10 * 60)

typedef enum { V_NOW, V_FORECAST, V_ALERTS } view_t;

static view_t      view = V_NOW;
static cJSON      *data;            /* lo último que trajo x6100-meteo */
static bool        updating;
static int         gen;             /* descarta respuestas de una apertura anterior */
static char        upd_error[120];
static lv_obj_t   *box;             /* el contenido de la vista */
static lv_obj_t   *status_lbl;
static lv_timer_t *refresh_timer;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);
static void now_cb(button_data_t *b);
static void forecast_cb(button_data_t *b);
static void alerts_cb(button_data_t *b);
static void update_cb(button_data_t *b);

static button_data_t btn_now      = {.type = BTN_TEXT, .label = "Now", .press = now_cb};
static button_data_t btn_forecast = {.type = BTN_TEXT, .label = "Forecast", .press = forecast_cb};
static button_data_t btn_alerts   = {.type = BTN_TEXT, .label = "Alerts", .press = alerts_cb};
static button_data_t btn_update   = {.type = BTN_TEXT, .label = "Update", .press = update_cb};

static buttons_page_t btn_page = {{&btn_now, &btn_forecast, &btn_alerts, &btn_update}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = NULL,
    .btn_page     = &btn_page,
    .key_cb       = key_cb,
};

dialog_t *dialog_meteo = &dialog;

/* ---------- valores ---------- */

static cJSON *obj(cJSON *o, const char *k) {
    return o ? cJSON_GetObjectItemCaseSensitive(o, k) : NULL;
}

static bool numv(cJSON *o, const char *k, double *v) {
    cJSON *j = obj(o, k);
    if (!cJSON_IsNumber(j)) return false;
    *v = j->valuedouble;
    return true;
}

static const char *strv(cJSON *o, const char *k) {
    const char *s = cJSON_GetStringValue(obj(o, k));
    return s ? s : "";
}

/* Con coma decimal, como se escribe acá; "--" si no hay dato */
static const char *fmt(char *buf, size_t size, double v, int dec, const char *unit) {
    snprintf(buf, size, "%.*f%s", dec, v, unit);
    for (char *p = buf; *p; p++)
        if (*p == '.') *p = ',';
    return buf;
}

/* De la estación INUMET y si no, de Open-Meteo */
static bool cur(const char *k, double *v) {
    return numv(obj(data, "station"), k, v) || numv(obj(data, "om"), k, v);
}

static const char *val(char *buf, size_t size, const char *k, int dec, const char *unit) {
    double v;
    return cur(k, &v) ? fmt(buf, size, v, dec, unit) : "--";
}

static const char *uv_text(double uv) {
    return uv < 3 ? tr("low") : uv < 6 ? tr("moderate") : uv < 8 ? tr("high") : uv < 11 ? tr("very high") : tr("extreme");
}

/* ---------- dibujo ---------- */

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, lv_coord_t x, lv_coord_t y, lv_coord_t w) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_recolor(l, true);
    if (w > 0) {
        lv_obj_set_width(l, w);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    }
    lv_obj_set_pos(l, x, y);
    return l;
}

/* Una medida: rótulo chico arriba, valor abajo */
static void tile(lv_coord_t x, lv_coord_t y, const char *name, const char *value) {
    lv_label_set_text(label(box, &sony_18, theme_hex(TC_text_dim), x, y, 172), name);
    lv_label_set_text(label(box, &sony_24, theme_hex(TC_text_value), x, y + 20, 172), value);
}

/* Las alertas para mi departamento: la más grave en una franja de su color */
static void alert_strip(lv_coord_t y) {
    cJSON *a, *worst = NULL;
    int    mine = 0;

    cJSON_ArrayForEach(a, obj(data, "alerts")) {
        if (!cJSON_IsTrue(obj(a, "mine"))) continue;
        mine++;
        double l1 = 0, l2 = 0;
        numv(a, "level", &l1);
        if (worst) numv(worst, "level", &l2);
        if (!worst || l1 > l2) worst = a;
    }
    lv_obj_t *strip = lv_obj_create(box);
    lv_obj_remove_style_all(strip);
    lv_obj_set_pos(strip, 6, y);
    lv_obj_set_size(strip, 770, 32);
    lv_obj_set_style_radius(strip, 4, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
    if (!worst) {
        lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
        lv_label_set_text_fmt(label(strip, &sony_20, theme_hex(TC_ok), 8, 5, 754), "%s %s",
                              tr("No INUMET alerts for"), strv(data, "dept"));
        return;
    }
    double level = 0;
    char   to[24];
    numv(worst, "level", &level);
    meteo_short_time(strv(worst, "to"), to, sizeof(to));
    lv_obj_set_style_bg_color(strip, lv_color_hex(meteo_level_color((int)level)), 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_label_set_text_fmt(label(strip, &sony_20, level == 4 ? 0xffffff : 0x000000, 8, 5, 754), "%s - %s - %s %s   %s",
                          meteo_level_name((int)level, strv(worst, "kind")), strv(worst, "fenomeno"), tr("to"), to,
                          mine > 1 ? tr("(more: Alerts)") : "");
}

static void draw_now(void) {
    char   b1[32], b2[32], b3[48];
    double t, v, w;

    /* izquierda: la temperatura grande, el cielo y la sensación */
    lv_obj_t *l = label(box, &sony_60, theme_hex(TC_text), 10, 0, 0);
    lv_label_set_text(l, cur("temp", &t) ? fmt(b1, sizeof(b1), t, 1, "°") : "--°");
    const char *sky = strv(obj(data, "om"), "text");
    lv_label_set_text(label(box, &sony_24, theme_hex(TC_accent), 10, 70, 240), sky[0] ? sky : " ");
    lv_label_set_text_fmt(label(box, &sony_20, theme_hex(TC_text_soft), 10, 100, 240), "%s %s", tr("Feels"),
                          val(b1, sizeof(b1), "feels", 1, "°"));
    {
        double lo, hi;
        cJSON *d0 = cJSON_GetArrayItem(obj(data, "days"), 0);
        if (numv(d0, "tmin", &lo) && numv(d0, "tmax", &hi))
            lv_label_set_text_fmt(label(box, &sony_20, theme_hex(TC_text_soft), 10, 126, 240), "%s %s / %s %s",
                                  tr("Min"), fmt(b1, sizeof(b1), lo, 0, "°"), tr("Max"), fmt(b2, sizeof(b2), hi, 0, "°"));
    }
    cJSON *st = obj(data, "station");
    if (st)
        lv_label_set_text_fmt(label(box, &sony_18, theme_hex(TC_text_dim), 10, 156, 240), "INUMET %s (%d km) %s",
                              strv(st, "name"), (int)cJSON_GetNumberValue(obj(st, "dist_km")), strv(st, "time"));
    else
        lv_label_set_text(label(box, &sony_18, theme_hex(TC_text_dim), 10, 156, 240), "Open-Meteo");

    /* derecha: las medidas, tres columnas */
    const lv_coord_t X[3] = {262, 438, 614}, Y[4] = {0, 50, 100, 150};

    tile(X[0], Y[0], tr("Humidity"), val(b1, sizeof(b1), "hum", 0, " %"));
    tile(X[1], Y[0], tr("Dew point"), val(b1, sizeof(b1), "dew", 1, "°"));
    if (cur("press", &v)) {
        double trend;
        /* tendencia en 3 h: sube, baja o estable (más de medio hPa) */
        if (numv(st, "press_trend", &trend))
            snprintf(b3, sizeof(b3), "%s %s", fmt(b1, sizeof(b1), v, 1, ""),
                     trend > 0.5 ? LV_SYMBOL_UP : trend < -0.5 ? LV_SYMBOL_DOWN : "=");
        else
            fmt(b3, sizeof(b3), v, 1, "");
    } else {
        snprintf(b3, sizeof(b3), "--");
    }
    tile(X[2], Y[0], tr("Pressure hPa"), b3);
    if (cur("wind_kmh", &w)) {
        const char *card = strv(st && cJSON_IsNumber(obj(st, "wind_kmh")) ? st : obj(data, "om"), "wind_card");
        snprintf(b3, sizeof(b3), "%s %s", card, fmt(b1, sizeof(b1), w, 0, " km/h"));
    } else {
        snprintf(b3, sizeof(b3), "--");
    }
    tile(X[0], Y[1], tr("Wind"), b3);
    tile(X[1], Y[1], tr("Gusts"), val(b1, sizeof(b1), "gust_kmh", 0, " km/h"));
    {
        double r1 = 0, r24 = 0;
        bool   h1 = numv(st, "rain_1h", &r1), h24 = numv(st, "rain_24h", &r24);
        if (h1 || h24)
            snprintf(b3, sizeof(b3), "%s / %s", h1 ? fmt(b1, sizeof(b1), r1, 1, "") : "-",
                     h24 ? fmt(b2, sizeof(b2), r24, 1, " mm") : "- mm");
        else
            snprintf(b3, sizeof(b3), "%s", val(b1, sizeof(b1), "precip", 1, " mm"));
        tile(X[2], Y[1], tr("Rain 1h / 24h"), b3);
    }
    tile(X[0], Y[2], tr("Visibility"), val(b1, sizeof(b1), "vis_km", 0, " km"));
    tile(X[1], Y[2], tr("Clouds"), numv(obj(data, "om"), "cloud", &v) ? fmt(b1, sizeof(b1), v, 0, " %") : "--");
    if (numv(obj(data, "om"), "uv", &v))
        snprintf(b3, sizeof(b3), "%s %s", fmt(b1, sizeof(b1), v, 1, ""), uv_text(v));
    else
        snprintf(b3, sizeof(b3), "--");
    tile(X[2], Y[2], "UV", b3);
    cJSON *sun = obj(data, "sun");
    snprintf(b3, sizeof(b3), LV_SYMBOL_UP "%s  " LV_SYMBOL_DOWN "%s", strv(sun, "rise"), strv(sun, "set"));
    tile(X[0], Y[3], tr("Sun"), sun ? b3 : "--");
    {
        double lo, hi;
        if (numv(st, "tmin24", &lo) && numv(st, "tmax24", &hi))
            snprintf(b3, sizeof(b3), "%s / %s", fmt(b1, sizeof(b1), lo, 1, "°"), fmt(b2, sizeof(b2), hi, 1, "°"));
        else
            snprintf(b3, sizeof(b3), "--");
        tile(X[1], Y[3], tr("Last 24 h"), b3);
    }
    {
        cJSON *d1 = cJSON_GetArrayItem(obj(data, "days"), 1);
        double p;
        if (d1 && numv(d1, "prob", &p))
            snprintf(b3, sizeof(b3), "%s %s", fmt(b1, sizeof(b1), p, 0, "%"), strv(d1, "text"));
        else
            snprintf(b3, sizeof(b3), "--");
        tile(X[2], Y[3], tr("Tomorrow"), b3);
    }
    alert_strip(204);
}

static void draw_forecast(void) {
    char   b1[24], b2[24];
    cJSON *d;
    int    i = 0;

    /* cinco días (Open-Meteo): máxima y mínima, cielo, probabilidad y milímetros, viento */
    cJSON_ArrayForEach(d, obj(data, "days")) {
        if (i == 5) break;
        lv_coord_t x = 8 + i * 156;
        double     lo, hi, p, mm, wmax;
        lv_label_set_text(label(box, &sony_22, theme_hex(TC_text_head), x, 0, 150), strv(d, "dow"));
        if (numv(d, "tmax", &hi) && numv(d, "tmin", &lo))
            lv_label_set_text_fmt(label(box, &sony_28, theme_hex(TC_text), x, 26, 150), "%s #%06x %s#",
                                  fmt(b1, sizeof(b1), hi, 0, "°"), theme_hex(TC_text_dim), fmt(b2, sizeof(b2), lo, 0, "°"));
        lv_obj_t *l = label(box, &sony_18, theme_hex(TC_accent), x, 60, 0);
        lv_obj_set_width(l, 148);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_label_set_text(l, strv(d, "text"));
        if (numv(d, "prob", &p) && numv(d, "rain_mm", &mm))
            lv_label_set_text_fmt(label(box, &sony_18, theme_hex(TC_info), x, 104, 150), "%s  %s",
                                  fmt(b1, sizeof(b1), p, 0, "%"), fmt(b2, sizeof(b2), mm, 1, " mm"));
        if (numv(d, "wind_max", &wmax))
            lv_label_set_text_fmt(label(box, &sony_18, theme_hex(TC_text_dim), x, 126, 150), "%s %s", tr("Wind"),
                                  fmt(b1, sizeof(b1), wmax, 0, " km/h"));
        i++;
    }
    if (!i) lv_label_set_text(label(box, &sony_22, theme_hex(TC_text_dim), 8, 0, 760), tr("No forecast yet"));

    /* el texto de INUMET para la zona, hoy y mañana */
    cJSON *zone = obj(data, "zone");
    if (!zone) return;
    lv_obj_t *t = lv_label_create(box);
    lv_obj_set_style_text_font(t, &sony_18, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(theme_hex(TC_text_value)), 0);
    lv_label_set_recolor(t, true);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, 764);
    lv_obj_set_pos(t, 8, 152);

    static char txt[2048];
    int         n = snprintf(txt, sizeof(txt), "#%06x INUMET - %s#\n", theme_hex(TC_text_head), strv(zone, "name"));
    int         k = 0;
    cJSON_ArrayForEach(d, obj(zone, "days")) {
        if (k++ == 2) break;
        cJSON *p;
        cJSON_ArrayForEach(p, obj(d, "periods")) {
            if (n >= (int)sizeof(txt) - 10) break;
            n += snprintf(txt + n, sizeof(txt) - n, "#%06x %s %s:# %s %s\n", theme_hex(TC_accent), strv(d, "label"),
                          strv(p, "p"), strv(p, "text"), strv(p, "wind"));
        }
    }
    lv_label_set_text(t, txt);
}

static void draw_alerts(void) {
    lv_obj_t *t = lv_label_create(box);
    lv_obj_set_style_text_font(t, &sony_20, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(theme_hex(TC_text)), 0);
    lv_label_set_recolor(t, true);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, 764);
    lv_obj_set_pos(t, 8, 0);

    static char txt[6144];
    int         n = 0, count = 0;

    /* primero las del departamento, después las demás */
    for (int pass = 0; pass < 2; pass++) {
        cJSON *a;
        cJSON_ArrayForEach(a, obj(data, "alerts")) {
            if (cJSON_IsTrue(obj(a, "mine")) != (pass == 0)) continue;
            if (n >= (int)sizeof(txt) - 64) break;
            double level = 0;
            char   from[24], to[24];
            numv(a, "level", &level);
            meteo_short_time(strv(a, "from"), from, sizeof(from));
            meteo_short_time(strv(a, "to"), to, sizeof(to));
            uint32_t c = level >= 2 ? meteo_level_color((int)level) : theme_hex(TC_warn);
            n += snprintf(txt + n, sizeof(txt) - n, "#%06x %s - %s#  %s\n", c, meteo_level_name((int)level, strv(a, "kind")),
                          strv(a, "fenomeno"), pass == 0 ? tr("(your department)") : "");
            n += snprintf(txt + n, sizeof(txt) - n, "%s %s  %s %s   %s\n%s\n#%06x %s#\n\n", tr("from"), from, tr("to"), to,
                          strv(a, "prob"), strv(a, "desc"), theme_hex(TC_text_dim), strv(a, "zones"));
            count++;
        }
    }
    if (!count)
        snprintf(txt, sizeof(txt), "#%06x %s#", theme_hex(TC_ok), tr("No weather warnings in force from INUMET"));
    lv_label_set_text(t, txt);
}

static void draw(void) {
    if (!box) return;
    lv_obj_clean(box);
    lv_obj_scroll_to_y(box, 0, LV_ANIM_OFF);
    if (!data) {
        lv_label_set_text(label(box, &sony_24, theme_hex(TC_text_dim), 10, 10, 760),
                          updating ? tr("Getting the weather...") : upd_error[0] ? upd_error : tr("No data: Update (needs WiFi)"));
        return;
    }
    switch (view) {
        case V_NOW:      draw_now(); break;
        case V_FORECAST: draw_forecast(); break;
        default:         draw_alerts(); break;
    }
}

static void show_status(void) {
    if (!status_lbl) return;
    const char *names[] = {tr("Now"), tr("Forecast"), tr("Alerts")};
    char        extra[160] = "";

    if (updating)
        snprintf(extra, sizeof(extra), "  -  %s", tr("updating..."));
    else if (upd_error[0])
        snprintf(extra, sizeof(extra), "  -  #%06x %s#", theme_hex(TC_err), upd_error);
    else if (data)
        snprintf(extra, sizeof(extra), "  -  %s %s", tr("updated"), strv(data, "time"));
    lv_label_set_text_fmt(status_lbl, "#%06x %s# #%06x %s - %s#%s", theme_hex(TC_text_head), tr("WEATHER"),
                          theme_hex(TC_accent), names[view], data ? strv(data, "dept") : "", extra);
}

/* ---------- datos ---------- */

static void load_file(void) {
    FILE *f = fopen(DATA_FILE, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = len > 0 && len < 1024 * 1024 ? malloc(len + 1) : NULL;
    if (text) text[fread(text, 1, len, f)] = 0;
    fclose(f);
    if (!text) return;
    cJSON *j = cJSON_Parse(text);
    free(text);
    if (!j) return;
    cJSON_Delete(data);
    data = j;
}

typedef struct {
    int  gen;
    char err[120];
} upd_result_t;

static void update_done(void *arg) {
    upd_result_t *r = arg;
    if (r->gen != gen || !dialog.run) return;
    updating = false;
    snprintf(upd_error, sizeof(upd_error), "%s", r->err);
    load_file();
    meteo_alerts_check_file();      /* una alerta nueva sale también acá */
    draw();
    show_status();
}

static void *update_thread(void *arg) {
    upd_result_t r = {.gen = (int)(intptr_t)arg};
    double       lat, lon;
    char         cmd[160], line[200] = "";

    meteo_position(&lat, &lon);
    snprintf(cmd, sizeof(cmd), HELPER " update %.4f %.4f 2>/dev/null", lat, lon);
    FILE *p = popen(cmd, "r");
    if (p) {
        char l[200];
        while (fgets(l, sizeof(l), p)) snprintf(line, sizeof(line), "%s", l);
        pclose(p);
    }
    line[strcspn(line, "\n")] = 0;
    if (!strncmp(line, "error ", 6))
        snprintf(r.err, sizeof(r.err), "%s", line + 6);
    else if (strncmp(line, "ok", 2))
        snprintf(r.err, sizeof(r.err), "%s", tr("No data: is WiFi connected?"));
    scheduler_put(update_done, &r, sizeof(r));
    return NULL;
}

static void start_update(void) {
    pthread_t t;
    if (updating) return;
    updating     = true;
    upd_error[0] = 0;
    show_status();
    if (!data) draw();
    if (pthread_create(&t, NULL, update_thread, (void *)(intptr_t)gen) == 0) {
        pthread_detach(t);
    } else {
        updating = false;
    }
}

static void refresh_cb(lv_timer_t *t) {
    (void)t;
    start_update();
}

/* ---------- botones y teclas ---------- */

static void set_view(view_t v) {
    view = v;
    draw();
    show_status();
}

static void now_cb(button_data_t *b) { (void)b; set_view(V_NOW); }
static void forecast_cb(button_data_t *b) { (void)b; set_view(V_FORECAST); }
static void alerts_cb(button_data_t *b) { (void)b; set_view(V_ALERTS); }
static void update_cb(button_data_t *b) { (void)b; start_update(); }

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct();
            break;
        case LV_KEY_UP:
        case LV_KEY_LEFT:
            if (box) lv_obj_scroll_by(box, 0, 60, LV_ANIM_ON);
            break;
        case LV_KEY_DOWN:
        case LV_KEY_RIGHT:
            if (box) lv_obj_scroll_by(box, 0, -60, LV_ANIM_ON);
            break;
        case LV_KEY_ENTER:
            set_view((view + 1) % 3);
            break;
        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;
        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
    }
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);
    gen++;

    status_lbl = lv_label_create(dialog.obj);
    lv_label_set_recolor(status_lbl, true);
    lv_label_set_long_mode(status_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(status_lbl, 770);
    lv_obj_set_style_text_font(status_lbl, &sony_20, 0);
    lv_obj_set_style_text_color(status_lbl, TC(text_dim), 0);
    lv_obj_set_pos(status_lbl, 14, 4);

    box = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, 6, 30);
    lv_obj_set_size(box, 788, DIALOG_H - 36);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);
    lv_obj_add_event_cb(box, key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, box);
    lv_group_set_editing(keyboard_group, true);

    buttons_unload_page();
    buttons_load_page(&btn_page);

    load_file();
    struct stat sb;
    bool fresh = stat(DATA_FILE, &sb) == 0 && time(NULL) - sb.st_mtime < REFRESH_S;
    draw();
    show_status();
    if (!fresh) start_update();
    refresh_timer = lv_timer_create(refresh_cb, REFRESH_S * 1000, NULL);
}

static void destruct_cb(void) {
    gen++;
    updating = false;
    if (refresh_timer) lv_timer_del(refresh_timer);
    refresh_timer = NULL;
    box = status_lbl = NULL;
}
