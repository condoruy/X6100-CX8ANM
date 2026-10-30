/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: banda aérea de HF. Las frecuencias de las rutas aéreas oceánicas (MWARA) y de los
 *  VOLMET de la zona elegida; la perilla MFK elige una y al apretarla (o con Ir) la radio va en
 *  USB. "Zona" pasa por Américas, Europa/África, Asia/Pacífico y todas (empieza en la de la región
 *  IARU de la radio). "Guardar" deja la frecuencia en la lista de canales de memoria.
 */
#include "dialog_aero.h"

#include "aero.h"
#include "buttons.h"
#include "cfg/cfg_api.h"
#include "dialog.h"
#include "events.h"
#include "keyboard.h"
#include "main_screen.h"
#include "msg.h"
#include "params/params.h"
#include "radio.h"
#include "styles.h"
#include "theme.h"
#include "i18n.h"

#include <stdio.h>
#include <string.h>

#define MAX_ROWS 400

typedef struct {
    int16_t  fam;
    uint16_t khz;
} row_t;

static row_t     rows[MAX_ROWS];
static int       rows_n;
static int       area = -1;         /* 1, 2, 3 o 0 (todas); -1: la de la región de la radio */
static lv_obj_t *title, *hint, *table;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);
static void area_cb(button_data_t *b);
static void go_cb(button_data_t *b);
static void save_cb(button_data_t *b);

static button_data_t btn_area = {.type = BTN_TEXT, .label = "Area", .press = area_cb};
static button_data_t btn_go   = {.type = BTN_TEXT, .label = "Go", .press = go_cb};
static button_data_t btn_save = {.type = BTN_TEXT, .label = "Save\nchannel", .press = save_cb};

static buttons_page_t btn_page = {{&btn_area, &btn_go, &btn_save}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = NULL,
    .btn_page     = &btn_page,
    .key_cb       = NULL,
};

dialog_t *dialog_aero = &dialog;

static const char *area_name(int a) {
    switch (a) {
        case 1:  return tr("Europe, Africa, Middle East");
        case 2:  return tr("Americas");
        case 3:  return tr("Asia and Pacific");
        default: return tr("All areas");
    }
}

static void show_title(void) {
    lv_label_set_text_fmt(title, "#%06x %s#   #%06x %s#", theme_hex(TC_text_head), tr("HF AIR BAND"), theme_hex(TC_accent),
                          area_name(area));
}

static void fill(void) {
    int32_t f = cparam_i_get(cfg_fg_freq);

    rows_n = 0;
    for (int i = 0; i < aero_families(); i++) {
        const aero_family_t *a = aero_family(i);
        if (area && a->area && a->area != area) continue;
        for (const uint16_t *k = a->khz; *k && rows_n < MAX_ROWS; k++) rows[rows_n++] = (row_t){i, *k};
    }
    lv_table_set_row_cnt(table, rows_n ? rows_n : 1);
    for (int r = 0; r < rows_n; r++) {
        const aero_family_t *a  = aero_family(rows[r].fam);
        bool                 on = f / 1000 == rows[r].khz;
        lv_table_set_cell_value_fmt(table, r, 0, "%s%s", on ? LV_SYMBOL_RIGHT " " : "", a->code);
        lv_table_set_cell_value_fmt(table, r, 1, "%u kHz", rows[r].khz);
        lv_table_set_cell_value(table, r, 2, tr(a->name));
    }
}

static int selected(void) {
    uint16_t row = 0, col = 0;
    lv_table_get_selected_cell(table, &row, &col);
    return row == LV_TABLE_CELL_NONE || row >= rows_n ? -1 : row;
}

static void select_row(int r) {
    lv_table_t *t = (lv_table_t *)table;
    lv_coord_t  h = lv_obj_get_height(table), y = r * 34 - h / 2 + 17;

    t->row_act = r;
    t->col_act = 0;
    lv_obj_invalidate(table);
    lv_obj_scroll_to_y(table, y > 0 ? y : 0, LV_ANIM_OFF);
}

static void tune(int r) {
    int32_t hz = rows[r].khz * 1000;

    if (radio_get_state() != RADIO_RX) return;
    main_screen_set_freq(hz);
    cparam_i_set(cfg_cur_mode, x6100_mode_usb);
    msg_update_text_fmt("%s  %u kHz  USB", aero_family(rows[r].fam)->code, rows[r].khz);
    fill();
}

static void area_cb(button_data_t *b) {
    (void)b;
    area = area == 2 ? 1 : area == 1 ? 3 : area == 3 ? 0 : 2;
    show_title();
    fill();
    select_row(0);
}

static void go_cb(button_data_t *b) {
    (void)b;
    int r = selected();
    if (r >= 0) tune(r);
}

static void save_cb(button_data_t *b) {
    (void)b;
    main_screen_save_channel();
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct();
            break;
        case LV_KEY_ENTER:
            go_cb(NULL);
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
    if (area < 0) area = params.region.x >= 1 && params.region.x <= 3 ? params.region.x : 2;

    title = lv_label_create(dialog.obj);
    lv_label_set_recolor(title, true);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 770);
    lv_obj_set_style_text_font(title, &sony_22, 0);
    lv_obj_set_style_text_color(title, TC(text), 0);
    lv_obj_set_pos(title, 14, 6);

    hint = lv_label_create(dialog.obj);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    lv_obj_set_width(hint, 770);
    lv_obj_set_style_text_font(hint, &sony_18, 0);
    lv_obj_set_style_text_color(hint, TC(text_dim), 0);
    lv_obj_set_pos(hint, 14, 34);
    lv_label_set_text(hint, tr("USB. By day 8-18 MHz, at night 3-6 MHz. VHF air band (118-137 MHz): Internet radio > List"));

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_set_pos(table, 10, 60);
    lv_obj_set_size(table, 780, DIALOG_H - 68);
    lv_table_set_col_cnt(table, 3);
    lv_table_set_col_width(table, 0, 160);
    lv_table_set_col_width(table, 1, 160);
    lv_table_set_col_width(table, 2, 450);
    lv_obj_set_style_border_width(table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, TC(text), LV_PART_ITEMS);
    lv_obj_set_style_text_font(table, &sony_22, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, TC(focus_text), LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_color(table, TC(focus), LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(table, 128, LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_add_event_cb(table, key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);

    buttons_unload_page();
    buttons_load_page(&btn_page);

    show_title();
    fill();
    /* si la radio ya está en una de la lista, abre ahí */
    int32_t f = cparam_i_get(cfg_fg_freq) / 1000, at = 0;
    for (int r = 0; r < rows_n; r++)
        if (rows[r].khz == f) { at = r; break; }
    select_row(at);
}

static void destruct_cb(void) {
    title = hint = table = NULL;
}
