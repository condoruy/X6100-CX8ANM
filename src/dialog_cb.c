/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: Banda Ciudadana. Los 40 canales de la banda elegida (A a E, y UK: la del Reino Unido, en FM); la perilla MFK elige
 *  uno y al apretarla (o con Ir) la radio va al canal en el modo predeterminado (AM, FM, USB o
 *  LSB), que después se cambia como siempre. "Perilla" decide si en la CB la perilla principal
 *  salta de canal en canal o sintoniza libre. "Guardar" deja la frecuencia actual en la lista de
 *  canales de memoria (V/M).
 */
#include "dialog_cb.h"

#include "buttons.h"
#include "cb.h"
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

static const x6100_mode_t modes[] = {x6100_mode_am, x6100_mode_nfm, x6100_mode_usb, x6100_mode_lsb};
static const char *const  mode_names[] = {"AM", "FM", "USB", "LSB"};

static lv_obj_t *title, *desc, *table;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);
static void band_cb(button_data_t *b);
static void mode_cb(button_data_t *b);
static void knob_cb(button_data_t *b);
static void go_cb(button_data_t *b);
static void save_cb(button_data_t *b);

static button_data_t btn_band = {.type = BTN_TEXT, .label = "Band", .press = band_cb};
static button_data_t btn_mode = {.type = BTN_TEXT, .label = "Mode", .press = mode_cb};
static button_data_t btn_knob = {.type = BTN_TEXT, .label = "Knob", .press = knob_cb};
static button_data_t btn_go   = {.type = BTN_TEXT, .label = "Go", .press = go_cb};
static button_data_t btn_save = {.type = BTN_TEXT, .label = "Save\nchannel", .press = save_cb};

static buttons_page_t btn_page = {{&btn_band, &btn_mode, &btn_knob, &btn_go, &btn_save}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = NULL,
    .btn_page     = &btn_page,
    .key_cb       = NULL,
};

dialog_t *dialog_cb = &dialog;

static int band(void) {
    return params.cb_band.x < CB_BANDS ? params.cb_band.x : CB_BAND_C;
}

static int mode_ix(void) {
    return params.cb_mode.x < 4 ? params.cb_mode.x : 0;
}

static void show_title(void) {
    char a[16], b[16];

    if (!title) return;
    if (desc) lv_label_set_text(desc, tr(cb_band_desc(band())));
    lv_label_set_text_fmt(title, "#%06x %s %s#  #%06x %s - %s MHz#   %s #%06x %s#   %s #%06x %s#",
                          theme_hex(TC_text_head), tr("CB BAND"), cb_band_name(band()), theme_hex(TC_text_dim),
                          cb_freq_text(cb_freq(band(), 1), a, sizeof(a)), cb_freq_text(cb_freq(band(), 40), b, sizeof(b)),
                          tr("Mode"),
                          theme_hex(TC_accent), mode_names[mode_ix()], tr("Knob"), theme_hex(TC_accent),
                          params.cb_knob.x ? tr("channels") : tr("free"));
}

static void fill_table(void) {
    int     cur_band = -1, cur_ch = -1;
    int32_t f        = cparam_i_get(cfg_fg_freq);

    cb_channel_of(f, &cur_band, &cur_ch);
    lv_table_set_row_cnt(table, CB_CHANNELS);
    for (int c = 1; c <= CB_CHANNELS; c++) {
        int32_t hz = cb_freq(band(), c);
        bool    on = cur_band == band() && cur_ch == c;
        char    f[16];
        lv_table_set_cell_value_fmt(table, c - 1, 0, "%s%s %d", on ? LV_SYMBOL_RIGHT " " : "", tr("Channel"), c);
        lv_table_set_cell_value_fmt(table, c - 1, 1, "%s MHz", cb_freq_text(hz, f, sizeof(f)));
        lv_table_set_cell_value(table, c - 1, 2, cb_channel_note(c));
    }
}

static int selected(void) {
    uint16_t row = 0, col = 0;
    lv_table_get_selected_cell(table, &row, &col);
    return row == LV_TABLE_CELL_NONE || row >= CB_CHANNELS ? -1 : row + 1;
}

static void select_row(int ch) {
    /* la tabla recuerda la celda elegida en su estado de edición */
    lv_table_t *t = (lv_table_t *)table;
    t->row_act    = ch - 1;
    t->col_act    = 0;
    lv_obj_invalidate(table);
    lv_coord_t h = lv_obj_get_height(table);
    lv_obj_scroll_to_y(table, (ch - 1) * 34 - h / 2 + 17 > 0 ? (ch - 1) * 34 - h / 2 + 17 : 0, LV_ANIM_OFF);
}

/* ---------- acciones ---------- */

static void tune(int ch) {
    char    name[24], f[16];
    int32_t hz = cb_freq(band(), ch);

    if (!hz || radio_get_state() != RADIO_RX) return;
    main_screen_set_freq(hz);
    cparam_i_set(cfg_cur_mode, modes[mode_ix()]);
    msg_update_text_fmt("%s  %s MHz  %s", cb_name(hz, name, sizeof(name)) ?: "CB", cb_freq_text(hz, f, sizeof(f)),
                        mode_names[mode_ix()]);
    fill_table();
}

static void band_cb(button_data_t *b) {
    (void)b;
    params_uint8_set(&params.cb_band, (band() + 1) % CB_BANDS);
    if (band() == CB_BAND_UK) params_uint8_set(&params.cb_mode, 1);    /* la UK es en FM */
    show_title();
    fill_table();
    select_row(19 <= CB_CHANNELS ? 19 : 1);
}

static void mode_cb(button_data_t *b) {
    (void)b;
    params_uint8_set(&params.cb_mode, (mode_ix() + 1) % 4);
    show_title();
    /* si ya está en un canal de la CB, cambia el modo enseguida */
    if (cb_channel_of(cparam_i_get(cfg_fg_freq), NULL, NULL) && radio_get_state() == RADIO_RX)
        cparam_i_set(cfg_cur_mode, modes[mode_ix()]);
}

static void knob_cb(button_data_t *b) {
    (void)b;
    params_bool_set(&params.cb_knob, !params.cb_knob.x);
    show_title();
    msg_update_text_fmt("%s: %s", tr("Main knob on CB"), params.cb_knob.x ? tr("channel by channel") : tr("free tuning"));
}

static void go_cb(button_data_t *b) {
    (void)b;
    int ch = selected();
    if (ch > 0) tune(ch);
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

    title = lv_label_create(dialog.obj);
    lv_label_set_recolor(title, true);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, 770);
    lv_obj_set_style_text_font(title, &sony_20, 0);
    lv_obj_set_style_text_color(title, TC(text), 0);
    lv_obj_set_pos(title, 14, 6);

    desc = lv_label_create(dialog.obj);     /* el nombre de la banda: oficial, de exportación, UK */
    lv_obj_set_style_text_font(desc, &sony_18, 0);
    lv_obj_set_style_text_color(desc, TC(accent), 0);
    lv_obj_set_pos(desc, 14, 32);

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_set_pos(table, 10, 56);
    lv_obj_set_size(table, 780, DIALOG_H - 64);
    lv_table_set_col_cnt(table, 3);
    lv_table_set_col_width(table, 0, 230);
    lv_table_set_col_width(table, 1, 230);
    lv_table_set_col_width(table, 2, 310);
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

    /* si la radio ya está en un canal, abre en su banda y con ese canal elegido */
    int b, ch = 19;
    if (cb_channel_of(cparam_i_get(cfg_fg_freq), &b, &ch)) params_uint8_set(&params.cb_band, b);
    show_title();
    fill_table();
    select_row(ch);
}

static void destruct_cb(void) {
    title = desc = table = NULL;
}
