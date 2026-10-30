/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "lvgl/lvgl.h"
#include <liquid/liquid.h>

#include "helpers.h"
#include "buttons.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void (*dialog_construct_cb_t)(lv_obj_t *);
typedef void (*dialog_destruct_cb_t)(void);
typedef void (*dialog_audio_cb_t)(unsigned int n, float *samples);
typedef void (*dialog_rotary_cb_t)(int32_t diff);

typedef struct {
    lv_obj_t                *obj;
    dialog_construct_cb_t   construct_cb;
    dialog_destruct_cb_t    destruct_cb;
    dialog_audio_cb_t       audio_cb;
    dialog_rotary_cb_t      rotary_cb;
    buttons_page_t          *btn_page;
    buttons_page_t          *prev_page;
    lv_event_cb_t           key_cb;
    bool                    restore_radio; /* save freq/mode on open, restore on close */
    bool                    run;
} dialog_t;


void dialog_construct(dialog_t *dialog, lv_obj_t *parent);
/* CX8ANM: abre el dialog sobre la pantalla principal (main_screen.c) */
void main_screen_open_dialog(dialog_t *d);
void dialog_destruct();

/* The open app changed the mode from mode to forced: back to mode when it closes */
void dialog_restore_mode_on_close(int32_t mode, int32_t forced);
/* The app just opened (one without restore_radio) gives back frequency and mode when it closes */
void dialog_restore_radio_on_close(void);

/* Each digital app opens where it was last used (frequency and mode, /mnt/app_freqs):
 * the next dialog_construct() tunes to name's after saving the radio, its close keeps
 * the app's last ones for the next time. The two calls for an app without a dialog (RTTY) */
void dialog_app_memory(const char *name);
bool app_mem_load(const char *name, int32_t *freq, int32_t *mode);
void app_mem_save(const char *name);

void dialog_send(lv_event_code_t event_code, void *param);
bool dialog_is_run();
bool dialog_type_is_run(dialog_t *dialog);

lv_obj_t * dialog_init(lv_obj_t *parent);
void dialog_item(dialog_t *dialog, lv_obj_t *obj);

bool dialog_need_audio();
void dialog_audio_samples(unsigned int n, float *samples);
void dialog_rotary(int32_t diff);

#ifdef __cplusplus
}
#endif
