/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Incluido automáticamente en todos los fuentes de la interfaz
 *  (CMake: -include i18n_hook.h). Hace pasar por tr() todo texto que llega a
 *  LVGL, así no hace falta tocar cada lv_label_set_text() del código.
 *  Lo que no está en la tabla de traducción se muestra igual que antes.
 */
#pragma once

#include "lvgl/lvgl.h"
#include "i18n.h"

#define lv_label_set_text(obj, txt)                     lv_label_set_text(obj, tr(txt))
#define lv_label_set_text_static(obj, txt)              lv_label_set_text_static(obj, tr_static(txt))
#define lv_label_set_text_fmt(obj, fmt, ...)            lv_label_set_text_fmt(obj, tr(fmt), ##__VA_ARGS__)
#define lv_dropdown_set_options(obj, opts)              lv_dropdown_set_options(obj, tr(opts))
#define lv_dropdown_set_options_static(obj, opts)       lv_dropdown_set_options_static(obj, tr_static(opts))
#define lv_dropdown_add_option(obj, opt, pos)           lv_dropdown_add_option(obj, tr(opt), pos)
#define lv_dropdown_set_text(obj, txt)                  lv_dropdown_set_text(obj, tr_static(txt))
#define lv_textarea_set_placeholder_text(obj, txt)      lv_textarea_set_placeholder_text(obj, tr(txt))
#define lv_list_add_text(list, txt)                     lv_list_add_text(list, tr(txt))
#define lv_list_add_btn(list, icon, txt)                lv_list_add_btn(list, icon, tr(txt))
#define lv_checkbox_set_text(obj, txt)                  lv_checkbox_set_text(obj, tr(txt))
#define lv_draw_label(ctx, dsc, area, txt, hint)        lv_draw_label(ctx, dsc, area, tr(txt), hint)
#define lv_canvas_draw_text(c, x, y, w, dsc, txt)       lv_canvas_draw_text(c, x, y, w, dsc, tr(txt))
#define lv_txt_get_size(size, txt, font, ls, lsp, mw, f) lv_txt_get_size(size, tr(txt), font, ls, lsp, mw, f)
