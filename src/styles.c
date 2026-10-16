/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "styles.h"
#include "theme.h"
#include "fonts/font_select.h"
#include "i18n.h"

#define PATH "A:/dev/shm/"

const uint32_t wf_palette_legacy[256] = {
    0x000000, 0x000004, 0x000008, 0x00000c, 0x00000e, 0x000012, 0x000016, 0x000018,
    0x00001c, 0x00001e, 0x000022, 0x000024, 0x000026, 0x00002a, 0x00002c, 0x000030,
    0x000032, 0x000034, 0x000038, 0x00003a, 0x00003c, 0x00003e, 0x000041, 0x000044,
    0x000046, 0x000048, 0x00004c, 0x00004e, 0x000050, 0x000051, 0x000054, 0x000058,
    0x000059, 0x00005c, 0x00005e, 0x000060, 0x000061, 0x000066, 0x000068, 0x000069,
    0x00006c, 0x00006e, 0x000070, 0x000071, 0x000076, 0x000078, 0x000079, 0x00007c,
    0x00007e, 0x00007f, 0x03007d, 0x06007b, 0x090079, 0x0b0077, 0x0e0075, 0x140071,
    0x17006f, 0x19006d, 0x1c006b, 0x1f0069, 0x220067, 0x250065, 0x270063, 0x2a0061,
    0x2d005f, 0x30005d, 0x33005b, 0x350059, 0x380057, 0x3b0055, 0x3e0053, 0x410051,
    0x43004f, 0x46004d, 0x4c0049, 0x4f0047, 0x510045, 0x540043, 0x570041, 0x5a003f,
    0x5d003c, 0x5f003b, 0x620038, 0x650037, 0x680034, 0x6b0033, 0x6d0030, 0x70002f,
    0x73002c, 0x76002b, 0x790028, 0x7b0027, 0x7e0024, 0x810023, 0x840020, 0x87001f,
    0x89001c, 0x8c001b, 0x8f0018, 0x8f0018, 0x920017, 0x950014, 0x970013, 0x9a0010,
    0x9d000f, 0xa0000c, 0xa3000b, 0xa50008, 0xa80007, 0xab0004, 0xae0003, 0xb10000,
    0xb20100, 0xb30500, 0xb40800, 0xb50c00, 0xb61000, 0xb61300, 0xb71700, 0xb81a00,
    0xb91e00, 0xba2200, 0xba2500, 0xba2500, 0xbb2900, 0xbc2c00, 0xbd3000, 0xbe3400,
    0xbe3700, 0xbf3b00, 0xc03e00, 0xc14200, 0xc24600, 0xc24900, 0xc34d00, 0xc45000,
    0xc55400, 0xc65800, 0xc65800, 0xc65b00, 0xc75f00, 0xc86200, 0xc96600, 0xca6a00,
    0xca6d00, 0xcb7100, 0xcc7400, 0xcd7800, 0xce7c00, 0xce7f00, 0xce7f00, 0xcf8300,
    0xd08600, 0xd18a00, 0xd28e00, 0xd29100, 0xd39500, 0xd49800, 0xd59c00, 0xd6a000,
    0xd6a300, 0xd6a300, 0xd7a700, 0xd8aa00, 0xd9ae00, 0xdab200, 0xdab500, 0xdbb900,
    0xdcbc00, 0xddc000, 0xdec400, 0xdec400, 0xdec700, 0xdfcb00, 0xe0ce00, 0xe1d200,
    0xe2d600, 0xe2d900, 0xe3dd00, 0xe3dd00, 0xe4e000, 0xe5e400, 0xe5e503, 0xe6e607,
    0xe6e60b, 0xe6e60f, 0xe7e712, 0xe7e717, 0xe7e717, 0xe8e81b, 0xe8e81f, 0xe8e823,
    0xe9e927, 0xe9e92b, 0xeaea2f, 0xeaea33, 0xeaea33, 0xeaea37, 0xebeb3b, 0xebeb3f,
    0xecec43, 0xecec47, 0xecec4b, 0xeded4f, 0xeded4f, 0xeded52, 0xeeee57, 0xeeee5b,
    0xeeee5f, 0xefef63, 0xefef67, 0xefef67, 0xf0f06b, 0xf0f06f, 0xf0f073, 0xf1f177,
    0xf1f17b, 0xf2f27f, 0xf2f27f, 0xf2f283, 0xf2f287, 0xf3f38b, 0xf3f38f, 0xf4f492,
    0xf4f497, 0xf4f497, 0xf4f49b, 0xf5f59f, 0xf5f5a3, 0xf6f6a7, 0xf6f6ab, 0xf6f6ab,
    0xf6f6af, 0xf7f7b3, 0xf7f7b7, 0xf8f8bb, 0xf8f8bf, 0xf8f8c3, 0xf8f8c3, 0xf9f9c7,
    0xf9f9cb, 0xfafacf, 0xfafad2, 0xfafad7, 0xfafad7, 0xfbfbdb, 0xfbfbdf, 0xfcfce3,
    0xfcfce7, 0xfcfceb, 0xfcfceb, 0xfdfdef, 0xfdfdf3, 0xfefef7, 0xfefefb, 0xffffff,
};

const uint32_t wf_palette_gauss[256] = {
    0x000021, 0x000024, 0x000028, 0x00012b, 0x00012d, 0x000130, 0x000134, 0x000136,
    0x00023a, 0x00023c, 0x000240, 0x000242, 0x000344, 0x000348, 0x00034a, 0x00044f,
    0x000451, 0x000553, 0x000557, 0x000659, 0x00065b, 0x00075e, 0x000862, 0x000864,
    0x000966, 0x000968, 0x000a6c, 0x000b6e, 0x000c6f, 0x000d71, 0x000d73, 0x000f76,
    0x001078, 0x001179, 0x00127b, 0x00137c, 0x00147d, 0x001680, 0x001881, 0x001982,
    0x001a83, 0x001c84, 0x001d85, 0x001f85, 0x002286, 0x002387, 0x002587, 0x002787,
    0x002987, 0x002a87, 0x012c87, 0x012e87, 0x013087, 0x013287, 0x013486, 0x013985,
    0x013b84, 0x013d84, 0x024083, 0x024282, 0x024481, 0x024780, 0x02497f, 0x034c7e,
    0x034e7c, 0x03517b, 0x035479, 0x045678, 0x045976, 0x045c75, 0x055e73, 0x056171,
    0x056470, 0x06676e, 0x076c6a, 0x076f68, 0x087266, 0x087464, 0x097762, 0x097a60,
    0x0a7d5e, 0x0b7f5c, 0x0b825a, 0x0c8458, 0x0d8756, 0x0e8a54, 0x0f8c52, 0x108e50,
    0x11914e, 0x12934c, 0x13954a, 0x149748, 0x159a47, 0x169c45, 0x179e43, 0x199f41,
    0x1aa13f, 0x1ba33d, 0x1da53c, 0x1da53c, 0x1ea63a, 0x20a838, 0x22a937, 0x23aa35,
    0x25ab33, 0x27ac32, 0x29ad30, 0x2bae2f, 0x2daf2e, 0x2faf2c, 0x31b02b, 0x33b02a,
    0x35b129, 0x38b128, 0x3ab127, 0x3db126, 0x3fb125, 0x42b124, 0x44b023, 0x47b022,
    0x4aaf21, 0x4daf21, 0x4fae20, 0x4fae20, 0x52ad1f, 0x55ad1f, 0x58ac1e, 0x5bab1e,
    0x5fa91d, 0x62a81d, 0x65a71d, 0x68a61d, 0x6ca41c, 0x6fa31c, 0x72a11c, 0x76a01c,
    0x799e1c, 0x7d9c1c, 0x7d9c1c, 0x809b1c, 0x84991c, 0x87971d, 0x8a951d, 0x8e941d,
    0x91921d, 0x95901e, 0x988e1e, 0x9c8c1e, 0x9f8a1f, 0xa3891f, 0xa3891f, 0xa68720,
    0xa98521, 0xad8321, 0xb08122, 0xb38022, 0xb77e23, 0xba7c24, 0xbd7b25, 0xc07926,
    0xc37726, 0xc37726, 0xc67627, 0xc87428, 0xcb7329, 0xce722a, 0xd0702b, 0xd36f2c,
    0xd56e2e, 0xd86d2f, 0xda6c30, 0xda6c30, 0xdc6b31, 0xde6a32, 0xe06934, 0xe26935,
    0xe46836, 0xe56738, 0xe76739, 0xe76739, 0xe9663b, 0xea663c, 0xeb663e, 0xec663f,
    0xed6641, 0xee6643, 0xef6644, 0xf06646, 0xf06646, 0xf16648, 0xf1674a, 0xf2674b,
    0xf2684d, 0xf3684f, 0xf36951, 0xf36a53, 0xf36a53, 0xf36b55, 0xf36c57, 0xf36d5a,
    0xf36e5c, 0xf36f5e, 0xf37060, 0xf37262, 0xf37262, 0xf27365, 0xf27567, 0xf2766a,
    0xf1786c, 0xf17a6f, 0xf07c71, 0xf07c71, 0xf07e74, 0xef8077, 0xef8279, 0xee847c,
    0xee877f, 0xed8982, 0xed8982, 0xed8c85, 0xed8e88, 0xec918b, 0xec938e, 0xec9691,
    0xeb9994, 0xeb9994, 0xeb9c98, 0xeb9f9b, 0xeba29e, 0xeba5a2, 0xeba9a5, 0xeba9a5,
    0xebaca9, 0xebafad, 0xebb3b0, 0xebb6b4, 0xebbab8, 0xecbebc, 0xecbebc, 0xecc1c0,
    0xedc5c4, 0xeec9c8, 0xefcdcc, 0xefd1d0, 0xefd1d0, 0xf0d5d5, 0xf2dad9, 0xf3dedd,
    0xf4e2e2, 0xf6e7e6, 0xf6e7e6, 0xf7ebeb, 0xf9f0f0, 0xfbf5f5, 0xfcfafa, 0xffffff,
};


const uint32_t *wf_palette;

lv_style_t  background_style;
lv_style_t  spectrum_style;
lv_style_t  freq_style;
lv_style_t  waterfall_style;

lv_style_t  btn_style;
lv_style_t  btn_active_style;
lv_style_t  btn_disabled_style;
lv_style_t  btn_mark_style;
lv_style_t  btn_mark_assigned_style;
lv_style_t  btn_mark_active_style;

lv_style_t  msg_style;
lv_style_t  msg_tiny_style;
lv_style_t  clock_style;
lv_style_t  knobs_style;
lv_style_t  info_style;
lv_style_t  info_row_style;
lv_style_t  info_item_style;
lv_style_t  meter_style;
lv_style_t  tx_info_style;

lv_style_t  panel_top_style;
lv_style_t  panel_mid_style;
lv_style_t  panel_bottom_style;
lv_style_t  panel_style;
lv_style_t  panel_info_style;

lv_style_t  dialog_style;
lv_style_t  freq_strip_style;
lv_style_t  dialog_item_style;
lv_style_t  dialog_item_focus_style;
lv_style_t  dialog_item_edited_style;
lv_style_t  dialog_dropdown_list_style;
lv_style_t  dialog_dropdown_sel_style;

lv_style_t  cw_tune_style;
lv_style_t  popup_style;

lv_color_t  bg_color;


/* A drawn box: the colour, its gradient (when the theme has them), border */
static void box(lv_style_t *s, theme_color_t top, theme_color_t bottom, theme_color_t border, int radius) {
    lv_style_set_bg_opa(s, LV_OPA_COVER);
    lv_style_set_bg_color(s, lv_color_hex(theme_hex(top)));
    lv_style_set_bg_grad_color(s, lv_color_hex(theme_hex(bottom)));
    lv_style_set_bg_grad_dir(s, theme_shape.box_gradient && theme_hex(top) != theme_hex(bottom) ? LV_GRAD_DIR_VER
                                                                                               : LV_GRAD_DIR_NONE);
    lv_style_set_border_width(s, theme_shape.border);
    lv_style_set_border_color(s, lv_color_hex(theme_hex(border)));
    lv_style_set_radius(s, radius);
}

int top_bar_h = TOP_BAR_STD_H, freq_strip_h = FREQ_STRIP_STD_H, clock_w = 168, meter_row_h = 0;

/* lv_style_reset(), not init: this runs again on a theme change and the old properties go */
void styles_init(void) {
    /* * */
    lv_style_t *style;

    top_bar_h    = TOP_BAR_STD_H;
    freq_strip_h = FREQ_STRIP_STD_H;
    clock_w      = 168;
    meter_row_h  = 0;
    /* VISION: 48 + 70 = the 66 + 50 of the others, so the apps start at the same y; the buttons
     * as tall as with DejaVu (the apps already fit them) */
    if (theme_shape.vision) {
        top_bar_h    = 48;
        freq_strip_h = 70;
        clock_w      = 132;
        meter_row_h  = 50;
        buttons_h    = BUTTONS_H_MAX;
    }
    /* Deck (Black Metal, Total Black): two 28 px rows, then the scales beside the frequency */
    if (theme_shape.deck) {
        top_bar_h    = 56;
        freq_strip_h = 102;
        clock_w      = 280;
    }

    lv_style_reset(&background_style);

    lv_style_reset(&spectrum_style);
    lv_style_set_bg_color(&spectrum_style, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&spectrum_style, LV_OPA_COVER);
    lv_style_set_border_color(&spectrum_style, lv_color_hex(0xAAAAAA));
    lv_style_set_border_width(&spectrum_style, 0);
    lv_style_set_radius(&spectrum_style, 0);
    lv_style_set_width(&spectrum_style, 800);
    lv_style_set_x(&spectrum_style, 0);

    lv_style_reset(&freq_style);
    lv_style_set_text_color(&freq_style, lv_color_white());
    lv_style_set_text_font(&freq_style, &sony_22);   /* VISION: 28 (fonts_scale) */
    lv_style_set_pad_ver(&freq_style, theme_shape.vision ? 5 : 9);
    lv_style_set_width(&freq_style, 150);
    lv_style_set_height(&freq_style, 36);
    lv_style_set_text_align(&freq_style, LV_TEXT_ALIGN_CENTER);

    lv_style_reset(&waterfall_style);
    lv_style_set_bg_color(&waterfall_style, lv_color_hex(0x000000));
    lv_style_set_border_color(&waterfall_style, lv_color_hex(0xAAAAAA));
    lv_style_set_border_width(&waterfall_style, 0);
    lv_style_set_radius(&waterfall_style, 0);
    lv_style_set_clip_corner(&waterfall_style, true);
    lv_style_set_width(&waterfall_style, 800);
    lv_style_set_x(&waterfall_style, 0);
    lv_style_set_pad_all(&waterfall_style, 0);

    /* Buttons */
    lv_style_reset(&btn_style);
    lv_style_set_text_font(&btn_style, &sony_30);
    lv_style_set_text_color(&btn_style, lv_color_white());
    /* Modes style: night blue, light border; active = gold border */
    lv_style_set_bg_opa(&btn_style, LV_OPA_COVER);
    lv_style_set_radius(&btn_style, 10);

    lv_style_reset(&btn_active_style);

    lv_style_reset(&btn_disabled_style);

    style = &btn_mark_style;
    lv_style_reset(style);
    lv_style_set_width(style, 4),
    lv_style_set_height(style, BUTTONS_H - 2 - 20),
    lv_style_set_radius(style, 2);
    lv_style_set_bg_opa(style, LV_OPA_COVER);
    lv_style_set_outline_width(style, 0);
    lv_style_set_border_width(style, 0);

    style = &btn_mark_assigned_style;
    lv_style_reset(style);
    lv_style_reset(&btn_mark_active_style);

    /* Message style */
    lv_style_reset(&msg_style);
    lv_style_set_pad_hor(&msg_style, 10);
    lv_style_set_text_font(&msg_style, &sony_38);
    lv_style_set_width(&msg_style, 603);
    // lv_style_set_height(&msg_style, 66);
    lv_style_set_x(&msg_style, 800 / 2 - (603 / 2));
    lv_style_set_y(&msg_style, 270);
    lv_style_set_radius(&msg_style, 0);
    lv_style_set_bg_img_opa(&msg_style, LV_OPA_COVER);
    lv_style_set_pad_ver(&msg_style, 20);

    lv_style_reset(&msg_tiny_style);
    lv_style_set_text_font(&msg_tiny_style, &sony_60);
    lv_style_set_width(&msg_tiny_style, 324);
    lv_style_set_height(&msg_tiny_style, 66);
    lv_style_set_x(&msg_tiny_style, 800 / 2 - (324 / 2));
    lv_style_set_y(&msg_tiny_style, 160 - 66/2 + 36/2);
    lv_style_set_radius(&msg_tiny_style, 0);
    lv_style_set_pad_ver(&msg_tiny_style, 12);

    /* Panel */
    lv_style_reset(&panel_style);
    lv_style_set_text_color(&panel_style, lv_color_white());
    lv_style_set_text_font(&panel_style, &sony_24);
    lv_style_set_width(&panel_style, 795);
    lv_style_set_height(&panel_style, 480 - BUTTONS_H - TOP_BAR_MARGIN - 230);
    lv_style_set_x(&panel_style, 800 / 2 - (795 / 2));
    lv_style_set_y(&panel_style, 230);
    lv_style_set_pad_ver(&panel_style, 10);
    lv_style_set_pad_hor(&panel_style, 10);
    lv_style_set_radius(&panel_style, 0);
    lv_style_set_bg_img_opa(&panel_style, LV_OPA_COVER);

    lv_style_reset(&panel_info_style);
    lv_style_set_align(&panel_info_style, LV_ALIGN_OUT_TOP_LEFT);
    lv_style_set_y(&panel_info_style, -38);
    lv_style_set_text_font(&panel_info_style, &sony_30);

    lv_style_reset(&dialog_style);
    lv_style_set_text_color(&dialog_style, lv_color_white());
    lv_style_set_text_font(&dialog_style, &sony_36);
    lv_style_set_width(&dialog_style, 796);
    lv_style_set_height(&dialog_style, DIALOG_H);
    lv_style_set_x(&dialog_style, 800 / 2 - (796 / 2));
    lv_style_set_y(&dialog_style, DIALOG_Y);
    lv_style_set_radius(&dialog_style, 7);

    lv_style_reset(&freq_strip_style);
    lv_style_set_x(&freq_strip_style, TOP_BAR_MARGIN);
    lv_style_set_y(&freq_strip_style, FREQ_STRIP_Y);
    lv_style_set_width(&freq_strip_style, 800 - 2 * TOP_BAR_MARGIN);
    lv_style_set_height(&freq_strip_style, FREQ_STRIP_H);
    lv_style_set_radius(&freq_strip_style, 7);
    lv_style_set_pad_ver(&dialog_style, 0);
    lv_style_set_pad_hor(&dialog_style, 0);

    lv_style_reset(&dialog_item_style);
    lv_style_set_bg_opa(&dialog_item_style, LV_OPA_TRANSP);
    lv_style_set_text_color(&dialog_item_style, lv_color_white());

    lv_style_reset(&dialog_item_focus_style);
    lv_style_set_text_color(&dialog_item_focus_style, lv_color_black());
    lv_style_set_border_color(&dialog_item_focus_style, lv_color_white());

    lv_style_reset(&dialog_item_edited_style);
    lv_style_set_bg_opa(&dialog_item_edited_style, LV_OPA_COVER);
    lv_style_set_text_color(&dialog_item_edited_style, lv_color_black());

    lv_style_reset(&dialog_dropdown_list_style);
    lv_style_set_text_font(&dialog_dropdown_list_style, &sony_30);
    lv_style_reset(&dialog_dropdown_sel_style);

    /* Clock */
    lv_style_reset(&clock_style);
    lv_style_set_text_color(&clock_style, lv_color_white());
    lv_style_set_align(&clock_style, LV_ALIGN_TOP_RIGHT);
    lv_style_set_x(&clock_style, -TOP_BAR_MARGIN);
    lv_style_set_y(&clock_style, TOP_BAR_MARGIN);
    lv_style_set_width(&clock_style, CLOCK_W);
    lv_style_set_height(&clock_style, TOP_BAR_BOX_H);
    lv_style_set_pad_all(&clock_style, 0);

    /* Knobs */
    lv_style_reset(&knobs_style);
    lv_style_set_text_color(&knobs_style, lv_color_white());
    lv_style_set_text_font(&knobs_style, &sony_24);
    lv_style_set_radius(&knobs_style, 8);
    lv_style_set_bg_color(&knobs_style, lv_color_black());
    lv_style_set_pad_hor(&knobs_style, 5);
    lv_style_set_pad_ver(&knobs_style, 3);

    /* Left info */
    lv_style_reset(&info_style);
    lv_style_set_align(&info_style, LV_ALIGN_TOP_LEFT);
    lv_style_set_x(&info_style, TOP_BAR_MARGIN);
    lv_style_set_y(&info_style, TOP_BAR_MARGIN);
    lv_style_set_width(&info_style, theme_shape.vision ? 800 - CLOCK_W - 3 * TOP_BAR_MARGIN - 3 : 360);
    lv_style_set_height(&info_style, TOP_BAR_BOX_H);
    lv_style_set_pad_hor(&info_style, 5);
    lv_style_set_pad_ver(&info_style, 0);
    lv_style_set_pad_row(&info_style, 0);

    lv_style_reset(&info_row_style);
    lv_style_set_align(&info_row_style, LV_ALIGN_CENTER);
    lv_style_set_radius(&info_row_style, 0);
    lv_style_set_border_width(&info_row_style, 0);
    lv_style_set_bg_opa(&info_row_style, LV_OPA_0);
    lv_style_set_pad_all(&info_row_style, 0);
    lv_style_set_pad_column(&info_row_style, 1);

    lv_style_reset(&info_item_style);
    lv_style_set_text_font(&info_item_style, theme_shape.vision ? &sony_28 : &sony_20);
    lv_style_set_pad_ver(&info_item_style, 1);
    lv_style_set_radius(&info_item_style, 3);

    /* Meter */
    lv_style_reset(&meter_style);
    lv_style_set_align(&meter_style, LV_ALIGN_TOP_LEFT);
    if (theme_shape.vision) {   /* the whole row under the frequency strip */
        lv_style_set_x(&meter_style, TOP_BAR_MARGIN);
        lv_style_set_y(&meter_style, FREQ_STRIP_Y + FREQ_STRIP_H + TOP_BAR_MARGIN);
        lv_style_set_width(&meter_style, 800 - 2 * TOP_BAR_MARGIN);
        lv_style_set_height(&meter_style, METER_ROW_H - 2 * TOP_BAR_MARGIN);
    } else {
        lv_style_set_x(&meter_style, TOP_BAR_METER_X);
        lv_style_set_y(&meter_style, TOP_BAR_MARGIN);
        lv_style_set_width(&meter_style, TOP_BAR_METER_W);
        lv_style_set_height(&meter_style, TOP_BAR_BOX_H);
    }
    lv_style_set_pad_all(&meter_style, 0);

    /* TX info: over the meter, twice as high (PWR and SWR rows) */
    lv_style_reset(&tx_info_style);
    lv_style_set_align(&tx_info_style, LV_ALIGN_TOP_LEFT);
    if (theme_shape.vision) {
        lv_style_set_x(&tx_info_style, TOP_BAR_MARGIN);
        lv_style_set_y(&tx_info_style, FREQ_STRIP_Y + FREQ_STRIP_H + TOP_BAR_MARGIN);
        lv_style_set_width(&tx_info_style, 800 - 2 * TOP_BAR_MARGIN);
        lv_style_set_height(&tx_info_style, METER_ROW_H - 2 * TOP_BAR_MARGIN);
    } else {
        lv_style_set_x(&tx_info_style, TOP_BAR_METER_X);
        lv_style_set_y(&tx_info_style, TOP_BAR_MARGIN);
        lv_style_set_width(&tx_info_style, TOP_BAR_METER_W);
        lv_style_set_height(&tx_info_style, TOP_BAR_BOX_H);
    }
    lv_style_set_pad_all(&tx_info_style, 0);

    lv_style_reset(&popup_style);

    lv_style_reset(&cw_tune_style);
    lv_style_set_radius(&cw_tune_style, 5);
    lv_style_set_bg_color(&cw_tune_style, lv_color_black());
    lv_style_set_x(&cw_tune_style, 30);
    lv_style_set_y(&cw_tune_style, FREQ_STRIP_Y + FREQ_STRIP_H + 18);

    /* Deck: the rows and the panel full width; the clock on the rows, the meter (and TX info) on the panel */
    if (theme_shape.deck) {
        lv_style_set_x(&info_style, 0);
        lv_style_set_y(&info_style, 0);
        lv_style_set_width(&info_style, 800);
        lv_style_set_height(&info_style, TOP_BAR_HEIGHT);
        lv_style_set_pad_hor(&info_style, 0);
        lv_style_set_x(&clock_style, -10);
        lv_style_set_y(&clock_style, 0);
        lv_style_set_height(&clock_style, TOP_BAR_HEIGHT - 1);
        lv_style_t *m[] = { &meter_style, &tx_info_style };
        for (int i = 0; i < 2; i++) {
            lv_style_set_x(m[i], 0);
            lv_style_set_y(m[i], FREQ_STRIP_Y);
            lv_style_set_width(m[i], 300);
            lv_style_set_height(m[i], FREQ_STRIP_H - 1);
        }
        lv_style_set_x(&freq_strip_style, 0);
        lv_style_set_width(&freq_strip_style, 800);
    }

    styles_apply();
}

/* The theme by name, its font, the styles: at start and on a theme change (main_screen_set_theme()) */
void styles_load(const char *name) {
    i18n_set_lang(params.language.x);   /* el idioma, antes de armar la interfaz */
    theme_init(name);    /* an unknown one falls back to IU1WXY */
    fonts_apply(theme_shape.font >= 0 ? theme_shape.font : params.font.x);
    if (theme_shape.vision)
        fonts_scale(VISION_FONT_STEPS);   /* everything bigger: apps, Settings, popups */
    styles_init();
}

/* The theme's image of a surface in place of its colours (the image draws
 * its own border); without one the colours, the style back as it was */
static void surface_img(lv_style_t *s, theme_image_t img, lv_coord_t w, lv_coord_t h) {
    const lv_img_dsc_t *d = theme_image(img, w, h);

    if (d) {
        lv_style_set_bg_img_src(s, d);
        lv_style_set_bg_opa(s, LV_OPA_TRANSP);
        lv_style_set_border_width(s, 0);
    } else {
        lv_style_remove_prop(s, LV_STYLE_BG_IMG_SRC);
    }
}

/* The shared styles from the theme's roles and shapes */
void styles_apply(void) {
    lv_style_t *bar[]   = { &clock_style, &info_style, &meter_style, &tx_info_style };
    lv_style_t *boxes[] = { &msg_style, &msg_tiny_style, &panel_style };
    bool        strong  = theme_shape.border >= 3;     /* high contrast boxes */

    wf_palette = theme_waterfall();

    bg_color = TC(bg);
    lv_style_set_bg_color(&background_style, bg_color);

    for (int i = 0; i < 4; i++) {
        box(bar[i], TC_bar, TC_bar_grad, TC_bar_border, theme_shape.radius);
    }
    box(&dialog_style, TC_dialog, TC_dialog_grad, TC_dialog_border, theme_shape.radius);
    box(&freq_strip_style, TC_strip, TC_strip_grad, TC_strip_border, theme_shape.radius);
    /* Deck: a line under the rows and the panel; the clock and the meter drawn on them */
    lv_style_set_border_side(&info_style, theme_shape.deck ? LV_BORDER_SIDE_BOTTOM : LV_BORDER_SIDE_FULL);
    lv_style_set_border_side(&freq_strip_style, theme_shape.deck ? LV_BORDER_SIDE_BOTTOM : LV_BORDER_SIDE_FULL);
    if (theme_shape.deck) {
        lv_style_set_bg_opa(&clock_style, LV_OPA_TRANSP);
        lv_style_set_border_width(&clock_style, 0);
        lv_style_set_bg_opa(&meter_style, LV_OPA_TRANSP);
        lv_style_set_border_width(&meter_style, 0);
    }

    /* Messages: drawn as the theme's popups in every theme; the panel: background images, or the same */
    for (int i = 0; i < 3; i++) {
        if (boxes[i] == &panel_style && theme_shape.images) {
            lv_style_remove_prop(boxes[i], LV_STYLE_BG_OPA);
            lv_style_remove_prop(boxes[i], LV_STYLE_BG_COLOR);
            lv_style_remove_prop(boxes[i], LV_STYLE_BORDER_WIDTH);
            lv_style_remove_prop(boxes[i], LV_STYLE_BORDER_COLOR);
            lv_style_set_bg_img_src(boxes[i], theme_shape.images == 1 ? PATH "images/panel_dark.bin" : PATH "images/panel.bin");
            continue;
        }
        lv_style_remove_prop(boxes[i], LV_STYLE_BG_IMG_SRC);
        lv_style_set_bg_opa(boxes[i], LV_OPA_COVER);
        lv_style_set_bg_color(boxes[i], TC(popup));
        lv_style_set_border_width(boxes[i], strong ? 3 : theme_shape.border);
        lv_style_set_border_color(boxes[i], TC(popup_border));
        if (boxes[i] != &panel_style) lv_style_set_radius(boxes[i], theme_shape.radius_popup);
    }

    /* Buttons */
    lv_style_set_bg_color(&btn_style, TC(btn));
    lv_style_set_bg_grad_color(&btn_style, TC(btn_grad));
    lv_style_set_bg_grad_dir(&btn_style, theme_shape.btn_gradient ? LV_GRAD_DIR_VER : LV_GRAD_DIR_NONE);
    lv_style_set_border_width(&btn_style, theme_shape.border);
    lv_style_set_border_color(&btn_style, TC(btn_border));
    lv_style_set_text_color(&btn_style, TC(btn_text));
    lv_style_set_radius(&btn_style, theme_shape.radius_btn);

    lv_style_set_bg_color(&btn_active_style, TC(btn_active));
    lv_style_set_bg_grad_color(&btn_active_style, TC(btn_active_grad));
    lv_style_set_bg_grad_dir(&btn_active_style, theme_shape.btn_gradient ? LV_GRAD_DIR_VER : LV_GRAD_DIR_NONE);
    lv_style_set_border_color(&btn_active_style, TC(btn_active_border));
    bool top_line = theme_shape.rule || theme_shape.deck;
    lv_style_set_border_width(&btn_active_style, top_line ? 3 : theme_shape.border < 2 ? 2 : theme_shape.border);
    /* Holo, deck: the active button is marked by a line on top (deck: the others by a thin one) */
    lv_style_set_border_side(&btn_active_style, top_line ? LV_BORDER_SIDE_TOP : LV_BORDER_SIDE_FULL);
    lv_style_set_border_side(&btn_style, theme_shape.deck ? LV_BORDER_SIDE_TOP : LV_BORDER_SIDE_FULL);
    lv_style_set_text_color(&btn_active_style, TC(btn_active_text));

    /* Disabled: same opaque button, dimmed text and border (not see-through) */
    lv_style_set_opa(&btn_disabled_style, LV_OPA_COVER);
    lv_style_set_text_color(&btn_disabled_style, TC(btn_off_text));
    lv_style_set_border_color(&btn_disabled_style, TC(btn_off_border));
    lv_style_set_bg_color(&btn_mark_style, TC(btn_mark));
    lv_style_set_opa(&btn_mark_assigned_style, theme_shape.focus_solid ? LV_OPA_COVER : LV_OPA_40);
    lv_style_set_bg_color(&btn_mark_assigned_style, TC(btn_mark_assigned));
    lv_style_set_opa(&btn_mark_active_style, LV_OPA_COVER);
    lv_style_set_bg_color(&btn_mark_active_style, TC(btn_mark_active));

    lv_style_set_text_color(&panel_info_style, TC(text_mute));
    lv_style_set_blend_mode(&panel_info_style, theme_shape.images ? LV_BLEND_MODE_ADDITIVE : LV_BLEND_MODE_NORMAL);

    lv_style_set_bg_color(&spectrum_style, TC(panel));
    lv_style_set_bg_color(&waterfall_style, TC(panel));

    /* Text on the theme's surfaces; the background images are dark, their text stays white */
    lv_color_t text = TC(text);
    lv_style_set_text_color(&freq_style, text);
    lv_style_set_text_color(&dialog_style, text);
    lv_style_set_text_color(&dialog_item_style, text);
    lv_style_set_text_color(&clock_style, text);
    lv_style_set_text_color(&knobs_style, text);
    lv_style_set_text_color(&msg_style, text);
    lv_style_set_text_color(&msg_tiny_style, text);
    lv_style_set_text_color(&panel_style, theme_shape.images ? lv_color_white() : text);

    /* Settings controls: focused and being edited */
    lv_style_set_text_color(&dialog_item_focus_style, TC(focus_text));
    lv_style_set_text_color(&dialog_item_edited_style, TC(focus_text));
    if (theme_shape.focus_solid) {
        lv_style_set_bg_opa(&dialog_item_focus_style, LV_OPA_COVER);
        lv_style_set_bg_color(&dialog_item_focus_style, TC(focus));
        lv_style_set_border_width(&dialog_item_focus_style, strong ? 3 : 2);
        lv_style_set_border_color(&dialog_item_focus_style, TC(sel_border));
        lv_style_set_bg_color(&dialog_item_edited_style, TC(edited));
    } else {
        lv_style_set_bg_opa(&dialog_item_focus_style, 128);
        lv_style_remove_prop(&dialog_item_focus_style, LV_STYLE_BG_COLOR);
        lv_style_set_border_width(&dialog_item_focus_style, 2);
        lv_style_set_border_color(&dialog_item_focus_style, TC(focus));
        lv_style_remove_prop(&dialog_item_edited_style, LV_STYLE_BG_COLOR);
    }

    /* Knob info and CW tune: see-through, or framed in high contrast */
    lv_style_set_bg_color(&knobs_style, TC(panel));
    lv_style_set_bg_opa(&knobs_style, strong ? LV_OPA_COVER : LV_OPA_60);
    lv_style_set_border_width(&knobs_style, strong ? 2 : 0);
    lv_style_set_border_color(&knobs_style, TC(panel_border));
    lv_style_set_bg_color(&cw_tune_style, TC(panel));
    /* bg_opa, not opa: an object opa makes LVGL draw it in a layer at each frame of its fade
     * (cw_tune_ui.c halves its blocks to match) */
    lv_style_set_bg_opa(&cw_tune_style, strong ? LV_OPA_COVER : LV_OPA_50);
    lv_style_set_border_width(&cw_tune_style, strong ? 2 : 0);
    lv_style_set_border_color(&cw_tune_style, TC(panel_border));


    /* Popups: the theme's popup colours, its border and corners (as the message boxes) */
    lv_style_set_bg_color(&popup_style, TC(popup));
    lv_style_set_bg_opa(&popup_style, LV_OPA_COVER);
    lv_style_set_border_color(&popup_style, TC(popup_border));
    lv_style_set_border_width(&popup_style, strong ? 3 : theme_shape.border);
    lv_style_set_radius(&popup_style, theme_shape.radius_popup);

    /* Dropdown lists: a popup, the selected option as the selected rows of the popups */
    lv_style_set_bg_color(&dialog_dropdown_list_style, TC(popup));
    lv_style_set_bg_opa(&dialog_dropdown_list_style, LV_OPA_COVER);
    lv_style_set_border_color(&dialog_dropdown_list_style, TC(popup_border));
    lv_style_set_border_width(&dialog_dropdown_list_style, strong ? 3 : theme_shape.border);
    lv_style_set_radius(&dialog_dropdown_list_style, theme_shape.radius_popup);
    lv_style_set_text_color(&dialog_dropdown_list_style, text);
    lv_style_set_bg_color(&dialog_dropdown_sel_style, TC(sel_bg));
    lv_style_set_bg_opa(&dialog_dropdown_sel_style, LV_OPA_COVER);
    lv_style_set_text_color(&dialog_dropdown_sel_style, TC(sel_text));

    /* Images of the theme, at the size of each surface (buttons.h BTN_WIDTH 160, 1 px apart) */
    lv_style_set_bg_opa(&btn_style, LV_OPA_COVER);
    lv_style_remove_prop(&btn_active_style, LV_STYLE_BG_OPA);
    lv_style_remove_prop(&btn_disabled_style, LV_STYLE_BG_OPA);
    lv_style_remove_prop(&btn_disabled_style, LV_STYLE_BORDER_WIDTH);
    surface_img(&background_style, TI_bg, 800, 480);
    lv_style_set_bg_opa(&background_style, LV_OPA_COVER);  /* the colour stays under the image */
    int inset = 1 + theme_shape.btn_gap;
    surface_img(&btn_style, TI_btn, 160 - 2 * inset, BUTTONS_H - 2 * inset);
    surface_img(&btn_active_style, TI_btn_active, 160 - 2 * inset, BUTTONS_H - 2 * inset);
    surface_img(&btn_disabled_style, TI_btn_off, 160 - 2 * inset, BUTTONS_H - 2 * inset);
    surface_img(&info_style, TI_bar, 360, TOP_BAR_BOX_H);
    surface_img(&meter_style, TI_bar, TOP_BAR_METER_W, TOP_BAR_BOX_H);
    surface_img(&tx_info_style, TI_bar, TOP_BAR_METER_W, TOP_BAR_BOX_H);
    surface_img(&clock_style, TI_bar, CLOCK_W, TOP_BAR_BOX_H);
    surface_img(&freq_strip_style, TI_strip, 800 - 2 * TOP_BAR_MARGIN, FREQ_STRIP_H);
    surface_img(&dialog_style, TI_dialog, 796, DIALOG_H);

    /* Info box items: framed or not */
    lv_style_set_border_width(&info_item_style, theme_shape.pill_border);
    lv_style_set_border_color(&info_item_style, TC(pill_border));

    /* Encoder marks: the theme's LED, or the bar on the left edge */
    if (theme_shape.mark[1] > 0) {
        lv_style_set_width(&btn_mark_style, theme_shape.mark[1]);
        lv_style_set_height(&btn_mark_style, theme_shape.mark[2]);
        surface_img(&btn_mark_style, TI_mark, theme_shape.mark[1], theme_shape.mark[2]);
        surface_img(&btn_mark_active_style, TI_mark_active, theme_shape.mark[1], theme_shape.mark[2]);
    } else {
        lv_style_remove_prop(&btn_mark_active_style, LV_STYLE_BG_IMG_SRC);
        lv_style_remove_prop(&btn_mark_active_style, LV_STYLE_BG_OPA);
        /* deck: a bar along the top, cyan on the control the encoder is on */
        lv_style_set_width(&btn_mark_style, theme_shape.deck ? 160 - 2 * inset : 4);
        lv_style_set_height(&btn_mark_style, theme_shape.deck ? 3 : BUTTONS_H - 2 - 20);
        lv_style_set_bg_opa(&btn_mark_style, LV_OPA_COVER);
        lv_style_remove_prop(&btn_mark_style, LV_STYLE_BG_IMG_SRC);
    }

    /* LVGL caches gradients by size, not colors: drop the old ones */
    lv_gradient_set_cache_size(LV_GRAD_CACHE_DEF_SIZE);
    lv_obj_invalidate(lv_scr_act());
}

/* A dropdown's list as a popup of the theme */
void styles_dropdown_list(lv_obj_t *list) {
    lv_obj_add_style(list, &dialog_dropdown_list_style, 0);
    lv_obj_add_style(list, &dialog_dropdown_sel_style, LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_add_style(list, &dialog_dropdown_sel_style, LV_PART_SELECTED | LV_STATE_PRESSED);
}

/* For screens laid out elsewhere (walkie.c): a box of the theme, its image at w x h */
void styles_theme_box(lv_style_t *s, theme_color_t top, theme_color_t bottom, theme_color_t border, int radius,
                      theme_image_t img, lv_coord_t w, lv_coord_t h) {
    lv_style_reset(s);   /* built again after a theme change (walkie_drop()) */
    box(s, top, bottom, border, radius);
    surface_img(s, img, w, h);
}
