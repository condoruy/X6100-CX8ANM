/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "lvgl/lvgl.h"
#include "params/params.h"

#define MEM_HKEY_MAX_ID         9

#define MEM_BACKUP_ID   (MEM_HKEY_MAX_ID + 1)
#define MEM_FT8_ID      100
#define MEM_FT4_ID      200
#define MEM_WSPR_ID     300
#define MEM_SSTV_ID     400

lv_obj_t * main_screen();

void main_screen_notify_rx_tx(bool tx);
void main_screen_notify_low_power(bool is_low);

void main_screen_keys_enable(bool value);
void main_screen_start_app(press_action_t page_app);
void main_screen_close_app(void);
void main_screen_vol_press(void);
void main_screen_mfk(int dir);
void main_screen_action(press_action_t action);
void main_screen_agc_next();

void main_screen_lock_freq(bool lock);
void main_screen_lock_band(bool lock);
void main_screen_lock_mode(bool lock);
void main_screen_lock_ab(bool lock);
/* Locks set by the apps, checked by the web remote */
bool main_screen_freq_locked();
bool main_screen_band_locked();
bool main_screen_mode_locked();

void main_screen_set_freq(uint64_t f);
/* Another theme, live (Settings, the remote) */
void main_screen_set_theme(const char *name);

void mem_load(uint16_t id);
void mem_save(uint16_t id);

/* Proprietary temporary status/channel overlay. */
void channel_overlay_show(const char *name);

/* CX8ANM: la frecuencia actual a la lista de canales (V/M), y esa lista desde los botones */
void main_screen_save_channel(void);
void main_screen_open_channels(void);
