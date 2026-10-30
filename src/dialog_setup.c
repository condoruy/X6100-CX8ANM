/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: asistente del primer encendido (dialog_setup.h).
 *
 *  1. Idioma      la lista con el nombre de cada idioma en ese idioma; al pasar al paso 2 la
 *                 interfaz se arma de nuevo en el idioma elegido.
 *  2. Licencia    el indicativo con el teclado de la pantalla, o "Sin licencia" (solo escucha).
 *  3. Región      IARU 1, 2 o 3, y a la derecha quiénes están en cada una.
 *  4. País        los de la región elegida; pone la hora del país, el locator de su capital si
 *                 no había y el país de la radio por Internet.
 *
 *  La perilla MFK elige, apretarla (o Siguiente) pasa al paso que sigue, ESC vuelve uno atrás.
 */
#include "dialog_setup.h"

#include "band_info.h"
#include "band_plan.h"
#include "buttons.h"
#include "clock.h"
#include "countries.h"
#include "dialog.h"
#include "events.h"
#include "i18n.h"
#include "keyboard.h"
#include "main_screen.h"
#include "msg.h"
#include "params/db.h"
#include "params/params.h"
#include "radio.h"
#include "station.h"
#include "styles.h"
#include "textarea_window.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef enum { S_LANG, S_CALL, S_REGION, S_COUNTRY, S_COUNT } step_t;

static setup_mode_t mode;
static step_t       step;
static int          sel_lang, sel_region = 2;
static char         sel_country[3];
static bool         kbd_open;

static lv_obj_t *title, *hint, *table, *info_box, *info, *call_lbl;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);
static void back_cb(button_data_t *b);
static void next_cb(button_data_t *b);
static void edit_cb(button_data_t *b);
static void nocall_cb(button_data_t *b);
static void cancel_cb(button_data_t *b);

static button_data_t btn_back   = {.type = BTN_TEXT, .label = "Back", .press = back_cb};
static button_data_t btn_next   = {.type = BTN_TEXT, .label = "Next", .press = next_cb};
static button_data_t btn_finish = {.type = BTN_TEXT, .label = "Finish", .press = next_cb};
static button_data_t btn_edit   = {.type = BTN_TEXT, .label = "Type\ncallsign", .press = edit_cb};
static button_data_t btn_nocall = {.type = BTN_TEXT, .label = "No license\n(listen)", .press = nocall_cb};
static button_data_t btn_save   = {.type = BTN_TEXT, .label = "Save", .press = next_cb};
static button_data_t btn_cancel = {.type = BTN_TEXT, .label = "Cancel", .press = cancel_cb};

static buttons_page_t page_lang    = {{&btn_next}};
static buttons_page_t page_call    = {{&btn_back, &btn_edit, &btn_nocall, &btn_next}};
static buttons_page_t page_region  = {{&btn_back, &btn_next}};
static buttons_page_t page_country = {{&btn_back, &btn_finish}};
static buttons_page_t page_single  = {{&btn_save, &btn_cancel}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = NULL,
    .btn_page     = &page_lang,
    .key_cb       = NULL,
};

dialog_t *dialog_setup = &dialog;

/* ---------- aplicar ---------- */

void region_apply(int region) {
    /* 160 m, 80 m, 40 m (y la radiodifusión de 41 m que empieza donde termina), 6 m */
    static const struct { int32_t b160, e80, e40, e6; } r[4] = {
        {0},
        {1810000, 3800000, 7200000, 52000000},
        {1800000, 4000000, 7300000, 54000000},
        {1800000, 3900000, 7200000, 54000000},
    };
    char sql[600];

    if (region < 1 || region > 3) return;
    snprintf(sql, sizeof(sql),
             "BEGIN;"
             "UPDATE bands SET start_freq = %d WHERE id = 1;"
             "UPDATE bands SET stop_freq = %d WHERE id = 2;"
             "UPDATE bands SET stop_freq = %d WHERE id = 4;"
             "UPDATE bands SET start_freq = %d WHERE id = 103 AND type = 0;"
             "UPDATE bands SET stop_freq = %d WHERE id = 12;"
             "COMMIT;",
             r[region].b160, r[region].e80, r[region].e40, r[region].e40, r[region].e6);
    if (!sql_query_exec(sql)) sql_query_exec("ROLLBACK;");
    params_uint8_set(&params.region, region);
    band_info_reload();
    band_plan_refresh();
}

void country_apply(const char *cc) {
    const country_t *c = country_find(cc);

    if (!c) return;
    params_str_set(&params.country, c->cc);
    if (c->tz[0]) clock_tz_set(c->tz);
    if (!params.qth.x[0] && c->loc[0]) params_str_set(&params.qth, c->loc);
    unlink("/mnt/iradio/country");      /* la radio por Internet sigue al país elegido */
}

/* ---------- pantalla ---------- */

static int rows(void) {
    switch (step) {
        case S_LANG:    return LANG_COUNT;
        case S_REGION:  return 3;
        case S_COUNTRY: return countries_in_region(sel_region);
        default:        return 0;
    }
}

static int selected(void) {
    uint16_t row = 0, col = 0;

    if (!table) return -1;
    lv_table_get_selected_cell(table, &row, &col);
    return row == LV_TABLE_CELL_NONE || row >= rows() ? -1 : row;
}

static void select_row(int r) {
    lv_table_t *t = (lv_table_t *)table;
    lv_coord_t  h = lv_obj_get_height(table);
    int         y = r * 34 - h / 2 + 17;

    if (r < 0) r = 0;
    t->row_act = r;
    t->col_act = 0;
    lv_obj_invalidate(table);
    lv_obj_scroll_to_y(table, y > 0 ? y : 0, LV_ANIM_OFF);
}

static void show_info(void) {
    int r = selected();

    if (!info) return;
    switch (step) {
        case S_LANG: {
            static const char *hello[LANG_COUNT] = {
                "Welcome! Choose your language.", "¡Bienvenido! Elija su idioma.", "Willkommen! Wählen Sie Ihre Sprache.",
                "Bienvenue ! Choisissez votre langue.", "Benvenuto! Scegli la tua lingua."};
            static char buf[400];
            size_t      n = 0;
            for (int i = 0; i < LANG_COUNT; i++)
                n += i == r ? snprintf(buf + n, sizeof(buf) - n, "#%06x %s#\n\n", theme_hex(TC_accent), hello[i])
                            : snprintf(buf + n, sizeof(buf) - n, "%s\n\n", hello[i]);
            lv_obj_set_style_text_font(info, &sony_22, 0);
            lv_label_set_text(info, buf);
            break;
        }
        case S_REGION: {
            static char buf[4096];
            int         region = r + 1;
            size_t      n;

            if (r < 0) break;
            n = snprintf(buf, sizeof(buf), "#%06x %s#\n%s\n\n#%06x %s (%d):#\n", theme_hex(TC_text_head),
                         region_title(region), region_summary(region), theme_hex(TC_text_head),
                         tr("Countries in this region"), countries_in_region(region));
            for (int i = 0; i < countries_in_region(region) && n < sizeof(buf) - 64; i++)
                n += snprintf(buf + n, sizeof(buf) - n, "%s%s", i ? ", " : "", country_name(country_sorted(region, i)));
            lv_obj_set_style_text_font(info, &sony_16, 0);
            lv_label_set_text(info, buf);
            lv_obj_scroll_to_y(info_box, 0, LV_ANIM_OFF);
            break;
        }
        case S_COUNTRY: {
            const country_t *c = r >= 0 ? country_sorted(sel_region, r) : NULL;
            if (!c) break;
            lv_obj_set_style_text_font(info, &sony_18, 0);
            lv_label_set_text_fmt(info, "#%06x %s#\n\n%s\n%s: %s\n%s: %s\n\n%s", theme_hex(TC_text_head), country_name(c),
                                  region_title(c->region), tr("ISO code"), c->cc, tr("Locator of the capital"),
                                  c->loc[0] ? c->loc : "-",
                                  tr("The clock takes this country's time, and Internet radio its stations."));
            break;
        }
        default:
            break;
    }
}

static void fill(void) {
    int n = rows(), cur = 0;

    lv_table_set_row_cnt(table, n ? n : 1);
    lv_table_set_cell_value(table, 0, 0, "");
    for (int i = 0; i < n; i++) {
        switch (step) {
            case S_LANG:
                lv_table_set_cell_value(table, i, 0, i18n_lang_name(i));
                break;
            case S_REGION:
                lv_table_set_cell_value(table, i, 0, region_title(i + 1));
                break;
            case S_COUNTRY:
                lv_table_set_cell_value(table, i, 0, country_name(country_sorted(sel_region, i)));
                break;
            default:
                break;
        }
    }
    switch (step) {
        case S_LANG:    cur = sel_lang; break;
        case S_REGION:  cur = sel_region - 1; break;
        case S_COUNTRY: cur = country_sorted_index(sel_region, sel_country); break;
        default:        break;
    }
    select_row(cur >= 0 ? cur : 0);
}

static void show_step(void) {
    static const char *names[S_COUNT] = {"Language", "License (callsign)", "Region", "Country"};
    static const char *hints[S_COUNT] = {
        "Choose the language with the MFK knob and press it",
        "Your amateur radio callsign: it is used by FT8, JS8, the log, the cluster and the spots. Without a license you can still listen",
        "The IARU region sets the band edges and the band plan shown under the spectrum",
        "Choose your country: it sets the clock and the Internet radio stations",
    };
    buttons_page_t *page;

    if (mode == SETUP_ALL)
        lv_label_set_text_fmt(title, "#%06x %s#   #%06x %s %d/4:# %s", theme_hex(TC_accent), tr("Welcome"),
                              theme_hex(TC_text_dim), tr("Step"), step + 1, tr(names[step]));
    else
        lv_label_set_text_fmt(title, "#%06x %s#", theme_hex(TC_accent), tr(names[step]));
    lv_label_set_text(hint, tr(hints[step]));

    bool list = step != S_CALL;
    if (list) {
        lv_obj_clear_flag(table, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(info_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(call_lbl, LV_OBJ_FLAG_HIDDEN);
        fill();
        show_info();
    } else {
        lv_obj_add_flag(table, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(info_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(call_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(call_lbl, "%s\n#%06x %s#", tr("Callsign"), theme_hex(TC_accent),
                              params.callsign.x[0] ? params.callsign.x : tr("none (listener)"));
    }

    if (mode != SETUP_ALL) page = &page_single;
    else page = step == S_LANG ? &page_lang : step == S_CALL ? &page_call : step == S_REGION ? &page_region : &page_country;
    dialog.btn_page = page;
    buttons_unload_page();
    buttons_load_page(page);
}

/* ---------- pasos ---------- */

static void finish(void) {
    params_bool_set(&params.setup_done, true);
    params_flush();
    msg_update_text_fmt("%s: %s, %s", tr("Saved"), region_title(params.region.x), country_name_cc(params.country.x));
    dialog_destruct();
}

/* el idioma cambió: la pantalla principal se arma de nuevo y el asistente vuelve en el paso 2 */
static void relang(void *arg) {
    (void)arg;
    main_screen_set_theme(params.theme_name.x);
    step = S_CALL;
    main_screen_open_dialog(dialog_setup);
}

static void go_next(void) {
    int r = selected();

    switch (step) {
        case S_LANG:
            if (r < 0) return;
            sel_lang = r;
            if (r != params.language.x) {
                params_uint8_set(&params.language, r);
                params_flush();
                lv_async_call(relang, NULL);
                return;
            }
            step = S_CALL;
            break;
        case S_CALL:
            step = S_REGION;
            break;
        case S_REGION:
            if (r < 0) return;
            if (r + 1 != sel_region) sel_country[0] = 0;    /* otra región: el país se elige de nuevo */
            sel_region = r + 1;
            region_apply(sel_region);
            if (mode != SETUP_ALL) {
                finish();
                return;
            }
            step = S_COUNTRY;
            break;
        case S_COUNTRY: {
            const country_t *c = r >= 0 ? country_sorted(sel_region, r) : NULL;
            if (!c) return;
            snprintf(sel_country, sizeof(sel_country), "%s", c->cc);
            country_apply(c->cc);
            finish();
            return;
        }
        default:
            return;
    }
    show_step();
}

static void go_back(void) {
    if (mode != SETUP_ALL) {
        dialog_destruct();
        return;
    }
    if (step == S_LANG) return;     /* la primera vez no se sale sin terminar */
    step--;
    show_step();
}

static void next_cb(button_data_t *b) { (void)b; go_next(); }
static void back_cb(button_data_t *b) { (void)b; go_back(); }
static void cancel_cb(button_data_t *b) { (void)b; dialog_destruct(); }

/* indicativo con el teclado de la pantalla */
static bool kbd_close(void) {
    kbd_open = false;
    textarea_window_close();
    lv_group_add_obj(keyboard_group, table);
    lv_group_focus_obj(table);
    show_step();
    return true;
}

static bool kbd_ok(void) {
    const char *val = textarea_window_get();

    if (!val[0]) {
        params_str_set(&params.callsign, "");
    } else if (!station_callsign_save(val)) {
        return false;
    }
    kbd_close();
    step = S_REGION;
    show_step();
    return true;
}

static void edit_cb(button_data_t *b) {
    (void)b;
    if (step != S_CALL || kbd_open) return;
    kbd_open = true;
    lv_group_remove_all_objs(keyboard_group);
    textarea_window_open(kbd_ok, kbd_close);
    station_callsign_edit(textarea_window_text());
}

static void nocall_cb(button_data_t *b) {
    (void)b;
    params_str_set(&params.callsign, "");
    step = S_REGION;
    show_step();
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            go_back();
            break;
        case LV_KEY_ENTER:
            if (step == S_CALL) edit_cb(NULL);
            else go_next();
            break;
        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            break;
        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            break;
        default:
            show_info();    /* la perilla movió la fila */
            break;
    }
}

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_recolor(l, true);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    title = mk_label(dialog.obj, &sony_26, TC(text));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 770);
    lv_obj_set_pos(title, 14, 6);

    hint = mk_label(dialog.obj, &sony_18, TC(text_dim));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, 770);
    lv_obj_set_pos(hint, 14, 40);

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_set_pos(table, 10, 92);
    lv_obj_set_size(table, 360, DIALOG_H - 100);
    lv_table_set_col_cnt(table, 1);
    lv_table_set_col_width(table, 0, 350);
    lv_obj_set_style_border_width(table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, TC(text), LV_PART_ITEMS);
    lv_obj_set_style_text_font(table, &sony_22, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 6, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, TC(focus_text), LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_color(table, TC(focus), LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(table, 128, LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_add_event_cb(table, key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);

    info_box = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(info_box);
    lv_obj_set_pos(info_box, 390, 92);
    lv_obj_set_size(info_box, 400, DIALOG_H - 100);
    lv_obj_set_scroll_dir(info_box, LV_DIR_VER);
    info = mk_label(info_box, &sony_18, TC(text));
    lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info, 390);

    call_lbl = mk_label(dialog.obj, &sony_36, TC(text));
    lv_obj_set_style_text_align(call_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(call_lbl, 780);
    lv_obj_set_pos(call_lbl, 10, 150);

    show_step();
}

static void destruct_cb(void) {
    if (kbd_open) {
        kbd_open = false;
        textarea_window_close();
    }
    title = hint = table = info_box = info = call_lbl = NULL;
}

/* ---------- abrir ---------- */

static void prefill(void) {
    sel_lang   = params.language.x < LANG_COUNT ? params.language.x : LANG_EN;
    sel_region = params.region.x >= 1 && params.region.x <= 3 ? params.region.x : 2;
    snprintf(sel_country, sizeof(sel_country), "%.2s", params.country.x);
}

void dialog_setup_open(setup_mode_t m) {
    mode = m;
    prefill();
    step = m == SETUP_REGION ? S_REGION : m == SETUP_COUNTRY ? S_COUNTRY : S_LANG;
    main_screen_open_dialog(dialog_setup);
}

bool dialog_setup_first_start(void) {
    if (params.setup_done.x) return false;
    dialog_setup_open(SETUP_ALL);
    return true;
}
