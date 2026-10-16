/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - FT8 message table view
 *
 *  Copyright (c) 2026
 */

#include "table_view.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../events.h"
#include "../styles.h"
#include "../theme.h"
#include "../flags.h"
#include "../cfg/cfg_api.h"

/*
 *  Cell data is owned by a fixed-size ring pool. We do NOT use LVGL's
 *  cell user_data API: lv_table_set_cell_user_data() internally calls
 *  lv_mem_free() on the previous user_data pointer (treating it as a
 *  malloc'd buffer), which would crash on our pool indices. Instead we
 *  keep a parallel s_row_cell[] mapping table row -> cell_data_t* in
 *  this module, and never touch LVGL's user_data slot at all.
 *
 *  Size invariant:
 *      CELL_POOL_SIZE  >= MAX_TABLE_MSG + CLEAN_N_ROWS
 *  Worst case, the table holds MAX_TABLE_MSG rows; one pending push
 *  touches the next slot before truncate_oldest() runs. Any slot that
 *  the ring write cursor is about to overwrite is guaranteed to have
 *  been truncated out of the table by that point.
 */

#define MAX_TABLE_MSG               512
#define CLEAN_N_ROWS                 64
#define CELL_POOL_SIZE              576  /* MAX_TABLE_MSG + CLEAN_N_ROWS */
#define TABLE_MAX_ROWS_INTERNAL     576  /* upper bound on row index used */
#define WAIT_SYNC_TEXT              "Wait sync"

static cell_data_t            s_pool[CELL_POOL_SIZE];
static uint16_t               s_pool_write = 0;
static cell_data_t           *s_row_cell[TABLE_MAX_ROWS_INTERNAL]; /* row -> pool entry, NULL if none */

static lv_obj_t              *s_table      = NULL;
static table_view_press_cb_t  s_press_cb   = NULL;
static table_view_actions_t   s_actions    = {0};

/*
 *  "For you": the last messages to this station, one row a caller, pinned in a small table
 *  under the list whatever the Show filter, so they don't scroll away. A caller leaves when we
 *  send it 73 / RR73 or after PIN_TTL_MS without news. The MFK goes down into it from the last
 *  row of the list (s_pin_sel >= 0), a press answers that caller as a press on the list does.
 */
#define PIN_MAX     3
#define PIN_TTL_MS  120000

static lv_obj_t    *s_pin;
static cell_data_t  s_pin_cd[PIN_MAX];
static uint32_t     s_pin_at[PIN_MAX];
static int          s_pin_n;
static int          s_pin_sel = -1;     /* the MFK row in the pin table, -1: in the list */
static uint16_t     s_list_row;         /* the list's row while the MFK is in the pin table */
static uint16_t     s_sel_prev;         /* the list's selected row after the last key */
static bool         s_own_key;          /* a key sent by push_ui (autoscroll), not the MFK */
static lv_coord_t   s_y, s_h;
static lv_timer_t  *s_pin_timer;

static inline cell_data_t *row_cell(uint16_t r) {
    if (r >= TABLE_MAX_ROWS_INTERNAL) return NULL;
    return s_row_cell[r];
}

/* The cell of row r of the list or of the pin table */
static cell_data_t *cell_of(lv_obj_t *obj, uint16_t r) {
    if (obj == s_pin) return r < s_pin_n ? &s_pin_cd[r] : NULL;
    return row_cell(r);
}

/* The pin table shown under the list with its rows, the list shorter by its height */
static void pin_layout(void) {
    if (!s_pin || !s_table) return;
    if (!s_pin_n) {
        lv_obj_add_flag(s_pin, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(s_table, s_h);
        s_pin_sel = -1;
        return;
    }
    lv_table_set_row_cnt(s_pin, s_pin_n);
    for (int i = 0; i < s_pin_n; i++)
        lv_table_set_cell_value(s_pin, i, 0, s_pin_cd[i].text);
    lv_obj_clear_flag(s_pin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(s_pin);
    lv_coord_t ph = lv_obj_get_height(s_pin);
    lv_obj_set_y(s_pin, s_y + s_h - ph);
    lv_obj_set_height(s_table, s_h - ph - 4);
    if (s_pin_sel >= s_pin_n) s_pin_sel = s_pin_n - 1;
    lv_obj_invalidate(s_pin);
}

static void pin_drop(int i) {
    memmove(&s_pin_cd[i], &s_pin_cd[i + 1], (size_t)(s_pin_n - i - 1) * sizeof(s_pin_cd[0]));
    memmove(&s_pin_at[i], &s_pin_at[i + 1], (size_t)(s_pin_n - i - 1) * sizeof(s_pin_at[0]));
    s_pin_n--;
}

/* A message to us: its caller's row, newest at the bottom */
static void pin_add(const cell_data_t *cd) {
    if (!cd->meta.call_de[0]) return;
    for (int i = 0; i < s_pin_n; i++)
        if (!strcmp(s_pin_cd[i].meta.call_de, cd->meta.call_de)) {
            pin_drop(i);
            break;
        }
    if (s_pin_n == PIN_MAX) pin_drop(0);
    s_pin_cd[s_pin_n] = *cd;
    s_pin_at[s_pin_n] = lv_tick_get();
    s_pin_n++;
    pin_layout();
}

/* We sent 73 or RR73: the QSO with the callers named in it is over */
static void pin_tx(const char *text) {
    size_t n = strlen(text);
    bool   end = (n >= 3 && !strcmp(text + n - 3, " 73")) || (n >= 4 && !strcmp(text + n - 4, "RR73"));
    bool   changed = false;

    if (!end) return;
    for (int i = s_pin_n - 1; i >= 0; i--) {
        char word[16];
        snprintf(word, sizeof(word), " %s ", s_pin_cd[i].meta.call_de);
        if (strstr(text, word)) {
            pin_drop(i);
            changed = true;
        }
    }
    if (changed) pin_layout();
}

static void pin_expire_cb(lv_timer_t *t) {
    (void)t;
    bool changed = false;
    for (int i = s_pin_n - 1; i >= 0; i--)
        if (lv_tick_elaps(s_pin_at[i]) > PIN_TTL_MS) {
            pin_drop(i);
            changed = true;
        }
    if (changed) pin_layout();
}

static inline void set_row_cell(uint16_t r, cell_data_t *cd) {
    if (r >= TABLE_MAX_ROWS_INTERNAL) return;
    s_row_cell[r] = cd;
}

static cell_data_t *pool_alloc_and_copy(const cell_data_t *src) {
    cell_data_t *c = &s_pool[s_pool_write];
    s_pool_write = (uint16_t)((s_pool_write + 1u) % CELL_POOL_SIZE);
    *c = *src;
    c->text[sizeof(c->text) - 1] = '\0';
    return c;
}

static void truncate_oldest(void) {
    if (!s_table) return;

    uint16_t rows = lv_table_get_row_cnt(s_table);
    if (rows <= MAX_TABLE_MSG) return;

    lv_table_t *tbl = (lv_table_t *)s_table;
    lv_coord_t  removed_height = 0;
    for (size_t i = 0; i < CLEAN_N_ROWS; i++) {
        removed_height += tbl->row_h[i];
    }
    if (tbl->row_act > CLEAN_N_ROWS) {
        tbl->row_act -= CLEAN_N_ROWS;
    } else {
        tbl->row_act = 0;
    }

    /* Shift rows up: LVGL copies cell text, our parallel s_row_cell[]
     * gets memmove'd in lockstep. No allocations, no frees, and we never
     * touch LVGL's user_data slot. */
    for (uint16_t i = CLEAN_N_ROWS; i < rows; i++) {
        const char *val = lv_table_get_cell_value(s_table, i, 0);
        lv_table_set_cell_value(s_table, (uint16_t)(i - CLEAN_N_ROWS), 0, val ? val : "");
    }
    if (rows > CLEAN_N_ROWS) {
        memmove(&s_row_cell[0], &s_row_cell[CLEAN_N_ROWS],
                (size_t)(rows - CLEAN_N_ROWS) * sizeof(s_row_cell[0]));
    }
    /* Zero out the tail entries that were just shifted away. */
    for (uint16_t i = (uint16_t)(rows - CLEAN_N_ROWS); i < rows; i++) {
        set_row_cell(i, NULL);
    }

    rows = (uint16_t)(rows - CLEAN_N_ROWS);
    lv_table_set_row_cnt(s_table, rows);
    lv_obj_scroll_by_bounded(s_table, 0, removed_height, LV_ANIM_OFF);
}

/* Return true if this cell_data_t matches an RX-header row. */
static bool is_rx_header(const cell_data_t *cd) {
    return cd && (cd->cell_type == CELL_RX_INFO);
}

void table_view_push_ui(const cell_data_t *src) {
    if (!s_table || !src) return;

    truncate_oldest();

    uint16_t rows = lv_table_get_row_cnt(s_table);
    if ((rows == 1) && (strcmp(lv_table_get_cell_value(s_table, 0, 0), tr(WAIT_SYNC_TEXT)) == 0)) {
        /* The single "Wait sync" placeholder is about to be replaced. */
        rows = 0;
    }

    /* Row autoscroll: keep scrolled-to-bottom unless the user moved away. */
    uint16_t selected_row = 0;
    uint16_t selected_col = 0;
    lv_table_get_selected_cell(s_table, &selected_row, &selected_col);
    bool scroll = (rows == (uint16_t)(selected_row + 1));

    /* In-place RX-header collapse: replace the last row text instead of
     * appending another identical "RX ..." marker. */
    if (is_rx_header(src) && (rows > 0)) {
        uint16_t last = (uint16_t)(rows - 1);
        cell_data_t *last_cd = row_cell(last);
        if (last_cd && is_rx_header(last_cd)) {
            strncpy(last_cd->text, src->text, sizeof(last_cd->text) - 1);
            last_cd->text[sizeof(last_cd->text) - 1] = '\0';
            lv_table_set_cell_value(s_table, last, 0, last_cd->text);
            return;
        }
    }

    /* A decode or a TX takes the place of the "RX ..." row under it: that row only shows the
     * receiving goes on while nothing comes (each row has its own time now) */
    if (!is_rx_header(src) && rows > 0 && is_rx_header(row_cell((uint16_t)(rows - 1)))) {
        rows--;
    }

    cell_data_t *stored = pool_alloc_and_copy(src);
    lv_table_set_cell_value(s_table, rows, 0, stored->text);
    set_row_cell(rows, stored);

    if (src->cell_type == CELL_RX_TO_ME)
        pin_add(src);
    else if (src->cell_type == CELL_TX_MSG)
        pin_tx(src->text);

    if (scroll) {
        static int32_t c = LV_KEY_DOWN;
        s_own_key = true;
        lv_event_send(s_table, LV_EVENT_KEY, &c);
        s_own_key = false;
    }
}

void table_view_add_msg_cb(void *data) {
    table_view_push_ui((const cell_data_t *)data);
}

void table_view_reset(void) {
    if (!s_table) return;

    /* Clear our parallel row->cell map; LVGL's user_data slot is never
     * touched (it stays NULL), so there is no stale-pointer free path
     * during set_row_cnt() shrink. */
    memset(s_row_cell, 0, sizeof(s_row_cell));
    lv_table_set_row_cnt(s_table, 1);
    lv_table_set_cell_value(s_table, 0, 0, WAIT_SYNC_TEXT);
    s_pool_write = 0;
    s_pin_n = 0;    /* another band or mode: the callers of the old one are gone */
    pin_layout();
}

/* -------- LVGL event plumbing ------------------------------------------ */

static void draw_part_begin_cb(lv_event_t *e) {
    lv_obj_t               *obj = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);

    if (dsc->part != LV_PART_ITEMS) return;

    uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
    uint32_t col = dsc->id - row * lv_table_get_col_cnt(obj);
    (void)col;
    cell_data_t *cd = cell_of(obj, (uint16_t)row);

    dsc->rect_dsc->bg_opa = obj == s_pin ? LV_OPA_80 : 115;

    if (!cd) {
        dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;
        dsc->rect_dsc->bg_color = TC(row);
    } else {
        switch (cd->cell_type) {
        case CELL_START_QSO:
        case CELL_RX_INFO:
            dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;
            dsc->rect_dsc->bg_color = TC(row);
            break;
        case CELL_RX_CQ:
            switch (cd->worked_type) {
            case SEARCH_WORKED_NO:
                dsc->rect_dsc->bg_color = TC(ft8_cq);
                break;
            case SEARCH_WORKED_YES:
                dsc->label_dsc->opa = LV_OPA_90;
                dsc->rect_dsc->bg_color = TC(ft8_cq_worked);
                break;
            case SEARCH_WORKED_SAME_MODE:
                dsc->label_dsc->decor = LV_TEXT_DECOR_STRIKETHROUGH;
                dsc->label_dsc->opa   = LV_OPA_80;
                dsc->rect_dsc->bg_color = TC(ft8_cq_same);
                break;
            }
            break;
        case CELL_RX_TO_ME:
            dsc->rect_dsc->bg_color = TC(ft8_to_me);
            break;
        case CELL_TX_MSG:
            dsc->rect_dsc->bg_color = TC(ft8_tx);
            break;
        default:
            dsc->rect_dsc->bg_color = TC(panel);
            break;
        }
    }

    uint16_t selected_row;
    uint16_t selected_col;
    lv_table_get_selected_cell(obj, &selected_row, &selected_col);
    if (obj == s_pin ? s_pin_sel == (int)row : (s_pin_sel < 0 && selected_row == row)) {
        dsc->rect_dsc->bg_color = lv_color_lighten(dsc->rect_dsc->bg_color, 20);
    }
}

static lv_coord_t text_width(const char *text, const lv_draw_label_dsc_t *ld) {
    lv_point_t size;

    lv_txt_get_size(&size, text, ld->font, ld->letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

/* Flag k as an LVGL image, converted on first use */
static const lv_img_dsc_t *flag_img(int k) {
    static lv_img_dsc_t img[sizeof(flag_px) / sizeof(flag_px[0])];
    static lv_color_t  *px[sizeof(flag_px) / sizeof(flag_px[0])];

    if (k < 0 || k >= (int)(sizeof(flag_px) / sizeof(flag_px[0]))) return NULL;
    if (!px[k]) {
        if (!(px[k] = malloc(FLAG_W * FLAG_H * sizeof(lv_color_t)))) return NULL;
        for (int i = 0; i < FLAG_W * FLAG_H; i++)
            px[k][i] = lv_color_make(flag_px[k][3 * i], flag_px[k][3 * i + 1], flag_px[k][3 * i + 2]);
        img[k].header.cf = LV_IMG_CF_TRUE_COLOR;
        img[k].header.w  = FLAG_W;
        img[k].header.h  = FLAG_H;
        img[k].data_size = FLAG_W * FLAG_H * sizeof(lv_color_t);
        img[k].data      = (const uint8_t *)px[k];
    }
    return &img[k];
}

/* The sender's flag and country, right-aligned up to `right`: the name when it fits after the
 * message (`text_end`), else its prefix, else the flag only (from SP2KMO's upstream PR #282) */
static void draw_country(lv_obj_draw_part_dsc_t *dsc, const cell_data_t *cd, lv_area_t *area, lv_coord_t right,
                         lv_coord_t text_end) {
    const lv_img_dsc_t *flag = flag_img(cd->flag);

    if (flag && right - FLAG_W >= text_end + 16) {
        lv_draw_img_dsc_t id;
        lv_area_t         fa;
        lv_draw_img_dsc_init(&id);
        fa.x2 = right;
        fa.x1 = right - FLAG_W + 1;
        fa.y1 = (area->y1 + area->y2 - FLAG_H) / 2;
        fa.y2 = fa.y1 + FLAG_H - 1;
        lv_draw_img(dsc->draw_ctx, &id, &fa, flag);
        right -= FLAG_W + 6;
    }
    const char *choice[2] = {cd->country, cd->cty_prefix};
    for (int i = 0; i < 2; i++) {
        if (!choice[i][0]) continue;
        lv_coord_t w = text_width(choice[i], dsc->label_dsc);
        if (right - w < text_end + 16) continue;
        area->x1 = right - w;
        area->x2 = right;
        lv_draw_label(dsc->draw_ctx, dsc->label_dsc, area, choice[i], NULL);
        return;
    }
}

static void draw_part_end_cb(lv_event_t *e) {
    lv_obj_t               *obj = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);

    if (dsc->part != LV_PART_ITEMS) return;

    uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
    uint32_t col = dsc->id - row * lv_table_get_col_cnt(obj);
    (void)col;
    cell_data_t *cd = cell_of(obj, (uint16_t)row);
    if (!cd) return;

    if ((cd->cell_type != CELL_RX_MSG) &&
        (cd->cell_type != CELL_RX_CQ) &&
        (cd->cell_type != CELL_RX_TO_ME)) {
        return;
    }

    char              buf[64];
    const lv_coord_t  cell_top    = lv_obj_get_style_pad_top(obj, LV_PART_ITEMS);
    const lv_coord_t  cell_bottom = lv_obj_get_style_pad_bottom(obj, LV_PART_ITEMS);
    lv_area_t         area;

    dsc->label_dsc->align = LV_TEXT_ALIGN_RIGHT;

    area.y1 = dsc->draw_area->y1 + cell_top;
    area.y2 = dsc->draw_area->y2 - cell_bottom;
    area.x2 = dsc->draw_area->x2 - 15;
    area.x1 = area.x2 - 120;

    snprintf(buf, sizeof(buf), "%i dB", cd->meta.local_snr);
    lv_draw_label(dsc->draw_ctx, dsc->label_dsc, &area, buf, NULL);

    lv_coord_t right    = area.x1 - 10;
    lv_coord_t text_end = dsc->draw_area->x1 + lv_obj_get_style_pad_left(obj, LV_PART_ITEMS) +
                          text_width(cd->text, dsc->label_dsc);
    if (cd->pota) { /* a tag after the message, in the row's text colour */
        lv_area_t      tag = area;
        lv_draw_rect_dsc_t rd;
        lv_draw_rect_dsc_init(&rd);
        rd.bg_opa       = LV_OPA_TRANSP;
        rd.border_width = 1;
        rd.border_color = dsc->label_dsc->color;
        rd.radius       = 3;
        tag.x1 = text_end + 10;
        tag.x2 = tag.x1 + text_width("POTA", dsc->label_dsc) + 7;
        lv_draw_rect(dsc->draw_ctx, &rd, &tag);
        tag.x1 += 4;
        tag.x2 -= 3;
        lv_draw_label(dsc->draw_ctx, dsc->label_dsc, &tag, "POTA", NULL);
        text_end = tag.x2 + 3;
    }
    /* Settings > FT8/FT4 info: 0 km, 1 mi, 2 country, 3 country + km, 4 country + mi (from SP2KMO's upstream 3074180e) */
    int32_t info = param_i_get(cfg_ft8_info);

    if (info != 2 && cd->dist > 0) {
        area.x2 = area.x1 - 10;
        area.x1 = area.x2 - 200;
        if (info == 1 || info == 4)
            snprintf(buf, sizeof(buf), "%i mi", (int)(cd->dist * 0.621371f + 0.5f));
        else
            snprintf(buf, sizeof(buf), "%i km", cd->dist);
        lv_draw_label(dsc->draw_ctx, dsc->label_dsc, &area, buf, NULL);
        right = area.x2 - text_width(buf, dsc->label_dsc) - 16;
    }
    if (info >= 2)
        draw_country(dsc, cd, &area, right, text_end);
}

static void cell_press_cb(lv_event_t *e) {
    if (!s_press_cb) return;

    uint16_t row, col;
    if (lv_event_get_target(e) == s_pin) {    /* touched */
        lv_table_get_selected_cell(s_pin, &row, &col);
        if (row < s_pin_n) s_press_cb(&s_pin_cd[row]);
        return;
    }
    if (s_pin_sel >= 0 && s_pin_sel < s_pin_n) {    /* the MFK on the pin table */
        s_press_cb(&s_pin_cd[s_pin_sel]);
        return;
    }
    lv_table_get_selected_cell(s_table, &row, &col);
    (void)col;
    cell_data_t *cd = row_cell(row);
    s_press_cb(cd);
}

/* The MFK past the last row of the list goes on into the pin table, up from its first row
 * back into the list. The list moved its own selection already (class first): put it back */
static void pin_key(uint32_t key) {
    lv_table_t *tbl = (lv_table_t *)s_table;
    uint16_t    rows = lv_table_get_row_cnt(s_table);
    uint16_t    row, col;

    lv_table_get_selected_cell(s_table, &row, &col);
    if (s_own_key) {
        s_sel_prev = row;
        return;
    }
    if (key == LV_KEY_DOWN || key == LV_KEY_RIGHT) {
        if (s_pin_sel >= 0) {
            tbl->row_act = s_list_row;
            if (s_pin_sel < s_pin_n - 1) s_pin_sel++;
        } else if (s_pin_n && rows && s_sel_prev == rows - 1 && row == rows - 1) {
            s_list_row = row;
            s_pin_sel  = 0;
        }
    } else if (key == LV_KEY_UP || key == LV_KEY_LEFT) {
        if (s_pin_sel >= 0) {
            tbl->row_act = s_list_row;
            s_pin_sel--;
        }
    } else {
        return;
    }
    lv_table_get_selected_cell(s_table, &s_sel_prev, &col);
    lv_obj_invalidate(s_table);
    if (s_pin) lv_obj_invalidate(s_pin);
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    if (lv_event_get_target(e) == s_table) pin_key(key);
    switch (key) {
    case LV_KEY_ESC:
        if (s_actions.on_close) s_actions.on_close();
        break;
    case KEY_VOL_LEFT_EDIT:
    case KEY_VOL_LEFT_SELECT:
        if (s_actions.on_vol_change) s_actions.on_vol_change(-1);
        break;
    case KEY_VOL_RIGHT_EDIT:
    case KEY_VOL_RIGHT_SELECT:
        if (s_actions.on_vol_change) s_actions.on_vol_change(1);
        break;
    default:
        break;
    }
}

void table_view_build(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    s_pool_write = 0;
    s_press_cb   = NULL;
    memset(&s_actions, 0, sizeof(s_actions));
    memset(s_row_cell, 0, sizeof(s_row_cell));

    s_table = lv_table_create(parent);
    lv_obj_remove_style(s_table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_add_event_cb(s_table, cell_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_table, key_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(s_table, draw_part_begin_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(s_table, draw_part_end_cb,   LV_EVENT_DRAW_PART_END,   NULL);

    lv_obj_set_size(s_table, w, h);
    lv_obj_set_pos(s_table, x, y);

    lv_table_set_col_cnt(s_table, 1);
    lv_table_set_col_width(s_table, 0, w - 2);

    lv_obj_set_style_border_width(s_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(s_table, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_table, TC(panel), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_table, theme_shape.border, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_table, TC(text), LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_table, 128, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_table, TC(text_soft), LV_PART_ITEMS);
    /* Its own size: the dialog's 36 (taller still with DejaVu) left room for five rows */
    /* Decoder text Compact (Settings > Display, after SP2KMO): more rows */
    bool compact = param_i_get(cfg_digi_text_compact);
    lv_obj_set_style_text_font(s_table, compact ? &sony_20 : &sony_26, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(s_table, compact ? 2 : 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(s_table, compact ? 2 : 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(s_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(s_table, 0, LV_PART_ITEMS);

    lv_table_set_cell_value(s_table, 0, 0, WAIT_SYNC_TEXT);

    /* "For you" under it: the same rows in the Large font, a line of the to-me colour on top */
    s_y = y;
    s_h = h;
    s_pin_n   = 0;
    s_pin_sel = -1;
    s_pin = lv_table_create(parent);
    lv_obj_remove_style(s_pin, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_add_event_cb(s_pin, cell_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_pin, draw_part_begin_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(s_pin, draw_part_end_cb, LV_EVENT_DRAW_PART_END, NULL);
    lv_obj_set_pos(s_pin, x, y + h);
    lv_obj_set_width(s_pin, w);
    lv_obj_set_height(s_pin, LV_SIZE_CONTENT);
    lv_table_set_col_cnt(s_pin, 1);
    lv_table_set_col_width(s_pin, 0, w - 2);
    lv_obj_set_style_border_width(s_pin, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(s_pin, 224, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_pin, TC(panel), LV_PART_MAIN);
    lv_obj_set_style_border_side(s_pin, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_pin, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_pin, TC(ft8_to_me), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_pin, TC(text), LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_pin, &sony_26, LV_PART_ITEMS);   /* always the Large rows: the calls must stand out */
    lv_obj_set_style_pad_top(s_pin, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(s_pin, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(s_pin, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(s_pin, 0, LV_PART_ITEMS);
    lv_obj_clear_flag(s_pin, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pin, LV_OBJ_FLAG_HIDDEN);
    s_pin_timer = lv_timer_create(pin_expire_cb, 5000, NULL);
}

void table_view_destroy(void) {
    if (s_pin_timer) {
        lv_timer_del(s_pin_timer);
        s_pin_timer = NULL;
    }
    if (s_pin) {
        lv_obj_del(s_pin);
        s_pin = NULL;
    }
    s_pin_n   = 0;
    s_pin_sel = -1;
    if (s_table) {
        /* LVGL will delete its internal cell strings; we never wrote to
         * cell user_data, so its NULL slots have nothing to free. */
        lv_obj_del(s_table);
        s_table = NULL;
    }
    s_press_cb = NULL;
    memset(&s_actions, 0, sizeof(s_actions));
    memset(s_row_cell, 0, sizeof(s_row_cell));
    s_pool_write = 0;
}

lv_obj_t *table_view_obj(void) {
    return s_table;
}

void table_view_set_press_cb(table_view_press_cb_t cb) {
    s_press_cb = cb;
}

void table_view_set_actions(const table_view_actions_t *actions) {
    if (actions) {
        s_actions = *actions;
    } else {
        memset(&s_actions, 0, sizeof(s_actions));
    }
}

/* Flag k of flags.h as an LVGL image (the NavTex app's DSC list) */
const lv_img_dsc_t *table_view_flag(int k) {
    return flag_img(k);
}
