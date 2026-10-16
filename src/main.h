/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include "rotary.h"
#include "encoder.h"

#define VERSION "x6100 IU1WXY firmware - v0.126"   /* la base; la app X6100 Link lee este número */

/* Versión de esta edición (CMake: VERSION_CX8ANM) */
#ifndef CX8ANM_VERSION
#define CX8ANM_VERSION "dev"
#endif
#define CX8ANM_NAME "Xiegu X6100-CX8ANM"


extern rotary_t     *vol;
extern encoder_t    *mfk;

/* LVGL and the panel portrait (480x800, the walkie screen) or landscape; false without DRM */
bool main_display_portrait(bool on);
