/*
 * SSTV receiver UI - initial panel skeleton
 */

#include "dialog_sstv.h"
#include "styles.h"
#include "theme.h"
#include "dialog.h"
#include "buttons.h"
#include "main_screen.h"
#include "keyboard.h"
#include "textarea_window.h"
#include "sstv/decoder.h"
#include "sstv/encoder.h"
#include "sstv/linesync.h"
#include "sstv/preview.h"
#include "sstv/vis.h"
#include "sstv/denoise.h"
#include "radio.h"
#include "remote_server.h"
#include "tx_player.h"
#include "events.h"
#include "msg.h"
#include "params/params.h"
#include "app_frame.h"
#include "freq_list.h"
#include "cfg/cfg_api.h"

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <png.h>

#include "audio.h"

/* The image fills the dialog height, 4:3 */
#define SSTV_CANVAS_H (APP_FRAME_BODY_H - 2)   /* the body of the common app look */
#define SSTV_CANVAS_W (SSTV_CANVAS_H * 4 / 3)
#define SSTV_TEST_WAV "/mnt/sstv.wav"
#define SSTV_DEBUG_WAV "/mnt/sstv-debug.wav"
#define SSTV_SAVE_DIR "/mnt/sstv"
#define SSTV_IMAGE_H SSTV_CANVAS_H
#define SSTV_IMAGE_W SSTV_CANVAS_W
#define SSTV_IMAGE_X (APP_FRAME_X + 1)
#define SSTV_IMAGE_Y (APP_FRAME_BODY_Y + 1)
#define SSTV_BROWSER_W 710
#define SSTV_BROWSER_H (APP_FRAME_BODY_H - 4)
#define MAX_PWR TX_PLAYER_MAX_PWR_W

/* Interactive TX picture selector (paths overridable for the PC harness) */
#ifndef SSTV_TX_PICS_DIR
#define SSTV_TX_PICS_DIR "/mnt/sstv/tx_pics"
#define SSTV_TX_TEXT_DIR "/mnt/sstv/tx_text"
#define SSTV_TX_PNG      "/mnt/sstv-tx.png"
#endif
#define SSTV_TX_PICTURE_DEBOUNCE_MS 300
/* TX menu: a popup box in the body, right of the picture */
#define SSTV_TX_LIST_X (SSTV_IMAGE_X + SSTV_IMAGE_W + 10)
#define SSTV_TX_LIST_Y (APP_FRAME_BODY_Y + 6)
#define SSTV_TX_LIST_W (APP_FRAME_X + APP_FRAME_W - 8 - SSTV_TX_LIST_X)
#define SSTV_TX_LIST_H (APP_FRAME_BODY_H - 12)
#define SSTV_TX_HEAD_H 32
#define SSTV_TX_HINT   "MFK: choose - press: confirm - Cancel TX: back"
#define SSTV_STATUS_TEXT "Freq.: calling frequencies   Pictures in /mnt/sstv"
/* Image position is calculated from the mode detected by VIS. */

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void test_cb(button_data_t *data);
static void close_cb(button_data_t *data);
static void restart_cb(button_data_t *data);
static void denoise_cb(button_data_t *data);
static void key_cb(lv_event_t *e);
static void start_cb(button_data_t *data);
static void cont_start_cb(button_data_t *data);
static void cont_mode_cb(button_data_t *data);
static void browser_cb(button_data_t *data);
static void delete_picture_cb(button_data_t *data);
static void page_1_cb(button_data_t *data);
static void page_2_cb(button_data_t *data);
static void page_3_cb(button_data_t *data);
static void audio_cb(unsigned int n, float *samples);
static void tx_ui_cb(button_data_t *data);
static void tx_picture_selector_build(void);
static void tx_picture_selector_destroy(void);
static void tx_picture_selector_refresh(void);
static void tx_picture_selection_changed_cb(lv_event_t *e);
static void tx_picture_pressed_cb(lv_event_t *e);
static void tx_picture_schedule_preview(void);
static void tx_picture_load_pending_preview(void);
static bool tx_picture_load_and_prepare(const char *filename);
static void tx_picture_render_preview(void);
static void tx_save_rx_canvas(void);
static void tx_restore_rx_canvas(void);
static void tx_rebuild_current_overlay(void);
static void tx_type_selector_build(void);
static void tx_type_selector_destroy(void);
static void tx_type_pressed_cb(lv_event_t *e);
static void tx_type_selection_changed_cb(lv_event_t *e);
static void tx_type_update_help(void);
static void tx_type_draw_cb(lv_event_t *e);
static void tx_instruction_show(const char *title, const char *hint);
static void tx_instruction_hide(void);
static void tx_build_final_frame(bool cq_overlay);
static void tx_template_selector_build(void);
static void tx_template_selector_destroy(void);
static void tx_template_selection_changed_cb(lv_event_t *e);
static void tx_template_pressed_cb(lv_event_t *e);
static void tx_template_render(void);
static void tx_template_render_selected(void);
static bool tx_target_keyboard_ok_cb(void);
static bool tx_target_keyboard_cancel_cb(void);
static lv_obj_t *tx_find_keyboard_recursive(lv_obj_t *parent);
static void tx_target_keyboard_force_lower(void);
static void tx_target_keyboard_open_now(void);
static void tx_color_selector_build(void);
static void tx_color_selector_destroy(void);
static void tx_color_selection_changed_cb(lv_event_t *e);
static void tx_color_pressed_cb(lv_event_t *e);
static void tx_color_render_selected(void);
static void ui_timer_cb(lv_timer_t *timer);
static void *test_thread_cb(void *arg);
static void *live_decode_thread_cb(void *arg);
static void *cont_decode_thread_cb(void *arg);
static void *tx_thread_cb(void *arg);
static bool save_debug_wav(const char *path, const float *samples, long count);
static bool save_sstv_png(const uint8_t *rgb, int width, int height, const char *name);
static void save_rx_picture(void);
static bool save_tx_png(const char *path, const uint8_t *rgb, unsigned int width, unsigned int height);
static bool tx_start_final_rf(void);
static bool tx_start_final_rf_mode(sstv_mode_t mode);
static void tx_return_to_auto_rx(void);
static void set_status(const char *text);
static void reset_ui_to_standby(void);
static void render_scaled_image(void);
static button_data_t btn_start;
static button_data_t btn_cont_start;
static button_data_t btn_cont_mode;
static button_data_t btn_browser;
static button_data_t btn_delete_picture;
static button_data_t btn_tx;
static buttons_page_t btn_page_1;

static void set_receiver_ui_stopped(void);
static void set_receiver_ui_listening(void);
static void set_receiver_ui_decoding(const char *mode_name);

static lv_obj_t *sstv_frame = NULL;
static lv_obj_t *sstv_canvas = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *progress_bar = NULL;  /* the bar of the common app look */
static app_frame_t frame;

/* SSTV calling frequencies; LSB below 10 MHz as usual on the air */
static const freq_list_item_t sstv_freqs[] = {
    { .group = "LSB" },
    { .freq_hz = 3735000,  .name = "SSTV 80m", .note = "Europe", .lsb = true },
    { .freq_hz = 3845000,  .name = "SSTV 80m", .note = "Americas", .lsb = true },
    { .freq_hz = 7165000,  .name = "SSTV 40m", .note = "Europe", .lsb = true },
    { .freq_hz = 7171000,  .name = "SSTV 40m", .note = "Americas", .lsb = true },
    { .group = "USB" },
    { .freq_hz = 14230000, .name = "SSTV 20m", .note = "calling" },
    { .freq_hz = 14233000, .name = "SSTV 20m" },
    { .freq_hz = 18160000, .name = "SSTV 17m" },
    { .freq_hz = 21340000, .name = "SSTV 15m" },
    { .freq_hz = 24975000, .name = "SSTV 12m" },
    { .freq_hz = 28680000, .name = "SSTV 10m" },
    { .freq_hz = 50510000, .name = "SSTV 6m" },
};
static volatile bool saved_overlay_pending = false;
static char saved_overlay_name[96];
static lv_color_t sstv_buffer[((DIALOG_H_MAX - 20) * 4 / 3) * (DIALOG_H_MAX - 20)];
static uint8_t test_rgb[SSTV_MAX_W * SSTV_MAX_H * 3];
static uint8_t dn_tmp[SSTV_MAX_W * SSTV_MAX_H * 3];   /* sstv_denoise() scratch */
static sstv_info_t test_info;
static lv_timer_t *ui_timer = NULL;
static pthread_t test_thread;
static bool test_thread_valid = false;
static volatile bool test_running = false;
static volatile bool test_ready = false;
static volatile int test_rows = 0;
static int shown_rows = 0;

/* SSTV test transmitter.  The worker owns all blocking audio/radio work;
 * LVGL is touched only by ui_timer_cb(). */
static pthread_t tx_thread;
static bool tx_thread_valid = false;
static volatile bool tx_running = false;
static volatile bool tx_abort = false;
static volatile bool tx_started = false;
static volatile int tx_progress = 0;
static sstv_mode_t tx_mode = SSTV_MODE_ROBOT8_BW;
static struct timespec tx_key_time;
static volatile bool tx_key_time_valid = false;

/* Nominal on-air SSTV payload durations.  Progress is deliberately based on
 * elapsed wall-clock time, not on how quickly audio_play() accepts samples. */
#define SSTV_TX_PREROLL_MS 500u
static unsigned int sstv_tx_duration_ms(sstv_mode_t mode)
{
    switch (mode) {
        case SSTV_MODE_ROBOT8_BW:   return 8910u;   /* VIS ~910 ms + 120 x 66.667 ms */
        case SSTV_MODE_MARTIN_M1:  return 115200u; /* VIS ~910 ms + 256 x 446.446 ms */
        case SSTV_MODE_MARTIN_M2:  return 58970u;  /* VIS ~910 ms + 256 x 226.798 ms */
        case SSTV_MODE_ROBOT36:    return 36910u;  /* VIS 910 ms + 240 x 150.000 ms */
        case SSTV_MODE_ROBOT72:    return 72910u;  /* VIS 910 ms + 240 x 300.000 ms */
        case SSTV_MODE_SCOTTIE_S1: return 110543u; /* VIS 910 + 9 ms + 256 x 428.220 ms */
        case SSTV_MODE_SCOTTIE_S2: return 72008u;  /* VIS 910 + 9 ms + 256 x 277.692 ms */
        default:                   return 0u;
    }
}

static uint64_t monotonic_ms_since(const struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t sec = (int64_t)now.tv_sec - (int64_t)start->tv_sec;
    int64_t nsec = (int64_t)now.tv_nsec - (int64_t)start->tv_nsec;
    return (uint64_t)(sec * 1000 + nsec / 1000000);
}
static volatile int tx_result = 0; /* 0 idle/running, 1 done, <0 error/abort */

/* New interactive TX UI state.  Step 1 only implements entry/cancel; the
 * picture/template phases are added on top of this state machine. */
typedef enum { SSTV_TX_UI_RX = 0, SSTV_TX_UI_SETUP, SSTV_TX_UI_RF } sstv_tx_ui_state_t;
static sstv_tx_ui_state_t tx_ui_state = SSTV_TX_UI_RX;
static bool tx_restore_auto_rx = false;
static char tx_mode_label[48];

typedef enum {
    SSTV_TX_PHASE_NONE = 0,
    SSTV_TX_PHASE_SELECT_PICTURE,
    SSTV_TX_PHASE_SELECT_TYPE,
    SSTV_TX_PHASE_SELECT_TEMPLATE,
    SSTV_TX_PHASE_SELECT_COLORS
} sstv_tx_phase_t;

static sstv_tx_phase_t tx_phase = SSTV_TX_PHASE_NONE;
static lv_obj_t *tx_picture_table = NULL;
static lv_obj_t *tx_type_table = NULL;
static lv_obj_t *tx_template_table = NULL;
static lv_obj_t *tx_color_table = NULL;
static lv_obj_t *tx_instruction_label = NULL;
/* Runtime-only persistence of the selected initial TX action row. */
static int tx_last_type_selected = 0;
static bool tx_template_preview_pending = false;
static bool tx_color_preview_pending = false;
static uint32_t tx_color_preview_since_ms = 0;
static bool tx_target_keyboard_open_pending = false;
static lv_obj_t *tx_target_keyboard_obj = NULL;
static uint32_t tx_target_keyboard_force_until = 0;
typedef enum {
    SSTV_TX_MESSAGE_CQ = 0,
    SSTV_TX_MESSAGE_REPLY,
    SSTV_TX_MESSAGE_REPLAY,
    SSTV_TX_MESSAGE_IMAGE_ONLY,
    SSTV_TX_MESSAGE_REPEAT_LAST
} sstv_tx_message_type_t;
static sstv_tx_message_type_t tx_message_type = SSTV_TX_MESSAGE_CQ;
static int tx_color_selected = 0;
/* Runtime-only persistence of the selected color-combination row. */
static int tx_last_color_selected = 0;
static char **tx_template_files = NULL;
static size_t tx_template_file_count = 0;
static size_t tx_template_file_capacity = 0;
static char tx_template_middle[256];
static char tx_template_bottom[128];
/* Runtime persistence by design: keep the last target while the app/process
 * remains alive, but do not write it to permanent firmware configuration. */
static char tx_target_callsign[32] = "";
static char **tx_picture_files = NULL;
static size_t tx_picture_file_count = 0;
static size_t tx_picture_file_capacity = 0;
static int tx_picture_selected = -1;
static int tx_picture_pending = -1;
static uint64_t tx_picture_pending_since_ms = 0;
static uint8_t tx_picture_rgb[320 * 256 * 3];
static uint8_t tx_final_rgb[320 * 256 * 3];
static unsigned int tx_picture_width = 0;
static unsigned int tx_picture_height = 0;

/* Runtime-only backup of the last SSTV transmission that completed
 * successfully.  Store the exact transmitted pixels plus the mode so
 * REPEAT LAST TX is independent of the currently selected TX Mode. */
static uint8_t tx_last_rgb[320 * 256 * 3];
static unsigned int tx_last_width = 0;
static unsigned int tx_last_height = 0;
static sstv_mode_t tx_last_mode = SSTV_MODE_SCOTTIE_S1;
static bool tx_last_valid = false;
static char tx_picture_name[256];
static char tx_last_picture_file[256] = "";
static lv_color_t tx_saved_rx_canvas[sizeof(sstv_buffer) / sizeof(sstv_buffer[0])];
static bool tx_saved_rx_canvas_valid = false;

/* Saved-SSTV browser.  It is deliberately independent from the live SSTV
 * canvas/state machine so reception and decoding continue while it is open. */
static lv_obj_t *sstv_browser_frame = NULL;
static lv_obj_t *sstv_browser_table = NULL;
static lv_obj_t *sstv_browser_canvas = NULL;
static lv_color_t *sstv_browser_buffer = NULL;
static char **sstv_browser_files = NULL;
static size_t sstv_browser_file_capacity = 0;
static unsigned int sstv_browser_file_count = 0;
static bool sstv_browser_open = false;
static bool sstv_browser_preview = false;
static uint8_t *sstv_browser_image = NULL;
static unsigned int sstv_browser_image_width = 0;
static unsigned int sstv_browser_image_height = 0;

static void sstv_browser_free_files(void);
static void sstv_browser_refresh(void);
static void sstv_browser_update_list(void);
static void sstv_browser_build(void);
static void sstv_browser_close(void);
static void sstv_browser_return_to_list(void);
static void sstv_browser_delete_selected(void);
static bool sstv_browser_open_png(const char *filename);
static void sstv_browser_free_image(void);
static void sstv_browser_render_image(void);
static void sstv_browser_pressed_cb(lv_event_t *e);
static void sstv_browser_preview_pressed_cb(lv_event_t *e);
static void sstv_browser_update_buttons(void);

/* Live RF capture.  Keep a short rolling prebuffer so the VIS leader is
 * preserved when we decide that an SSTV transmission has started. */
#define SSTV_PREBUF_SAMPLES (SSTV_SAMPLE_RATE * 2)
#define SSTV_VIS_WINDOW_SAMPLES (SSTV_SAMPLE_RATE)
#define SSTV_VIS_CHECK_STEP (SSTV_SAMPLE_RATE / 20) /* 50 ms */

static pthread_mutex_t live_mutex = PTHREAD_MUTEX_INITIALIZER;
static float live_ring[SSTV_PREBUF_SAMPLES];
static float live_vis_window[SSTV_VIS_WINDOW_SAMPLES];
static size_t live_ring_pos = 0;
static size_t live_ring_count = 0;
static unsigned int live_since_check = 0;
static uint64_t live_wait_samples = 0;
static uint64_t live_wait_log_mark = 0;
static volatile bool live_enabled = false;
static volatile bool live_capturing = false;
static volatile bool live_decode_pending = false;
static volatile bool live_decoding = false;
static volatile bool live_canvas_clear_pending = false;
static volatile bool live_abort_requested = false;
static volatile bool live_mode_ui_pending = false;
static volatile bool live_restart_enabled = true;
static float *live_samples = NULL;
static long live_written = 0;
static long live_target = 0;
static sstv_info_t live_info;

/* Manual continuous RX. This state machine is deliberately separate from
 * Auto RX so the validated VIS-driven path remains unchanged. */
#define SSTV_CONT_MAX_SECONDS 300
static volatile bool cont_capturing = false;
static volatile bool cont_decode_pending = false;
static volatile bool cont_decoding = false;
static float *cont_samples = NULL;
static long cont_written = 0;
static long cont_capacity = 0;
static sstv_mode_t cont_mode = SSTV_MODE_ROBOT8_BW;
static sstv_mode_t cont_decode_mode = SSTV_MODE_ROBOT8_BW;

/* Manual RX "Auto" mode: the line-sync tracker (sstv/linesync.c, from
 * 1KO125's x6100_gui 1.0.2) finds the mode from the 1200-Hz syncs, with no
 * VIS.  In both Manual modes the tracker also ends the capture when the
 * picture is over.  cont_ls is fed by audio_cb() under live_mutex. */
static bool cont_autodetect = true;
static sstv_linesync_t cont_ls;
static volatile bool cont_locked = false;
static volatile bool cont_lock_ui_pending = false;
static long cont_limit = 0;

/* Progressive display (sstv/preview.c): ui_timer_cb() draws the rows whose
 * audio has arrived, from a slice copied under live_mutex; the final decode
 * replaces it.  live_ls follows the Auto RX capture. */
#define SSTV_PV_SLICE_MAX (2 * 1051 * SSTV_SAMPLE_RATE / 1000 + 256)   /* 2 Scottie DX lines */
#define SSTV_PV_PERIOD_MS 200       /* canvas refresh while receiving */
#define SSTV_PV_MAX_ROWS  8         /* rows per refresh */
static sstv_linesync_t live_ls;
static sstv_preview_t live_pv, cont_pv;
static volatile bool live_pv_on = false, cont_pv_on = false;
static float pv_slice[SSTV_PV_SLICE_MAX];
static float live_vis_start_s = -1.0f;            /* VIS start in the last window found */
static volatile int live_unsupported_vis = -1;
static const char *volatile cont_fail_msg = NULL;    /* Manual decode gave no picture */

static const sstv_mode_t cont_modes[] = {
    SSTV_MODE_ROBOT8_BW, SSTV_MODE_ROBOT36, SSTV_MODE_ROBOT72,
    SSTV_MODE_MARTIN_M1, SSTV_MODE_MARTIN_M2,
    SSTV_MODE_SCOTTIE_S1, SSTV_MODE_SCOTTIE_S2
};

static const char *cont_mode_name(sstv_mode_t mode)
{
    switch (mode) {
        case SSTV_MODE_ROBOT8_BW: return "Robot 8";
        case SSTV_MODE_ROBOT36: return "Robot 36";
        case SSTV_MODE_ROBOT72: return "Robot 72";
        case SSTV_MODE_MARTIN_M1: return "Martin M1";
        case SSTV_MODE_MARTIN_M2: return "Martin M2";
        case SSTV_MODE_SCOTTIE_S1: return "Scottie S1";
        case SSTV_MODE_SCOTTIE_S2: return "Scottie S2";
        case SSTV_MODE_SCOTTIE_DX: return "Scottie DX";
        default: return "Unknown";
    }
}

/* RX Mode button label: Manual RX mode, "Auto" = from the line syncs */
static const char *cont_rx_mode_name(void)
{
    return cont_autodetect ? "Auto" : cont_mode_name(cont_mode);
}

/* Mode a Manual capture is decoded with (live_mutex held); unknown: the
 * decoder thread classifies the whole capture */
static sstv_mode_t cont_rx_decode_mode(void)
{
    if (!cont_autodetect) return cont_mode;
    return cont_locked ? sstv_linesync_status(&cont_ls)->mode : SSTV_MODE_UNKNOWN;
}

/* Nominal image duration for Manual RX.  The capture limit adds five seconds
 * so starting Manual RX slightly before the SSTV transmission does not cut
 * off the last rows.  SSTV_CONT_MAX_SECONDS remains the absolute safety cap. */
static float cont_mode_seconds(sstv_mode_t mode)
{
    switch (mode) {
        case SSTV_MODE_ROBOT8_BW:  return 8.0f;
        case SSTV_MODE_ROBOT36:    return 36.5f;
        case SSTV_MODE_ROBOT72:    return 72.5f;
        case SSTV_MODE_MARTIN_M1:  return 115.0f;
        case SSTV_MODE_MARTIN_M2:  return 59.0f;
        case SSTV_MODE_SCOTTIE_S1: return 111.0f;
        case SSTV_MODE_SCOTTIE_S2: return 72.0f;
        default:                    return (float)SSTV_CONT_MAX_SECONDS - 5.0f;
    }
}

static void update_rx_controls(void)
{
    bool auto_active = live_enabled || live_capturing || live_decode_pending || live_decoding;
    bool cont_active = cont_capturing || cont_decode_pending || cont_decoding;

    if (tx_ui_state != SSTV_TX_UI_RX) {
        buttons_disabled(&btn_start, true);
        buttons_disabled(&btn_cont_start, true);
        buttons_disabled(&btn_cont_mode, tx_ui_state == SSTV_TX_UI_RF);
        btn_page_1.items[3] = &btn_cont_mode;
        if (buttons_get_cur_page() == &btn_page_1) buttons_load_page(&btn_page_1);
        return;
    }

    buttons_disabled(&btn_start, cont_active);
    buttons_disabled(&btn_cont_start, auto_active || cont_decode_pending || cont_decoding);
    buttons_disabled(&btn_cont_mode, auto_active || cont_active);

    /* The mode selector is only present while both receivers are idle. */
    btn_page_1.items[3] = (!auto_active && !cont_active) ? &btn_cont_mode : NULL;
    if (buttons_get_cur_page() == &btn_page_1)
        buttons_load_page(&btn_page_1);
}

static void set_status(const char *text)
{
    if (status_label) lv_label_set_text(status_label, text ? text : "");

    /* The first line, without the full stop, as the state in the header */
    if (frame.head_right && text) {
        char   state[48];
        size_t n = strcspn(text, "\n");

        snprintf(state, sizeof(state), "%.*s", (int)n, text);
        n = strlen(state);
        if (n && state[n - 1] == '.') state[n - 1] = 0;
        lv_label_set_text(frame.head_right, state);
    }
}

static void header_update(void)
{
    int32_t     f = cparam_i_get(cfg_fg_freq);
    char        l[64];

    snprintf(l, sizeof(l), "SSTV   %.3f MHz", f / 1e6);
    if (strcmp(l, lv_label_get_text(frame.head_left)))
        lv_label_set_text(frame.head_left, l);
}

static void freq_cb(button_data_t *data)
{
    (void)data;
    if (freq_list_is_open())
        freq_list_close();
    else
        freq_list_open(dialog_sstv->obj, "SSTV - calling frequencies", sstv_freqs,
                       sizeof(sstv_freqs) / sizeof(sstv_freqs[0]), 0, NULL);
}

static void set_receiver_ui_stopped(void)
{
    set_status("Stand by.\n\n- Press Start Auto RX to start Autodecoder.\n\n- Press Start Manual RX to start Manual decoder, press RX Mode button to cycle Auto (mode from the line syncs) and all available modes for the Manual decoder.");
    if (progress_bar) {
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    btn_start.label = "Start\nAuto RX";
    buttons_disabled(&btn_start, false);
    if (btn_start.disp_btn) buttons_refresh(&btn_start);
    update_rx_controls();
}

static void set_receiver_ui_listening(void)
{
    set_status("SSTV Autodecoder is running.\n\nAwaiting signal...");
    if (progress_bar) {
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    btn_start.label = "Stop\nAuto RX";
    buttons_disabled(&btn_start, false);
    if (btn_start.disp_btn) buttons_refresh(&btn_start);
    update_rx_controls();
}

static void set_receiver_ui_decoding(const char *mode_name)
{
    char status[96];
    snprintf(status, sizeof(status), tr("Decoding %s"), mode_name ? mode_name : "SSTV");
    set_status(status);
    if (progress_bar) lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    btn_start.label = "Halt\nDecoding";
    buttons_disabled(&btn_start, false);
    if (btn_start.disp_btn) buttons_refresh(&btn_start);
    update_rx_controls();
}

static void reset_ui_to_standby(void)
{
    set_receiver_ui_stopped();
}

static bool save_sstv_png(const uint8_t *rgb, int width, int height, const char *name)
{
    if (!rgb || width <= 0 || height <= 0) return false;
    if (mkdir(SSTV_SAVE_DIR, 0775) != 0 && errno != EEXIST) {
        fprintf(stderr, "SSTV SAVE: mkdir failed: %s\n", strerror(errno)); return false;
    }
    char path[160];
    snprintf(path, sizeof(path), SSTV_SAVE_DIR "/%s", name);
    FILE *fp=fopen(path,"wb"); if(!fp){fprintf(stderr,"SSTV SAVE: fopen %s failed: %s\n",path,strerror(errno));return false;}
    png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
    png_infop info=png ? png_create_info_struct(png) : NULL;
    if(!png || !info){if(png)png_destroy_write_struct(&png,NULL);fclose(fp);return false;}
    if(setjmp(png_jmpbuf(png))){png_destroy_write_struct(&png,&info);fclose(fp);return false;}
    png_init_io(png,fp);
    png_set_IHDR(png,info,width,height,8,PNG_COLOR_TYPE_RGB,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png,info);
    for(int y=0;y<height;++y) png_write_row(png,(png_bytep)(rgb+(size_t)y*width*3));
    png_write_end(png,NULL); png_destroy_write_struct(&png,&info); fclose(fp);
    fprintf(stderr,"SSTV SAVE: image saved to %s (%dx%d)\n",path,width,height);
    return true;
}


static bool save_tx_png(const char *path, const uint8_t *rgb,
                        unsigned int width, unsigned int height)
{
    if (!path || !rgb || width == 0 || height == 0) return false;

    FILE *fp=fopen(path,"wb");
    if (!fp) {
        fprintf(stderr,"SSTV TX: fopen %s failed: %s\n",path,strerror(errno));
        return false;
    }

    png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
    png_infop info=png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        if (png) png_destroy_write_struct(&png,NULL);
        fclose(fp);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png,&info);
        fclose(fp);
        return false;
    }

    png_init_io(png,fp);
    png_set_IHDR(png,info,width,height,8,PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png,info);
    for (unsigned int y=0;y<height;y++)
        png_write_row(png,(png_bytep)(rgb+(size_t)y*width*3));
    png_write_end(png,NULL);
    png_destroy_write_struct(&png,&info);
    fclose(fp);

    fprintf(stderr,"SSTV TX: final WYSIWYG framebuffer written to %s (%ux%u)\n",
            path,width,height);
    return true;
}

/* Received picture in test_rgb: save it as sstv-STAMP.png; with Denoise on,
 * clean test_rgb (shown on screen afterwards) and save it as sstv-STAMP_dn.png.
 * Decoder threads only, before test_ready. */
static void save_rx_picture(void)
{
    char stem[40], name[48];
    time_t now = time(NULL);
    struct tm tm_now;

    if (!localtime_r(&now, &tm_now)) return;
    strftime(stem, sizeof(stem), "sstv-%Y%m%d-%H%M%S", &tm_now);
    snprintf(name, sizeof(name), "%s.png", stem);
    bool saved = save_sstv_png(test_rgb, test_info.width, test_info.height, name);
    if (params.sstv_denoise.x) {
        struct timespec t0;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sstv_denoise(test_rgb, dn_tmp, test_info.width, test_info.height);
        fprintf(stderr, "SSTV DENOISE: %dx%d in %llu ms\n", test_info.width, test_info.height,
                (unsigned long long)monotonic_ms_since(&t0));
        snprintf(name, sizeof(name), "%s_dn.png", stem);
        if (save_sstv_png(test_rgb, test_info.width, test_info.height, name))
            saved = true;
    }
    if (saved) {
        snprintf(saved_overlay_name, sizeof(saved_overlay_name), "%s", name);
        saved_overlay_pending = true;
    }
}

static void render_scaled_image(void)
{
    if (!sstv_canvas || test_rows <= 0 || test_info.width <= 0 || test_info.height <= 0) return;
    lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
    int out_h=SSTV_IMAGE_H;
    int out_w=(test_info.width*out_h)/test_info.height;
    if(out_w>SSTV_IMAGE_W){out_w=SSTV_IMAGE_W;out_h=(test_info.height*out_w)/test_info.width;}
    int x0=(SSTV_IMAGE_W-out_w)/2;
    int y0=(SSTV_IMAGE_H-out_h)/2;
    for(int oy=0;oy<out_h;++oy){
        int sy=(int)((int64_t)oy*test_info.height/out_h);
        for(int ox=0;ox<out_w;++ox){
            int sx=(int)((int64_t)ox*test_info.width/out_w);
            size_t o=((size_t)sy*test_info.width+sx)*3;
            lv_canvas_set_px(sstv_canvas,x0+ox,y0+oy,lv_color_make(test_rgb[o],test_rgb[o+1],test_rgb[o+2]));
        }
    }
    lv_obj_invalidate(sstv_canvas);
}


/* VIS detection for Auto RX: sstv/vis.c, from 1KO125's x6100_gui 1.0.2.
 * Called from audio_cb() with live_mutex held.  A valid header of a mode we
 * do not decode is reported by ui_timer_cb(). */
static bool live_find_vis(const float *x, int n, sstv_info_t *out)
{
    sstv_vis_result_t r;
    if (sstv_vis_detect(x, n, out, &r)) {
        live_vis_start_s = r.start_s + r.slip_s;
        fprintf(stderr, "SSTV LIVE: VIS=%d -> %s, offset %+.1f Hz, score %.2f\n",
                out->vis, out->name, r.offset_hz, r.score);
        return true;
    }
    if (r.unsupported_vis >= 0) live_unsupported_vis = r.unsupported_vis;
    return false;
}

/* Progressive display of the Auto RX capture just committed in
 * live_samples[0..live_written), whose last SSTV_VIS_WINDOW_SAMPLES are the
 * window the VIS was found in.  live_mutex held. */
static void live_preview_start(void)
{
    double vis = (double)(live_written - SSTV_VIS_WINDOW_SAMPLES) + live_vis_start_s * SSTV_SAMPLE_RATE;
    double row0 = vis + (SSTV_VIS_LEN_S + sstv_row0_anchor_after_vis(live_info.mode)) * SSTV_SAMPLE_RATE;
    sstv_linesync_init(&live_ls, live_info.mode);
    sstv_linesync_process(&live_ls, live_samples, (size_t)live_written);
    sstv_preview_init(&live_pv, live_info.mode, vis >= 0.0 ? row0 : -1.0);
    live_pv_on = true;
}

/* Draw the rows of pv whose audio has arrived in buf[0..*written), at most
 * SSTV_PV_MAX_ROWS, straight into the canvas buffer (scaled like
 * render_scaled_image()), then invalidate the canvas once.  The slice and
 * the row state are copied under live_mutex (audio_cb() may restart the
 * capture), the row is demodulated and drawn outside it.  Returns false
 * when the capture is over. */
static bool preview_tick(sstv_preview_t *pv, const sstv_linesync_t *ls, float *const *buf,
                         const long *written, volatile bool *capturing)
{
    uint8_t        rgb[SSTV_MAX_W * 3];
    sstv_preview_t row_pv;
    int            drawn = 0;

    for (int k = 0; k < SSTV_PV_MAX_ROWS; ++k) {
        long a, b;
        bool ok = false, on = true;
        pthread_mutex_lock(&live_mutex);
        if (!*capturing || !*buf) on = false;
        else if (sstv_preview_need(pv, *written, sstv_linesync_status(ls), &a, &b) && b - a <= SSTV_PV_SLICE_MAX) {
            memcpy(pv_slice, *buf + a, (size_t)(b - a) * sizeof(float));
            row_pv = *pv;
            pv->next_row++;
            ok = true;
        }
        pthread_mutex_unlock(&live_mutex);
        if (!on) return false;
        if (!ok) break;
        int row = sstv_preview_render(&row_pv, pv_slice, rgb);
        if (row < 0 || !sstv_canvas) continue;
        int h = row_pv.height, w = row_pv.width, out_h = SSTV_IMAGE_H, out_w = (w * out_h) / h;
        if (out_w > SSTV_IMAGE_W) { out_w = SSTV_IMAGE_W; out_h = (h * out_w) / w; }
        int x0 = (SSTV_IMAGE_W - out_w) / 2, y0 = (SSTV_IMAGE_H - out_h) / 2;
        int oy1 = (int)(((int64_t)(row + 1) * out_h + h - 1) / h);
        for (int oy = (int)(((int64_t)row * out_h + h - 1) / h); oy < oy1 && oy < out_h; ++oy)
            for (int ox = 0; ox < out_w; ++ox) {
                const uint8_t *q = rgb + (size_t)(ox * w / out_w) * 3;
                sstv_buffer[(size_t)(y0 + oy) * SSTV_CANVAS_W + x0 + ox] = lv_color_make(q[0], q[1], q[2]);
            }
        ++drawn;
    }
    if (drawn && sstv_canvas) lv_obj_invalidate(sstv_canvas);
    return true;
}

static float live_mode_seconds(sstv_mode_t m)
{
    switch(m) {
        case SSTV_MODE_ROBOT8_BW: return 8.0f;   /* VIS 2 is found since sstv/vis.c */
        case SSTV_MODE_ROBOT36: return 36.5f;
        case SSTV_MODE_ROBOT72: return 72.5f;
        case SSTV_MODE_MARTIN_M1: return 115.0f;
        case SSTV_MODE_MARTIN_M2: return 59.0f;
        case SSTV_MODE_SCOTTIE_S1: return 111.0f;
        case SSTV_MODE_SCOTTIE_S2: return 72.0f;
        case SSTV_MODE_SCOTTIE_DX: return 270.0f;
        default: return 120.0f;
    }
}

static button_data_t btn_sstv_page_1 = {
    .type = BTN_TEXT, .label = "(SSTV 1:3)", .press = page_2_cb
};
static button_data_t btn_sstv_page_2 = {
    .type = BTN_TEXT, .label = "(SSTV 2:3)", .press = page_3_cb
};
static button_data_t btn_sstv_page_3 = {
    .type = BTN_TEXT, .label = "(SSTV 3:3)", .press = page_1_cb
};
static button_data_t btn_start = { .type = BTN_TEXT, .label = "Start\nAuto RX", .press = start_cb };
static button_data_t btn_cont_start = { .type = BTN_TEXT, .label = "Start\nManual RX", .press = cont_start_cb };
static button_data_t btn_cont_mode = { .type = BTN_TEXT, .label = "RX Mode\nAuto", .press = cont_mode_cb };
static button_data_t btn_browser = { .type = BTN_TEXT, .label = "RX Pics\nBrowser", .press = browser_cb };
static button_data_t btn_delete_picture = { .type = BTN_TEXT, .label = "Delete\npicture", .press = delete_picture_cb };
static button_data_t btn_tx = { .type = BTN_TEXT, .label = "TX", .press = tx_ui_cb };
static button_data_t btn_restart = { .type = BTN_TEXT, .label = "Auto RX\nRestart ON", .press = restart_cb };
static button_data_t btn_test = { .type = BTN_TEXT, .label = "Test RX", .press = test_cb };
static button_data_t btn_denoise = { .type = BTN_TEXT, .label = "Denoise\nON", .press = denoise_cb };
static button_data_t btn_close = { .type = BTN_TEXT, .label = "Close", .press = close_cb };
static button_data_t btn_freq = { .type = BTN_TEXT, .label = "Freq.", .press = freq_cb };

static buttons_page_t btn_page_1 = {
    .items = { &btn_sstv_page_1, &btn_start, &btn_cont_start, &btn_cont_mode, &btn_tx }
};
static buttons_page_t btn_page_2 = {
    .items = { &btn_sstv_page_2, &btn_freq, &btn_browser, &btn_delete_picture, &btn_restart }
};
static buttons_page_t btn_page_3 = {
    .items = { &btn_sstv_page_3, &btn_test, &btn_dig_pwr, &btn_denoise, &btn_close }
};

static dialog_t dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = audio_cb,
    .key_cb = key_cb,
    .restore_radio = true,
    .btn_page = &btn_page_1
};

dialog_t *dialog_sstv = &dialog;

static void construct_cb(lv_obj_t *parent)
{
    dialog.obj = dialog_init(parent);
    lv_obj_set_size(dialog.obj, 795, DIALOG_H);   /* at DIALOG_Y (dialog_style) */

    /* Common app look: header, picture and state in the black body, status line */
    app_frame_create(&frame, dialog.obj);
    lv_label_set_text(frame.status, SSTV_STATUS_TEXT);
    header_update();

    lv_obj_t *body = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(body);
    app_frame_body(body);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    if (param_f_get(cfg_pwr) > MAX_PWR) {
        radio_set_pwr(MAX_PWR);
        msg_schedule_text_fmt("Power is limited to %0.0fW", MAX_PWR);
    }

    sstv_canvas = lv_canvas_create(dialog.obj);
    lv_canvas_set_buffer(
        sstv_canvas,
        sstv_buffer,
        SSTV_CANVAS_W,
        SSTV_CANVAS_H,
        LV_IMG_CF_TRUE_COLOR
    );
    lv_obj_set_size(sstv_canvas, SSTV_CANVAS_W, SSTV_CANVAS_H);
    lv_canvas_fill_bg(sstv_canvas, TC(frame), LV_OPA_COVER);

    lv_obj_move_background(body);

    /* White image sheet: maximum 4:3 SSTV area, at the left of the body */
    lv_obj_align(sstv_canvas, LV_ALIGN_TOP_LEFT, SSTV_IMAGE_X, SSTV_IMAGE_Y);
    lv_obj_set_size(sstv_canvas, SSTV_IMAGE_W, SSTV_IMAGE_H);
    lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);

    /* Route front-panel key/encoder events to SSTV just like WEFAX/NAVTEX.
     * Without a focused object in keyboard_group, key_cb() never receives
     * the volume knob events while the SSTV dialog is open. */
    lv_group_add_obj(keyboard_group, sstv_canvas);
    lv_obj_add_event_cb(sstv_canvas, key_cb, LV_EVENT_KEY, NULL);
    lv_group_focus_obj(sstv_canvas);
    lv_group_set_editing(keyboard_group, true);

    status_label = lv_label_create(dialog.obj);
    lv_label_set_text(status_label, "Stand by.\nPress Start Auto RX to start Autodecoder.\nPress Start Manual RX to start Manual decoder, press RX Mode button to cycle Auto (mode from the line syncs) and all available modes for the Manual decoder.");
    lv_obj_set_style_text_color(status_label, TC(text), 0);
    lv_obj_set_style_text_font(status_label, &sony_22, 0);
    lv_obj_set_width(status_label, APP_FRAME_W - SSTV_IMAGE_W - 30);
    lv_obj_set_pos(status_label, SSTV_IMAGE_X + SSTV_IMAGE_W + 15, APP_FRAME_BODY_Y + 10);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_LEFT, 0);

    progress_bar = frame.bar;
    lv_obj_set_style_bg_color(progress_bar, TC(ok_bar), LV_PART_INDICATOR);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    set_receiver_ui_stopped();

    /* RESTART defaults ON: a new valid VIS restarts the current Auto RX capture. */
    btn_restart.label = live_restart_enabled ? "Auto RX\nRestart ON" : "Auto RX\nRestart OFF";
    buttons_disabled(&btn_restart, false);
    buttons_disabled(&btn_test, false);
    btn_denoise.label = params.sstv_denoise.x ? "Denoise\nON" : "Denoise\nOFF";

    shown_rows = 0;
    test_ready = false;
    test_rows = 0;
    live_canvas_clear_pending = false;
    live_abort_requested = false;
    live_mode_ui_pending = false;
    ui_timer = lv_timer_create(ui_timer_cb, 30, NULL);
}

static void destruct_cb(void)
{
    /* Closing the SSTV dialog is a hard receiver reset.  Stop both RX paths
     * before destroying the LVGL objects so audio_cb() cannot append to a
     * buffer while it is being released. */
    pthread_mutex_lock(&live_mutex);
    live_enabled = false;
    live_capturing = false;
    live_decode_pending = false;
    cont_capturing = false;
    cont_decode_pending = false;
    pthread_mutex_unlock(&live_mutex);

    /* Never allow the dialog to disappear while its TX worker can still own
     * the modem.  Request abort and wait for radio_set_modem(false). */
    if (tx_thread_valid) {
        tx_abort = true;
        pthread_join(tx_thread, NULL);
        tx_thread_valid = false;
    }
    tx_running = false;
    tx_started = false;

    if (ui_timer) {
        lv_timer_del(ui_timer);
        ui_timer = NULL;
    }

    /* A decoder thread may already be using live_samples/cont_samples and
     * test_rgb.  Let it leave cleanly before freeing/resetting that state. */
    if (test_thread_valid) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    /* A decoder that was already running can re-arm Auto RX when it exits,
     * so force every receiver flag back to idle after the join as well. */
    pthread_mutex_lock(&live_mutex);
    live_enabled = false;
    live_capturing = false;
    live_decode_pending = false;
    live_decoding = false;
    live_canvas_clear_pending = false;
    live_abort_requested = false;
    live_mode_ui_pending = false;
    live_ring_pos = 0;
    live_ring_count = 0;
    live_since_check = 0;
    live_wait_samples = 0;
    live_wait_log_mark = 0;

    cont_capturing = false;
    cont_decode_pending = false;
    cont_decoding = false;
    cont_locked = false;
    cont_lock_ui_pending = false;
    cont_fail_msg = NULL;
    live_pv_on = cont_pv_on = false;
    live_unsupported_vis = -1;
    pthread_mutex_unlock(&live_mutex);

    free(live_samples);
    live_samples = NULL;
    live_written = 0;
    live_target = 0;

    free(cont_samples);
    cont_samples = NULL;
    cont_written = 0;
    cont_capacity = 0;

    test_running = false;
    test_ready = false;
    test_rows = 0;
    shown_rows = 0;
    saved_overlay_pending = false;
    saved_overlay_name[0] = '\0';

    /* Button_data objects are static and survive dialog destruction.  Restore
     * their default labels/mode now so the next open always starts clean. */
    cont_mode = SSTV_MODE_ROBOT8_BW;
    cont_autodetect = true;
    cont_decode_mode = SSTV_MODE_ROBOT8_BW;
    btn_start.label = "Start\nAuto RX";
    btn_cont_start.label = "Start\nManual RX";
    btn_cont_mode.label = "RX Mode\nAuto";
    btn_tx.label = "TX";
    tx_ui_state = SSTV_TX_UI_RX;
    tx_restore_auto_rx = false;
    btn_page_1.items[3] = &btn_cont_mode;

    tx_color_selector_destroy();
    tx_template_selector_destroy();
    tx_type_selector_destroy();
    tx_picture_selector_destroy();
    tx_instruction_hide();
    tx_phase = SSTV_TX_PHASE_NONE;
    sstv_browser_close();
    radio_set_pwr(param_f_get(cfg_pwr));   /* the 7 W cap ends with the app, as in WSPR */

    fprintf(stderr, "SSTV: dialog closed; Auto RX and Manual RX reset to defaults\n");

    sstv_canvas = NULL;
    sstv_frame = NULL;
    status_label = NULL;
    progress_bar = NULL;
    frame.head_left = frame.head_right = frame.status = NULL;  /* deleted with the dialog */
    freq_list_close();
}

static void *test_thread_cb(void *arg)
{
    const char *path = arg ? (const char *)arg : SSTV_TEST_WAV;
    fprintf(stderr, "SSTV %s: starting automatic VIS detection\n", arg ? "LIVE" : "TEST");
    int rows = sstv_decode_wav(path, test_rgb, &test_info);
    if (rows > 0 && params.sstv_denoise.x)
        sstv_denoise(test_rgb, dn_tmp, test_info.width, test_info.height);
    test_rows = rows > 0 ? rows : 0;
    test_ready = true;
    test_running = false;
    if (arg) {
        pthread_mutex_lock(&live_mutex);
        live_ring_pos=live_ring_count=0; live_since_check=0; live_wait_samples=live_wait_log_mark=0;
        live_enabled=true;
        pthread_mutex_unlock(&live_mutex);
        fprintf(stderr,"SSTV LIVE: decoder finished; listening again for next VIS header\n");
    }
    return NULL;
}


static bool save_debug_wav(const char *path, const float *samples, long count)
{
    if (!path || !samples || count <= 0) return false;
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "SSTV LIVE: ERROR opening debug WAV %s\n", path);
        return false;
    }

    uint32_t data_bytes = (uint32_t)count * 2u;
    uint32_t riff_size = 36u + data_bytes;
    uint32_t sample_rate = SSTV_SAMPLE_RATE;
    uint32_t byte_rate = sample_rate * 2u;
    uint16_t audio_format = 1, channels = 1, block_align = 2, bits = 16;

    fwrite("RIFF", 1, 4, fp); fwrite(&riff_size, 4, 1, fp); fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    uint32_t fmt_size = 16; fwrite(&fmt_size, 4, 1, fp);
    fwrite(&audio_format, 2, 1, fp); fwrite(&channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp); fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&data_bytes, 4, 1, fp);

    int16_t pcm[1024];
    long pos = 0;
    while (pos < count) {
        long n = count - pos;
        if (n > (long)(sizeof(pcm) / sizeof(pcm[0]))) n = (long)(sizeof(pcm) / sizeof(pcm[0]));
        for (long i = 0; i < n; ++i) {
            float v = samples[pos + i];
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            pcm[i] = (int16_t)lrintf(v * 32767.0f);
        }
        if (fwrite(pcm, sizeof(int16_t), (size_t)n, fp) != (size_t)n) {
            fprintf(stderr, "SSTV LIVE: ERROR writing debug WAV %s\n", path);
            fclose(fp);
            return false;
        }
        pos += n;
    }
    fclose(fp);
    fprintf(stderr, "SSTV LIVE: debug WAV saved to %s (%ld samples @ %d Hz)\n",
            path, count, SSTV_SAMPLE_RATE);
    return true;
}

static void *live_decode_thread_cb(void *arg)
{
    (void)arg;
    fprintf(stderr, "SSTV LIVE: decoding captured %s directly from RAM (%ld samples @ %d Hz)\n",
            live_info.name, live_written, SSTV_SAMPLE_RATE);
    save_debug_wav(SSTV_DEBUG_WAV, live_samples, live_written);
    int rows = sstv_decode_samples(live_samples, live_written, live_info.mode, test_rgb, &test_info);
    bool aborted = live_abort_requested;
    test_rows = (!aborted && rows > 0) ? rows : 0;
    if (!aborted && rows > 0) {
        save_rx_picture();
    }
    test_ready = !aborted;
    live_abort_requested = false;
    free(live_samples); live_samples = NULL; live_written = live_target = 0;
    test_running = false;
    live_decoding = false;
    pthread_mutex_lock(&live_mutex);
    live_ring_pos=live_ring_count=0; live_since_check=0; live_wait_samples=live_wait_log_mark=0;
    live_enabled=true;
    pthread_mutex_unlock(&live_mutex);
    fprintf(stderr,"SSTV LIVE: RAM decoder finished, rows=%d; listening again for next VIS header\n", rows);
    return NULL;
}

static void *cont_decode_thread_cb(void *arg)
{
    (void)arg;
    /* Auto without a real-time lock: the mode from the whole capture */
    if (cont_decode_mode == SSTV_MODE_UNKNOWN)
        cont_decode_mode = sstv_linesync_classify(cont_samples, cont_written, SSTV_MODE_UNKNOWN, NULL);
    const char *name = cont_mode_name(cont_decode_mode);
    fprintf(stderr, "SSTV CONT: decoding manual %s capture from RAM (%ld samples @ %d Hz)\n",
            name, cont_written, SSTV_SAMPLE_RATE);
    int rows = cont_decode_mode == SSTV_MODE_UNKNOWN ? -1 :
               sstv_decode_cont_samples(cont_samples, cont_written, cont_decode_mode, test_rgb, &test_info);
    test_rows = rows > 0 ? rows : 0;
    if (rows > 0) {
        save_rx_picture();
    }
    test_ready = true;
    free(cont_samples); cont_samples = NULL;
    cont_written = cont_capacity = 0;
    test_running = false;
    cont_decoding = false;
    if (rows <= 0)
        cont_fail_msg = cont_decode_mode == SSTV_MODE_UNKNOWN ? "No SSTV picture found" : "Decoding failed";
    fprintf(stderr, "SSTV CONT: decoder finished, rows=%d; receiver returned to stand-by\n", rows);
    return NULL;
}

static void ui_timer_cb(lv_timer_t *timer)
{
    if (frame.head_left)
        header_update();
    (void)timer;

    if (tx_target_keyboard_open_pending) {
        tx_target_keyboard_open_pending=false;
        tx_target_keyboard_open_now();
    }

    if (tx_target_keyboard_force_until != 0) {
        uint32_t now=lv_tick_get();
        if ((int32_t)(tx_target_keyboard_force_until-now) > 0) {
            tx_target_keyboard_force_lower();
        } else {
            tx_target_keyboard_force_lower();
            tx_target_keyboard_force_until=0;
        }
    }

    /* audio_cb() runs outside the LVGL/UI context.  It only requests the
     * clear; the actual canvas operation is performed here by the UI timer. */
    if (live_canvas_clear_pending) {
        live_canvas_clear_pending = false;
        if (sstv_canvas) {
            lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
            lv_obj_invalidate(sstv_canvas);
        }
    }

    /* VIS detection runs in the audio callback.  Update LVGL/button text only
     * here, in the UI context. */
    if (live_mode_ui_pending) {
        live_mode_ui_pending = false;
        snprintf(tx_mode_label,sizeof(tx_mode_label),"RX Mode\n%s",cont_rx_mode_name());
        btn_cont_mode.label=tx_mode_label;
        if (btn_cont_mode.disp_btn) buttons_refresh(&btn_cont_mode);
    }

    if (live_unsupported_vis >= 0) {
        msg_schedule_text_fmt("SSTV mode not supported (VIS %d)", live_unsupported_vis);
        live_unsupported_vis = -1;
    }

    if (cont_lock_ui_pending) {
        char status[192];
        cont_lock_ui_pending = false;
        snprintf(status, sizeof(status),
                 tr("SSTV Manual decoder is running\n\n%s line syncs found\n\nPress Stop Manual RX to render received picture"),
                 cont_mode_name(sstv_linesync_status(&cont_ls)->mode));
        if (cont_capturing) set_status(status);
    }

    /* Progressive display, in chunks of rows */
    static uint32_t pv_last_ms;
    if ((live_pv_on || cont_pv_on) && lv_tick_elaps(pv_last_ms) >= SSTV_PV_PERIOD_MS) {
        pv_last_ms = lv_tick_get();
        if (live_pv_on && !preview_tick(&live_pv, &live_ls, &live_samples, &live_written, &live_capturing))
            live_pv_on = false;
        if (cont_pv_on && !preview_tick(&cont_pv, &cont_ls, &cont_samples, &cont_written, &cont_capturing))
            cont_pv_on = false;
    }

    if (cont_fail_msg) {
        msg_schedule_text_fmt("%s", cont_fail_msg);
        cont_fail_msg = NULL;
        test_ready = false;
        reset_ui_to_standby();
    }

    if (tx_ui_state == SSTV_TX_UI_SETUP &&
        tx_phase == SSTV_TX_PHASE_SELECT_PICTURE)
        tx_picture_load_pending_preview();

    if (tx_ui_state == SSTV_TX_UI_SETUP &&
        tx_phase == SSTV_TX_PHASE_SELECT_TEMPLATE &&
        tx_template_preview_pending) {
        tx_template_preview_pending = false;
        tx_template_render_selected();
    }

    if (tx_ui_state == SSTV_TX_UI_SETUP &&
        tx_phase == SSTV_TX_PHASE_SELECT_COLORS &&
        tx_color_preview_pending &&
        (uint32_t)(lv_tick_get()-tx_color_preview_since_ms) >= 300) {
        tx_color_preview_pending = false;
        tx_color_render_selected();
    }

    if (saved_overlay_pending) {
        char message[160];
        saved_overlay_pending = false;
        snprintf(message, sizeof(message), tr("SSTV picture %s saved"), saved_overlay_name);
        msg_schedule_text_fmt("%s", message);

        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "t", "sstv_rx");
        cJSON_AddStringToObject(o, "file", saved_overlay_name);
        cJSON_AddStringToObject(o, "mode", test_info.name ? test_info.name : "");
        remote_event(o);
    }

    if (tx_running) {
        if (progress_bar) {
            int v = 0;
            if (tx_key_time_valid) {
                uint64_t keyed_ms = monotonic_ms_since(&tx_key_time);
                if (keyed_ms > SSTV_TX_PREROLL_MS) {
                    uint64_t payload_ms = keyed_ms - SSTV_TX_PREROLL_MS;
                    unsigned int duration_ms = sstv_tx_duration_ms(tx_mode);
                    if (duration_ms > 0) {
                        if (payload_ms >= duration_ms) v = 1000;
                        else v = (int)((payload_ms * 1000u) / duration_ms);
                    }
                }
            }
            tx_progress = v;
            app_frame_bar(&frame, v, theme_hex(TC_tx));   /* red while transmitting, as in WSPR */
        }
        if (tx_started) { char msg[96]; snprintf(msg,sizeof(msg),tr("Transmitting %s..."),cont_mode_name(tx_mode)); set_status(msg); }
    } else if (tx_thread_valid) {
        pthread_join(tx_thread, NULL);
        tx_thread_valid = false;
        if (progress_bar) {
            lv_bar_set_value(progress_bar, tx_result == 1 ? 1000 : 0, LV_ANIM_OFF);
            lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(progress_bar, TC(ok_bar), LV_PART_INDICATOR);
        }
        if (tx_ui_state == SSTV_TX_UI_RF) {
            cJSON *o = cJSON_CreateObject();
            cJSON_AddStringToObject(o, "t", "sstv_txdone");
            cJSON_AddNumberToObject(o, "result", tx_result);
            remote_event(o);

            if (tx_result == 1) {
                /* Commit LAST TX only after the RF worker has completed the
                 * entire transmission successfully.  Aborts/errors leave the
                 * previous backup untouched. */
                size_t last_bytes=(size_t)tx_picture_width*tx_picture_height*3;
                if (tx_picture_width && tx_picture_height &&
                    last_bytes <= sizeof(tx_last_rgb)) {
                    memcpy(tx_last_rgb,tx_final_rgb,last_bytes);
                    tx_last_width=tx_picture_width;
                    tx_last_height=tx_picture_height;
                    tx_last_mode=tx_mode;
                    tx_last_valid=true;
                    fprintf(stderr,"SSTV TX: saved LAST TX (%ux%u, %s)\n",
                            tx_last_width,tx_last_height,cont_mode_name(tx_last_mode));
                }
                char msg[96];
                snprintf(msg,sizeof(msg),tr("SSTV %s TX complete"),cont_mode_name(tx_mode));
                msg_schedule_text_fmt("%s", msg);
            } else if (tx_result == -1) {
                msg_schedule_text_fmt("SSTV TX halted");
            } else {
                char msg[96];
                snprintf(msg,sizeof(msg),tr("SSTV TX failed (%d)"),tx_result);
                msg_schedule_text_fmt("%s", msg);
            }

            /* QSO workflow: completion, Halt and even an encoder/audio error
             * all leave the radio immediately listening in Auto RX. */
            tx_started=false;
            tx_progress=0;
            tx_return_to_auto_rx();
        } else {
            if (tx_result == 1) {
                char msg[96];
                snprintf(msg,sizeof(msg),tr("%s test TX complete."),cont_mode_name(tx_mode));
                set_status(msg);
                snprintf(msg,sizeof(msg),tr("SSTV %s test TX complete"),cont_mode_name(tx_mode));
                msg_schedule_text_fmt("%s", msg);
            } else if (tx_result == -1) {
                set_status("SSTV test TX aborted.");
            } else {
                char msg[96];
                snprintf(msg,sizeof(msg),tr("SSTV test TX failed (%d)."),tx_result);
                set_status(msg);
            }
            tx_started=false;
            tx_progress=0;
        }
    }

    if (test_thread_valid && !test_running) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    if (cont_decode_pending && !test_running && !test_thread_valid) {
        cont_decode_pending = false;
        cont_decoding = true;
        fprintf(stderr, "SSTV CONT: STOP - starting manual RAM decoder\n");
        char status[96];
        snprintf(status, sizeof(status), tr("Decoding Cont %s"),
                 cont_decode_mode == SSTV_MODE_UNKNOWN ? "Auto" : cont_mode_name(cont_decode_mode));
        set_status(status);
        if (progress_bar) lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
        shown_rows=0; test_rows=0; test_ready=false; test_running=true;
        update_rx_controls();
        int rc=pthread_create(&test_thread,NULL,cont_decode_thread_cb,NULL);
        if (rc==0) test_thread_valid=true;
        else { fprintf(stderr,"SSTV CONT: decoder pthread_create failed: %d\n",rc); test_running=false; cont_decoding=false; }
    }

    if (live_decode_pending && !test_running && !test_thread_valid) {
        live_decode_pending = false;
        live_decoding = true;
        fprintf(stderr, "SSTV LIVE: capture complete; starting direct RAM image decoder\n");
        set_receiver_ui_decoding(live_info.name);
        if (progress_bar) lv_bar_set_value(progress_bar,1000,LV_ANIM_OFF);
        shown_rows=0; test_rows=0; test_ready=false; test_running=true;
        int rc=pthread_create(&test_thread,NULL,live_decode_thread_cb,NULL);
        if (rc==0) test_thread_valid=true;
        else { fprintf(stderr,"SSTV LIVE: decoder pthread_create failed: %d\n",rc); test_running=false; live_decoding=false; }
    }

    /* Live progress follows the expected capture duration. */
    if (live_capturing && live_target > 0) {
        set_receiver_ui_decoding(live_info.name);
        if (progress_bar) {
            int v=(int)((live_written*1000L)/live_target); if(v<0)v=0; if(v>1000)v=1000;
            lv_bar_set_value(progress_bar,v,LV_ANIM_OFF);
        }
    }

    /* A structurally failed Auto decode must not leave the receiver stuck at
     * the end of the progress bar.  Discard the failed attempt, report it to
     * the operator, then resume VIS listening. */
    if (test_ready && test_rows <= 0 && live_enabled) {
        test_ready = false;
        shown_rows = 0;
        if (progress_bar)
            lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        msg_schedule_text_fmt("Decoding failed");
        set_receiver_ui_listening();
        return;
    }

    if (!sstv_canvas || !test_ready || shown_rows >= test_rows) return;
    render_scaled_image();
    shown_rows=test_rows;
    if (progress_bar) lv_bar_set_value(progress_bar,1000,LV_ANIM_OFF);
    if (live_enabled)
        set_receiver_ui_listening();
    else
        reset_ui_to_standby();
}


static void key_cb(lv_event_t *e)
{
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    switch (key) {
        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            return;
        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            return;
        default:
            break;
    }
}

static bool tx_abort_cb(void *ctx)
{
    (void)ctx;
    return tx_abort;
}

static void *tx_thread_cb(void *arg)
{
    (void)arg;
    int16_t *samples = NULL;
    size_t count = 0;

    int rc;
    switch (tx_mode) {
        case SSTV_MODE_ROBOT8_BW:
            rc = sstv_encode_robot8_png_pcm(SSTV_TX_PNG, AUDIO_PLAY_RATE, &samples, &count);
            break;
        case SSTV_MODE_MARTIN_M1:
            rc = sstv_encode_martin_m1_png_pcm(SSTV_TX_PNG, AUDIO_PLAY_RATE, &samples, &count);
            break;
        case SSTV_MODE_MARTIN_M2:
            rc = sstv_encode_martin_m2_png_pcm(SSTV_TX_PNG, AUDIO_PLAY_RATE, &samples, &count);
            break;
        case SSTV_MODE_SCOTTIE_S1:
            rc = sstv_encode_scottie_s1_png_pcm(SSTV_TX_PNG, AUDIO_PLAY_RATE, &samples, &count);
            break;
        case SSTV_MODE_ROBOT36:
            rc = sstv_encode_robot36_png_pcm(SSTV_TX_PNG, AUDIO_PLAY_RATE, &samples, &count);
            break;
        case SSTV_MODE_ROBOT72:
            rc = sstv_encode_robot72_png_pcm(SSTV_TX_PNG, AUDIO_PLAY_RATE, &samples, &count);
            break;
        case SSTV_MODE_SCOTTIE_S2:
            rc = sstv_encode_scottie_s2_png_pcm(SSTV_TX_PNG, AUDIO_PLAY_RATE, &samples, &count);
            break;
        default:
            fprintf(stderr,"SSTV TX: encoder not implemented for %s\n", cont_mode_name(tx_mode));
            rc = -40;
            break;
    }
    if (rc < 0) {
        fprintf(stderr,"SSTV TX: PCM generation failed rc=%d\n",rc);
        tx_result = rc;
        tx_running = false;
        return NULL;
    }
    if (tx_abort) {
        free(samples);
        tx_result = -1;
        tx_running = false;
        return NULL;
    }

    /* Give the TX/codec path time to settle before the SSTV VIS leader starts:
     * silence in front of the picture.  Kept out of the encoder so generated
     * WAV files remain standard. */
    const size_t preroll_count = ((size_t)AUDIO_PLAY_RATE * SSTV_TX_PREROLL_MS) / 1000u;
    int16_t *p = realloc(samples, (preroll_count + count) * sizeof(*samples));
    if (!p) {
        free(samples);
        tx_result = -32;
        tx_running = false;
        return NULL;
    }
    samples = p;
    memmove(samples + preroll_count, samples, count * sizeof(*samples));
    memset(samples, 0, preroll_count * sizeof(*samples));
    count += preroll_count;

    /* Same TX path as FT8/JS8/WSPR: tx_player keys the radio, keeps the
     * power at 5 W at most and corrects the level from ALC and power.  The
     * tones are absolute audio frequencies, so no VFO shift (offset = its
     * centre tone). */
    fprintf(stderr,"SSTV TX: keying modem for %s (%zu samples)\n", cont_mode_name(tx_mode), count);
    clock_gettime(CLOCK_MONOTONIC, &tx_key_time);
    tx_key_time_valid = true;
    tx_started = true;
    bool done = tx_player_play(samples, (uint32_t)count, TX_PLAYER_AUDIO_HZ,
                               tx_player_base_gain_offset(), tx_abort_cb, NULL);
    tx_started = false;
    tx_key_time_valid = false;
    free(samples);

    if (!done || tx_abort) {
        fprintf(stderr,"SSTV TX: aborted\n");
        tx_result = -1;
    } else {
        fprintf(stderr,"SSTV TX: %s complete\n", cont_mode_name(tx_mode));
        tx_progress = 1000;
        tx_result = 1;
    }
    tx_running = false;
    return NULL;
}

static void restart_cb(button_data_t *data)
{
    (void)data;

    pthread_mutex_lock(&live_mutex);
    live_restart_enabled = !live_restart_enabled;

    /* When enabling during an active capture, start the parallel VIS detector
     * from fresh audio.  This prevents the VIS which started the current
     * picture from immediately being detected again as a restart. */
    if (live_restart_enabled && live_capturing) {
        live_ring_pos = 0;
        live_ring_count = 0;
        live_since_check = 0;
    }
    pthread_mutex_unlock(&live_mutex);

    btn_restart.label = live_restart_enabled ? "Auto RX\nRestart ON" : "Auto RX\nRestart OFF";
    if (btn_restart.disp_btn) buttons_refresh(&btn_restart);
    fprintf(stderr,"SSTV LIVE: VIS restart %s\n",live_restart_enabled ? "ON" : "OFF");
}

/* Takes effect from the next picture; the original is always saved */
static void denoise_cb(button_data_t *data)
{
    (void)data;
    params_bool_set(&params.sstv_denoise, !params.sstv_denoise.x);
    btn_denoise.label = params.sstv_denoise.x ? "Denoise\nON" : "Denoise\nOFF";
    if (btn_denoise.disp_btn) buttons_refresh(&btn_denoise);
}

static void close_cb(button_data_t *data)
{
    (void)data;
    dialog_destruct();
}

static void test_cb(button_data_t *data)
{
    (void)data;

    if (!sstv_canvas || test_running || cont_capturing || cont_decode_pending || cont_decoding)
        return;

    if (live_enabled || live_capturing) {
        pthread_mutex_lock(&live_mutex);
        live_enabled=false; live_capturing=false;
        free(live_samples); live_samples=NULL; live_written=live_target=0;
        pthread_mutex_unlock(&live_mutex);
        fprintf(stderr,"SSTV LIVE: paused by WAV Test\n");
    }

    if (test_thread_valid) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
    set_status("Decoding Test WAV");
    if (progress_bar) {
        lv_bar_set_value(progress_bar,0,LV_ANIM_OFF);
        lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    shown_rows = 0;
    test_rows = 0;
    test_ready = false;
    test_running = true;

    int rc = pthread_create(&test_thread, NULL, test_thread_cb, NULL);
    if (rc != 0) {
        fprintf(stderr, "SSTV TEST: pthread_create failed: %d\n", rc);
        test_running = false;
        return;
    }
    test_thread_valid = true;
}

static void page_1_cb(button_data_t *data)
{
    (void)data; buttons_load_page(&btn_page_1);
}

static void page_2_cb(button_data_t *data)
{
    (void)data;
    buttons_load_page(&btn_page_2);
    sstv_browser_update_buttons();
}

static void page_3_cb(button_data_t *data)
{
    (void)data; buttons_load_page(&btn_page_3);
}


static uint64_t tx_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static int tx_picture_name_compare(const void *a, const void *b)
{
    const char *const *sa = a;
    const char *const *sb = b;
    return strcmp(*sa, *sb);
}

static void tx_picture_free_files(void)
{
    for (size_t i = 0; i < tx_picture_file_count; ++i)
        free(tx_picture_files[i]);
    free(tx_picture_files);
    tx_picture_files = NULL;
    tx_picture_file_count = 0;
    tx_picture_file_capacity = 0;
}

static void tx_picture_selector_refresh(void)
{
    tx_picture_free_files();

    DIR *dir = opendir(SSTV_TX_PICS_DIR);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            const char *name = entry->d_name;
            size_t len = strlen(name);

            /* Ignore hidden files, including macOS AppleDouble metadata (._*). */
            if (name[0] == '.')
                continue;

            if (len < 5 || strcmp(name + len - 4, ".png") != 0)
                continue;

            if (tx_picture_file_count == tx_picture_file_capacity) {
                size_t new_capacity = tx_picture_file_capacity ?
                                      tx_picture_file_capacity * 2 : 32;
                char **p = realloc(tx_picture_files,
                                   new_capacity * sizeof(*tx_picture_files));
                if (!p)
                    break;
                tx_picture_files = p;
                tx_picture_file_capacity = new_capacity;
            }

            char *copy = strdup(name);
            if (copy)
                tx_picture_files[tx_picture_file_count++] = copy;
        }
        closedir(dir);
    }

    qsort(tx_picture_files, tx_picture_file_count,
          sizeof(tx_picture_files[0]), tx_picture_name_compare);
}

static void tx_picture_native_size(sstv_mode_t mode,
                                   unsigned int *width,
                                   unsigned int *height)
{
    switch (mode) {
        case SSTV_MODE_ROBOT8_BW:
            *width = 160; *height = 120; break;
        case SSTV_MODE_ROBOT36:
        case SSTV_MODE_ROBOT72:
            *width = 320; *height = 240; break;
        case SSTV_MODE_MARTIN_M1:
        case SSTV_MODE_MARTIN_M2:
        case SSTV_MODE_SCOTTIE_S1:
        case SSTV_MODE_SCOTTIE_S2:
        default:
            *width = 320; *height = 256; break;
    }
}

static bool tx_picture_decode_png(const char *path,
                                  uint8_t **out_rgb,
                                  unsigned int *out_w,
                                  unsigned int *out_h)
{
    *out_rgb = NULL;
    *out_w = *out_h = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return false;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                  NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    png_uint_32 w = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 h = png_get_image_height(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);

    if (w == 0 || h == 0 || w > 8192 || h > 8192) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);
    int channels = png_get_channels(png_ptr, info_ptr);
    png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    if (channels != 3 && channels != 4) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    uint8_t *rgb = malloc((size_t)w * h * 3);
    uint8_t *row = malloc(rowbytes);
    if (!rgb || !row) {
        free(rgb); free(row);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    for (png_uint_32 y = 0; y < h; ++y) {
        png_read_row(png_ptr, row, NULL);
        for (png_uint_32 x = 0; x < w; ++x) {
            const uint8_t *p = row + (size_t)x * channels;
            uint8_t *d = rgb + ((size_t)y * w + x) * 3;
            if (channels == 4 && p[3] != 255) {
                unsigned int a = p[3];
                d[0] = (uint8_t)((p[0] * a + 255u * (255u - a)) / 255u);
                d[1] = (uint8_t)((p[1] * a + 255u * (255u - a)) / 255u);
                d[2] = (uint8_t)((p[2] * a + 255u * (255u - a)) / 255u);
            } else {
                d[0] = p[0]; d[1] = p[1]; d[2] = p[2];
            }
        }
    }

    png_read_end(png_ptr, NULL);
    free(row);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    *out_rgb = rgb;
    *out_w = (unsigned int)w;
    *out_h = (unsigned int)h;
    return true;
}

static void tx_picture_aspect_fit(const uint8_t *src,
                                  unsigned int sw, unsigned int sh,
                                  uint8_t *dst,
                                  unsigned int dw, unsigned int dh)
{
    memset(dst, 0, (size_t)dw * dh * 3);

    double scale_x = (double)dw / (double)sw;
    double scale_y = (double)dh / (double)sh;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    unsigned int rw = (unsigned int)floor(sw * scale + 0.5);
    unsigned int rh = (unsigned int)floor(sh * scale + 0.5);
    if (rw < 1) rw = 1;
    if (rh < 1) rh = 1;
    if (rw > dw) rw = dw;
    if (rh > dh) rh = dh;

    unsigned int x0 = (dw - rw) / 2;
    unsigned int y0 = (dh - rh) / 2;

    /* Bilinear resize.  This is run only after the 300 ms selection debounce,
     * never on every MFK detent. */
    for (unsigned int y = 0; y < rh; ++y) {
        double sy = ((double)y + 0.5) * sh / rh - 0.5;
        if (sy < 0.0) sy = 0.0;
        unsigned int y1 = (unsigned int)sy;
        unsigned int y2 = y1 + 1 < sh ? y1 + 1 : y1;
        double fy = sy - y1;

        for (unsigned int x = 0; x < rw; ++x) {
            double sx = ((double)x + 0.5) * sw / rw - 0.5;
            if (sx < 0.0) sx = 0.0;
            unsigned int x1 = (unsigned int)sx;
            unsigned int x2 = x1 + 1 < sw ? x1 + 1 : x1;
            double fx = sx - x1;

            for (unsigned int c = 0; c < 3; ++c) {
                double p11 = src[((size_t)y1 * sw + x1) * 3 + c];
                double p12 = src[((size_t)y1 * sw + x2) * 3 + c];
                double p21 = src[((size_t)y2 * sw + x1) * 3 + c];
                double p22 = src[((size_t)y2 * sw + x2) * 3 + c];
                double top = p11 + (p12 - p11) * fx;
                double bot = p21 + (p22 - p21) * fx;
                double v = top + (bot - top) * fy;
                dst[((size_t)(y0 + y) * dw + (x0 + x)) * 3 + c] =
                    (uint8_t)(v + 0.5);
            }
        }
    }
}

static bool tx_picture_load_and_prepare(const char *filename)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", SSTV_TX_PICS_DIR, filename);

    uint8_t *src = NULL;
    unsigned int sw = 0, sh = 0;
    if (!tx_picture_decode_png(path, &src, &sw, &sh)) {
        fprintf(stderr, "SSTV TX UI: cannot load picture %s\n", path);
        return false;
    }

    unsigned int dw, dh;
    tx_picture_native_size(cont_mode, &dw, &dh);
    tx_picture_aspect_fit(src, sw, sh, tx_picture_rgb, dw, dh);
    free(src);

    tx_picture_width = dw;
    tx_picture_height = dh;
    tx_build_final_frame(false);
    snprintf(tx_picture_name, sizeof(tx_picture_name), "%s", filename);
    fprintf(stderr, "SSTV TX UI: prepared %s (%ux%u -> %ux%u, %s)\n",
            filename, sw, sh, dw, dh, cont_mode_name(cont_mode));
    return true;
}


static void tx_instruction_hide(void)
{
    if (tx_instruction_label) {
        lv_obj_del(tx_instruction_label);
        tx_instruction_label = NULL;
    }
    if (frame.status) lv_label_set_text(frame.status, SSTV_STATUS_TEXT);
}

/* The TX menus look like the Modes menu: a dark box with a title, the list
 * is a table laid over it (tx_list_create), the hint is in the status line. */
static void tx_instruction_show(const char *title, const char *hint)
{
    tx_instruction_hide();

    lv_obj_t *box = lv_obj_create(dialog.obj);
    tx_instruction_label = box;
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, SSTV_TX_LIST_X, SSTV_TX_LIST_Y);
    lv_obj_set_size(box, SSTV_TX_LIST_W, SSTV_TX_LIST_H);
    lv_obj_add_style(box, &popup_style, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *head = lv_label_create(box);
    lv_obj_set_style_text_font(head, &sony_20, 0);
    lv_obj_set_style_text_color(head, lv_color_hex(APP_FRAME_HEAD), 0);
    lv_label_set_text(head, title);
    lv_obj_set_pos(head, 20, 7);

    if (frame.status) lv_label_set_text(frame.status, hint);
}

static void tx_list_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *d = lv_event_get_draw_part_dsc(e);
    if (!d || d->part != LV_PART_ITEMS || !d->rect_dsc) return;

    uint16_t row = LV_TABLE_CELL_NONE, col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(lv_event_get_target(e), &row, &col);
    bool on = d->id == row;

    d->rect_dsc->bg_color = TC(sel_bg);
    d->rect_dsc->bg_opa = on ? LV_OPA_COVER : LV_OPA_TRANSP;
    d->rect_dsc->border_color = TC(sel_border);
    d->rect_dsc->border_opa = LV_OPA_COVER;
    d->rect_dsc->border_width = on ? 2 : 0;
    d->rect_dsc->border_side = LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM;
    d->rect_dsc->outline_width = 0;
    if (on && d->label_dsc) d->label_dsc->color = TC(sel_text);
}

/* A list in the box of tx_instruction_show(): the MFK chooses, a press confirms */
static lv_obj_t *tx_list_create(uint16_t rows)
{
    lv_obj_t *t = lv_table_create(dialog.obj);
    lv_obj_remove_style(t, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_set_pos(t, SSTV_TX_LIST_X + 2, SSTV_TX_LIST_Y + SSTV_TX_HEAD_H);
    lv_obj_set_size(t, SSTV_TX_LIST_W - 4, SSTV_TX_LIST_H - SSTV_TX_HEAD_H - 8);
    lv_obj_set_style_border_width(t, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(t, &sony_22, LV_PART_ITEMS);
    lv_obj_set_style_text_color(t, TC(text), LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(t, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(t, 18, LV_PART_ITEMS);
    lv_table_set_col_cnt(t, 1);
    lv_table_set_col_width(t, 0, SSTV_TX_LIST_W - 4);
    lv_table_set_row_cnt(t, rows);
    lv_obj_add_event_cb(t, key_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(t, tx_list_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
    return t;
}

/* Compact 5x7 bitmap font for the pixels that are actually transmitted.
 * This deliberately does not depend on LVGL rendering: tx_final_rgb is the
 * single WYSIWYG framebuffer used by both preview and, later, the encoder. */
static const uint8_t *tx_glyph5x7(char ch)
{
    static const uint8_t blank[5]={0,0,0,0,0};
    static const uint8_t glyphs[][5] = {
        {0x7E,0x11,0x11,0x11,0x7E}, /* A */
        {0x7F,0x49,0x49,0x49,0x36}, /* B */
        {0x3E,0x41,0x41,0x41,0x22}, /* C */
        {0x7F,0x41,0x41,0x22,0x1C}, /* D */
        {0x7F,0x49,0x49,0x49,0x41}, /* E */
        {0x7F,0x09,0x09,0x09,0x01}, /* F */
        {0x3E,0x41,0x49,0x49,0x7A}, /* G */
        {0x7F,0x08,0x08,0x08,0x7F}, /* H */
        {0x00,0x41,0x7F,0x41,0x00}, /* I */
        {0x20,0x40,0x41,0x3F,0x01}, /* J */
        {0x7F,0x08,0x14,0x22,0x41}, /* K */
        {0x7F,0x40,0x40,0x40,0x40}, /* L */
        {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
        {0x7F,0x04,0x08,0x10,0x7F}, /* N */
        {0x3E,0x41,0x41,0x41,0x3E}, /* O */
        {0x7F,0x09,0x09,0x09,0x06}, /* P */
        {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
        {0x7F,0x09,0x19,0x29,0x46}, /* R */
        {0x46,0x49,0x49,0x49,0x31}, /* S */
        {0x01,0x01,0x7F,0x01,0x01}, /* T */
        {0x3F,0x40,0x40,0x40,0x3F}, /* U */
        {0x1F,0x20,0x40,0x20,0x1F}, /* V */
        {0x3F,0x40,0x38,0x40,0x3F}, /* W */
        {0x63,0x14,0x08,0x14,0x63}, /* X */
        {0x07,0x08,0x70,0x08,0x07}, /* Y */
        {0x61,0x51,0x49,0x45,0x43}, /* Z */
        {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
        {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
        {0x42,0x61,0x51,0x49,0x46}, /* 2 */
        {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
        {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
        {0x27,0x45,0x45,0x45,0x39}, /* 5 */
        {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
        {0x01,0x71,0x09,0x05,0x03}, /* 7 */
        {0x36,0x49,0x49,0x49,0x36}, /* 8 */
        {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
        {0x20,0x10,0x08,0x04,0x02}  /* / */
    };
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch >= 'A' && ch <= 'Z') return glyphs[ch-'A'];
    if (ch >= '0' && ch <= '9') return glyphs[26 + ch-'0'];
    if (ch == '/') return glyphs[36];
    return blank;
}

static void tx_rgb_pixel(uint8_t *rgb, unsigned int w, unsigned int h,
                         int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || x >= (int)w || y >= (int)h) return;
    size_t o=((size_t)y*w+(unsigned int)x)*3;
    rgb[o]=r; rgb[o+1]=g; rgb[o+2]=b;
}

static int tx_text_width(const char *text, int scale)
{
    size_t n = text ? strlen(text) : 0;
    return n ? (int)(n * 6 - 1) * scale : 0;
}

typedef enum {
    TX_TEXT_ALIGN_LEFT = 0,
    TX_TEXT_ALIGN_CENTER,
    TX_TEXT_ALIGN_RIGHT
} tx_text_align_t;


typedef struct { uint8_t r,g,b; } tx_rgb_t;
typedef struct { tx_rgb_t top,middle,bottom; } tx_color_combo_t;

#define TX_RGB(R,G,B) { (R),(G),(B) }

/* Strong, deliberately saturated choices for readability over photographs.
 * #0 and #1 are the only monochromatic sets. Pink occurs only as BOTTOM. */
static const tx_color_combo_t tx_color_combos[20] = {
    { TX_RGB(255,  0,  0), TX_RGB(255,  0,  0), TX_RGB(255,  0,  0) }, /* red */
    { TX_RGB(255,255,255), TX_RGB(255,255,255), TX_RGB(255,255,255) }, /* white */
    { TX_RGB(255,255,  0), TX_RGB(  0,255,  0), TX_RGB( 64,128,255) }, /* yellow green blue */
    { TX_RGB(255,128,  0), TX_RGB(192, 64,255), TX_RGB( 64,192,255) }, /* orange violet light blue */
    { TX_RGB(255,  0,  0), TX_RGB(255,255,  0), TX_RGB(  0,255,  0) }, /* red yellow green */
    { TX_RGB( 64,128,255), TX_RGB(255,128,  0), TX_RGB(255,255,255) }, /* blue orange white */
    { TX_RGB(192, 64,255), TX_RGB(  0,255,  0), TX_RGB(255,255,  0) }, /* violet green yellow */
    { TX_RGB(  0,255,  0), TX_RGB( 64,192,255), TX_RGB(255,128,  0) }, /* green light blue orange */
    { TX_RGB(255,255,  0), TX_RGB(255,  0,  0), TX_RGB(192, 64,255) }, /* yellow red violet */
    { TX_RGB( 64,192,255), TX_RGB(255,255,  0), TX_RGB(255, 64,160) }, /* light blue yellow pink */

    /* Darker palette: still saturated enough to remain useful for SSTV text. */
    { TX_RGB(  0,112, 32), TX_RGB(  0, 64,160), TX_RGB(160,  0,  0) }, /* dark green, blue, red */
    { TX_RGB(128, 64,  0), TX_RGB( 96,  0,128), TX_RGB(  0,112,112) }, /* brown, dark violet, teal */
    { TX_RGB(  0, 80,144), TX_RGB(144, 64,  0), TX_RGB(112,  0, 80) }, /* deep blue, burnt orange, wine */
    { TX_RGB( 72,104,  0), TX_RGB(  0, 72,112), TX_RGB(128, 32, 32) }, /* olive, petrol blue, brick */
    { TX_RGB(112,  0,128), TX_RGB(  0,104, 64), TX_RGB(160, 80,  0) }, /* violet, emerald, amber */
    { TX_RGB(  0, 88,128), TX_RGB(128,  0, 64), TX_RGB( 64,112,  0) }, /* ocean, burgundy, leaf */
    { TX_RGB(144, 48,  0), TX_RGB( 48, 64,144), TX_RGB(  0,120, 96) }, /* rust, indigo, sea green */
    { TX_RGB( 80,  0,128), TX_RGB(144, 80,  0), TX_RGB(  0, 96,128) }, /* purple, ochre, dark cyan-blue */
    { TX_RGB(  0,128, 64), TX_RGB(128,  0, 32), TX_RGB( 64, 64,160) }, /* forest, crimson, royal blue */
    { TX_RGB(128, 48, 96), TX_RGB( 32,112, 80), TX_RGB(144, 96,  0) }  /* plum, jade, dark gold */
};

static tx_rgb_t tx_text_top    = TX_RGB(255,0,0);
static tx_rgb_t tx_text_middle = TX_RGB(255,0,0);
static tx_rgb_t tx_text_bottom = TX_RGB(255,0,0);

static void tx_draw_text_aligned_color(uint8_t *rgb, unsigned int w, unsigned int h,
                                       int y, const char *text, int scale,
                                       tx_text_align_t align, tx_rgb_t color)
{
    if (!text || !*text) return;
    int tw=tx_text_width(text,scale);
    while (scale>1 && tw>(int)w-8) {
        scale--;
        tw=tx_text_width(text,scale);
    }

    const int margin=15;
    int x0;
    if (align==TX_TEXT_ALIGN_LEFT)
        x0=margin;
    else if (align==TX_TEXT_ALIGN_RIGHT)
        x0=(int)w-tw-margin;
    else
        x0=((int)w-tw)/2;

    if (x0<0) x0=0;

    for (int pass=0;pass<2;++pass) {
        for (size_t ci=0;text[ci];++ci) {
            const uint8_t *g=tx_glyph5x7(text[ci]);
            int gx=x0+(int)ci*6*scale;
            for (int x=0;x<5;x++) for (int yy=0;yy<7;yy++) {
                if (!(g[x]&(1u<<yy))) continue;
                for (int sy=0;sy<scale;sy++) for (int sx=0;sx<scale;sx++) {
                    int px=gx+x*scale+sx,py=y+yy*scale+sy;
                    if (pass==0) {
                        for (int oy=-4;oy<=4;oy++)
                            for (int ox=-4;ox<=4;ox++)
                                if (ox*ox+oy*oy<=17)
                                    tx_rgb_pixel(rgb,w,h,px+ox,py+oy,0,0,0);
                    } else {
                        tx_rgb_pixel(rgb,w,h,px,py,color.r,color.g,color.b);
                        tx_rgb_pixel(rgb,w,h,px+1,py,color.r,color.g,color.b);
                    }
                }
            }
        }
    }
}

/* Middle template text: slightly smaller than top/bottom and automatically
 * wrapped at a word boundary to at most two centered lines. */
static void tx_draw_middle_wrapped(uint8_t *rgb, unsigned int w, unsigned int h,
                                   const char *text)
{
    if (!text || !*text) return;
    int scale=(w<=160)?2:3;
    int max_chars=((int)w-12)/(6*scale);
    if (max_chars < 4) max_chars=4;

    char line1[256]={0}, line2[256]={0};
    size_t len=strlen(text);
    if ((int)len <= max_chars) {
        snprintf(line1,sizeof(line1),"%s",text);
    } else {
        int cut=max_chars;
        while (cut>0 && text[cut]!=' ') cut--;
        if (cut==0) cut=max_chars;
        snprintf(line1,sizeof(line1),"%.*s",cut,text);
        const char *p=text+cut;
        while (*p==' ') p++;
        snprintf(line2,sizeof(line2),"%.*s",max_chars,p);
    }

    int text_h=7*scale;
    int gap=scale+4;
    int total=line2[0] ? text_h*2+gap : text_h;
    int y=((int)h-total)/2;
    if (tx_message_type == SSTV_TX_MESSAGE_REPLAY)
        y -= (int)h * 34 / 256;
    if (y < 5) y = 5;
    tx_draw_text_aligned_color(rgb,w,h,y,line1,scale,
                               TX_TEXT_ALIGN_CENTER,tx_text_middle);
    if (line2[0])
        tx_draw_text_aligned_color(rgb,w,h,y+text_h+gap,line2,scale,
                                   TX_TEXT_ALIGN_CENTER,tx_text_middle);
}

static void tx_save_rx_canvas(void)
{
    memcpy(tx_saved_rx_canvas, sstv_buffer, sizeof(tx_saved_rx_canvas));
    tx_saved_rx_canvas_valid = true;
}

static void tx_restore_rx_canvas(void)
{
    if (!tx_saved_rx_canvas_valid || !sstv_canvas) return;
    memcpy(sstv_buffer, tx_saved_rx_canvas, sizeof(tx_saved_rx_canvas));
    lv_obj_invalidate(sstv_canvas);
}

static void tx_rebuild_current_overlay(void)
{
    if (!tx_picture_width || !tx_picture_height) return;

    switch (tx_phase) {
        case SSTV_TX_PHASE_SELECT_PICTURE:
            memcpy(tx_final_rgb,tx_picture_rgb,
                   (size_t)tx_picture_width*tx_picture_height*3);
            break;
        case SSTV_TX_PHASE_SELECT_TYPE:
            /* Initial action phase: never replace the received image. */
            tx_restore_rx_canvas();
            return;
        case SSTV_TX_PHASE_SELECT_TEMPLATE:
            tx_template_render();
            break;
        case SSTV_TX_PHASE_SELECT_COLORS:
            if (tx_message_type == SSTV_TX_MESSAGE_CQ) tx_build_final_frame(true);
            else tx_template_render();
            break;
        default:
            memcpy(tx_final_rgb,tx_picture_rgb,
                   (size_t)tx_picture_width*tx_picture_height*3);
            break;
    }
    tx_picture_render_preview();
}

static void tx_build_final_frame(bool cq_overlay)
{
    if (!tx_picture_width || !tx_picture_height) return;
    size_t bytes=(size_t)tx_picture_width*tx_picture_height*3;
    memcpy(tx_final_rgb, tx_picture_rgb, bytes);

    if (cq_overlay) {
        int scale=(tx_picture_width<=160)?2:3;
        int text_h=7*scale;
        tx_draw_text_aligned_color(tx_final_rgb,tx_picture_width,tx_picture_height,
                                   5,"CQ CQ SSTV",scale,TX_TEXT_ALIGN_LEFT,
                                   tx_text_top);
        /* Middle line intentionally empty. */
        tx_draw_text_aligned_color(tx_final_rgb,tx_picture_width,tx_picture_height,
                                   (int)tx_picture_height-text_h-5,params.callsign.x,
                                   scale,TX_TEXT_ALIGN_RIGHT,tx_text_bottom);
    }
}

static void tx_picture_render_preview(void)
{
    if (!sstv_canvas || tx_picture_width == 0 || tx_picture_height == 0)
        return;

    lv_canvas_fill_bg(sstv_canvas, lv_color_black(), LV_OPA_COVER);

    int out_w = SSTV_IMAGE_W;
    int out_h = (int)((uint64_t)tx_picture_height * out_w / tx_picture_width);
    if (out_h > SSTV_IMAGE_H) {
        out_h = SSTV_IMAGE_H;
        out_w = (int)((uint64_t)tx_picture_width * out_h / tx_picture_height);
    }
    int x0 = (SSTV_IMAGE_W - out_w) / 2;
    int y0 = (SSTV_IMAGE_H - out_h) / 2;

    for (int y = 0; y < out_h; ++y) {
        unsigned int sy = (unsigned int)((uint64_t)y * tx_picture_height / out_h);
        for (int x = 0; x < out_w; ++x) {
            unsigned int sx = (unsigned int)((uint64_t)x * tx_picture_width / out_w);
            size_t o = ((size_t)sy * tx_picture_width + sx) * 3;
            lv_canvas_set_px(sstv_canvas, x0 + x, y0 + y,
                             lv_color_make(tx_final_rgb[o],
                                           tx_final_rgb[o + 1],
                                           tx_final_rgb[o + 2]));
        }
    }
    lv_obj_invalidate(sstv_canvas);
}

static void tx_picture_schedule_preview(void)
{
    if (!tx_picture_table || tx_picture_file_count == 0)
        return;

    int16_t row = LV_TABLE_CELL_NONE, col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_picture_table, &row, &col);
    if (row == LV_TABLE_CELL_NONE || row < 0 ||
        (size_t)row >= tx_picture_file_count)
        return;

    tx_picture_pending = row;
    tx_picture_pending_since_ms = tx_now_ms();
}

static void tx_picture_selection_changed_cb(lv_event_t *e)
{
    (void)e;
    if (tx_ui_state == SSTV_TX_UI_SETUP &&
        tx_phase == SSTV_TX_PHASE_SELECT_PICTURE)
        tx_picture_schedule_preview();
}

/* IMAGE ONLY: the prepared picture goes out as it is (TX setup, or the web remote) */
static void tx_image_only_start(void)
{
    size_t bytes=(size_t)tx_picture_width*tx_picture_height*3;
    memcpy(tx_final_rgb,tx_picture_rgb,bytes);
    tx_picture_render_preview();
    tx_picture_selector_destroy();
    tx_instruction_hide();
    lv_group_add_obj(keyboard_group,sstv_canvas);
    lv_group_focus_obj(sstv_canvas);
    lv_group_set_editing(keyboard_group,true);
    if (status_label) lv_obj_clear_flag(status_label,LV_OBJ_FLAG_HIDDEN);
    fprintf(stderr,"SSTV TX UI: IMAGE ONLY picture confirmed -> RF\n");
    tx_start_final_rf();
}

static void tx_picture_pressed_cb(lv_event_t *e)
{
    (void)e;
    if (tx_ui_state != SSTV_TX_UI_SETUP ||
        tx_phase != SSTV_TX_PHASE_SELECT_PICTURE ||
        tx_picture_file_count == 0)
        return;

    int16_t row = LV_TABLE_CELL_NONE, col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_picture_table, &row, &col);
    if (row == LV_TABLE_CELL_NONE || row < 0 ||
        (size_t)row >= tx_picture_file_count)
        return;

    /* A quick rotate+press must confirm the picture currently highlighted,
     * even if the 300 ms preview debounce has not expired yet. */
    tx_picture_pending = row;
    if (tx_picture_selected != row) {
        if (!tx_picture_load_and_prepare(tx_picture_files[row]))
            return;
        tx_picture_selected = row;
        tx_picture_render_preview();
    }

    snprintf(tx_last_picture_file,sizeof(tx_last_picture_file),"%s",
             tx_picture_files[row]);

    lv_group_remove_obj(tx_picture_table);
    lv_obj_add_flag(tx_picture_table, LV_OBJ_FLAG_HIDDEN);

    fprintf(stderr, "SSTV TX UI: picture confirmed: %s\n", tx_picture_name);
    if (tx_message_type == SSTV_TX_MESSAGE_IMAGE_ONLY) {
        tx_image_only_start();
        return;
    }
    if (tx_message_type == SSTV_TX_MESSAGE_CQ) {
        tx_build_final_frame(true);
        tx_picture_render_preview();
        tx_phase = SSTV_TX_PHASE_SELECT_COLORS;
        tx_color_selector_build();
        fprintf(stderr, "SSTV TX UI: CQ picture confirmed -> colors\n");
    } else {
        tx_phase = SSTV_TX_PHASE_SELECT_TEMPLATE;
        tx_template_selector_build();
        fprintf(stderr, "SSTV TX UI: REPLY picture confirmed -> templates\n");
    }
}

static void tx_picture_load_pending_preview(void)
{
    if (tx_picture_pending < 0 ||
        tx_picture_pending == tx_picture_selected ||
        (size_t)tx_picture_pending >= tx_picture_file_count)
        return;

    if (tx_now_ms() - tx_picture_pending_since_ms <
        SSTV_TX_PICTURE_DEBOUNCE_MS)
        return;

    int row = tx_picture_pending;
    if (tx_picture_load_and_prepare(tx_picture_files[row])) {
        tx_picture_selected = row;
        tx_picture_render_preview();
    }
}


static void tx_type_selector_destroy(void)
{
    if (tx_type_table) {
        lv_group_remove_obj(tx_type_table);
        lv_obj_del(tx_type_table);
        tx_type_table = NULL;
    }
}


static void tx_template_free_files(void)
{
    for (size_t i=0;i<tx_template_file_count;i++) free(tx_template_files[i]);
    free(tx_template_files);
    tx_template_files=NULL;
    tx_template_file_count=0;
    tx_template_file_capacity=0;
}

static int tx_template_name_compare(const void *a,const void *b)
{
    return strcasecmp(*(const char * const *)a,*(const char * const *)b);
}

static void tx_template_refresh(void)
{
    tx_template_free_files();
    DIR *dir=opendir(SSTV_TX_TEXT_DIR);
    if (!dir) return;
    struct dirent *entry;
    while ((entry=readdir(dir))!=NULL) {
        const char *name=entry->d_name;
        size_t len=strlen(name);
        if (name[0]=='.' || len<9 || strcmp(name+len-8,".sstvtxt")!=0) continue;
        if (tx_template_file_count==tx_template_file_capacity) {
            size_t nc=tx_template_file_capacity?tx_template_file_capacity*2:16;
            char **p=realloc(tx_template_files,nc*sizeof(*p));
            if (!p) break;
            tx_template_files=p; tx_template_file_capacity=nc;
        }
        char *copy=strdup(name);
        if (copy) tx_template_files[tx_template_file_count++]=copy;
    }
    closedir(dir);
    qsort(tx_template_files,tx_template_file_count,sizeof(tx_template_files[0]),
          tx_template_name_compare);
}

static bool tx_json_string(const char *json,const char *key,char *out,size_t outsz)
{
    out[0]='\0';
    char pat[64];
    snprintf(pat,sizeof(pat),"\"%s\"",key);
    const char *p=strstr(json,pat);
    if (!p) return true; /* missing is allowed */
    p=strchr(p+strlen(pat),':');
    if (!p) return false;
    p++;
    while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++;
    if (!strncmp(p,"null",4)) return true;
    if (*p!='"') return false;
    p++;
    size_t n=0;
    while (*p && *p!='"') {
        char c=*p++;
        if (c=='\\' && *p) {
            char e=*p++;
            if (e=='n') c=' ';
            else if (e=='t') c=' ';
            else if (e=='"'||e=='\\'||e=='/') c=e;
            else c=e;
        }
        if (n+1<outsz) out[n++]=c;
    }
    if (*p!='"') return false;
    out[n]='\0';
    return true;
}

static bool tx_template_load(const char *filename)
{
    char path[512];
    snprintf(path,sizeof(path),"%s/%s",SSTV_TX_TEXT_DIR,filename);
    FILE *fp=fopen(path,"rb");
    if (!fp) return false;
    if (fseek(fp,0,SEEK_END)!=0) { fclose(fp); return false; }
    long sz=ftell(fp);
    if (sz<0 || sz>16384) { fclose(fp); return false; }
    rewind(fp);
    char *json=malloc((size_t)sz+1);
    if (!json) { fclose(fp); return false; }
    size_t got=fread(json,1,(size_t)sz,fp);
    fclose(fp);
    json[got]='\0';
    bool ok=tx_json_string(json,"middle",tx_template_middle,sizeof(tx_template_middle)) &&
            tx_json_string(json,"bottom",tx_template_bottom,sizeof(tx_template_bottom));
    free(json);
    return ok;
}

static void tx_replay_draw_rx_thumbnail(uint8_t *rgb,
                                        unsigned int w, unsigned int h)
{
    if (!rgb || !w || !h || !tx_saved_rx_canvas_valid) return;

    /* About 96x72 on a 320x256 TX frame.  Scale with the selected picture
     * and keep the saved RX canvas aspect ratio. */
    int thumb_w=(int)w*96/320;
    if (thumb_w < 48) thumb_w=48;
    int thumb_h=(int)((int64_t)thumb_w*SSTV_CANVAS_H/SSTV_CANVAS_W);

    /* Leave room for the bottom line. */
    int bottom_reserved=(w<=160)?24:38;
    /* Place the replay thumbnail 15 px from the left edge on a 320 px frame.
     * Move it 3 px upward to make room for the 3 px black border below. */
    int x0=(int)w*15/320;
    int y0=(int)h*135/256 - 3;
    if (y0+thumb_h+3 > (int)h-bottom_reserved)
        y0=(int)h-bottom_reserved-thumb_h-3;
    if (y0<3) y0=3;

    for (int y=0;y<thumb_h;y++) {
        int sy=(int)((int64_t)y*SSTV_CANVAS_H/thumb_h);
        for (int x=0;x<thumb_w;x++) {
            int sx=(int)((int64_t)x*SSTV_CANVAS_W/thumb_w);
            uint32_t c=lv_color_to32(
                tx_saved_rx_canvas[(size_t)sy*SSTV_CANVAS_W+sx]);
            tx_rgb_pixel(rgb,w,h,x0+x,y0+y,
                         (uint8_t)((c>>16)&0xff),
                         (uint8_t)((c>>8)&0xff),
                         (uint8_t)(c&0xff));
        }
    }

    /* Solid 3 px black border around the replay thumbnail. */
    for (int b=1;b<=3;b++) {
        for (int x=-b;x<thumb_w+b;x++) {
            tx_rgb_pixel(rgb,w,h,x0+x,y0-b,0,0,0);
            tx_rgb_pixel(rgb,w,h,x0+x,y0+thumb_h-1+b,0,0,0);
        }
        for (int y=-b;y<thumb_h+b;y++) {
            tx_rgb_pixel(rgb,w,h,x0-b,y0+y,0,0,0);
            tx_rgb_pixel(rgb,w,h,x0+thumb_w-1+b,y0+y,0,0,0);
        }
    }
}

static void tx_template_render(void)
{
    if (!tx_picture_width || !tx_picture_height) return;
    memcpy(tx_final_rgb,tx_picture_rgb,
           (size_t)tx_picture_width*tx_picture_height*3);

    int outer_scale=(tx_picture_width<=160)?2:3;
    tx_draw_text_aligned_color(tx_final_rgb,tx_picture_width,tx_picture_height,5,
                               tx_target_callsign[0] ? tx_target_callsign : "<TARGET CALLSIGN>",
                               outer_scale,TX_TEXT_ALIGN_LEFT,tx_text_top);

    tx_draw_middle_wrapped(tx_final_rgb,tx_picture_width,tx_picture_height,
                           tx_template_middle);

    if (tx_message_type == SSTV_TX_MESSAGE_REPLAY)
        tx_replay_draw_rx_thumbnail(tx_final_rgb,tx_picture_width,tx_picture_height);

    char bottom[256];
    if (tx_template_bottom[0])
        snprintf(bottom,sizeof(bottom),"%s%s",tx_template_bottom,params.callsign.x);
    else
        snprintf(bottom,sizeof(bottom),"%s",params.callsign.x);
    int scale=(tx_picture_width<=160)?2:3;
    tx_draw_text_aligned_color(tx_final_rgb,tx_picture_width,tx_picture_height,
                               (int)tx_picture_height-7*scale-5,bottom,
                               scale,TX_TEXT_ALIGN_RIGHT,tx_text_bottom);
}

static void tx_template_render_selected(void)
{
    if (!tx_template_table || !tx_template_file_count) return;
    int16_t row=LV_TABLE_CELL_NONE,col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_template_table,&row,&col);
    if (row==LV_TABLE_CELL_NONE || row<0 || (size_t)row>=tx_template_file_count) return;
    if (!tx_template_load(tx_template_files[row])) {
        fprintf(stderr,"SSTV TX UI: invalid template %s\n",tx_template_files[row]);
        return;
    }
    tx_template_render();
    tx_picture_render_preview();
}

static void tx_template_selection_changed_cb(lv_event_t *e)
{
    (void)e;
    if (tx_ui_state==SSTV_TX_UI_SETUP &&
        tx_phase==SSTV_TX_PHASE_SELECT_TEMPLATE)
        tx_template_preview_pending=true;
}

static void tx_template_pressed_cb(lv_event_t *e)
{
    (void)e;
    if (tx_ui_state!=SSTV_TX_UI_SETUP ||
        tx_phase!=SSTV_TX_PHASE_SELECT_TEMPLATE || !tx_template_file_count) return;
    tx_template_preview_pending=false;
    tx_template_render_selected();

    int16_t row=LV_TABLE_CELL_NONE,col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_template_table,&row,&col);
    if (row==LV_TABLE_CELL_NONE || row<0 || (size_t)row>=tx_template_file_count) return;
    char shown[256];
    snprintf(shown,sizeof(shown),"%s",tx_template_files[row]);
    size_t n=strlen(shown); if(n>=8) shown[n-8]='\0';

    fprintf(stderr,"SSTV TX UI: template confirmed: %s\n",shown);
    tx_template_selector_destroy();
    /* Preserve REPLAY; normal non-CQ traffic remains REPLY. */
    if (tx_message_type != SSTV_TX_MESSAGE_REPLAY)
        tx_message_type=SSTV_TX_MESSAGE_REPLY;
    tx_phase=SSTV_TX_PHASE_SELECT_COLORS;
    tx_color_selector_build();
}

static void tx_template_selector_destroy(void)
{
    tx_template_preview_pending=false;
    if (tx_template_table) {
        lv_group_remove_obj(tx_template_table);
        lv_obj_del(tx_template_table);
        tx_template_table=NULL;
    }
    tx_template_free_files();
}

static void tx_template_selector_build(void)
{
    tx_template_selector_destroy();
    tx_template_refresh();
    if(status_label) lv_obj_add_flag(status_label,LV_OBJ_FLAG_HIDDEN);
    tx_instruction_show("TX TEXT", SSTV_TX_HINT);

    tx_template_table=tx_list_create(1);

    if (!tx_template_file_count) {
        lv_table_set_cell_value(tx_template_table,0,0,"No TX templates found");
        lv_group_add_obj(keyboard_group,sstv_canvas);
        lv_group_focus_obj(sstv_canvas);
        lv_group_set_editing(keyboard_group,true);
        return;
    }

    lv_table_set_row_cnt(tx_template_table,(uint16_t)tx_template_file_count);
    for(size_t i=0;i<tx_template_file_count;i++) {
        char shown[256];
        snprintf(shown,sizeof(shown),"%s",tx_template_files[i]);
        size_t n=strlen(shown); if(n>=8) shown[n-8]='\0';
        lv_table_set_cell_value(tx_template_table,(uint16_t)i,0,shown);
    }
    lv_obj_add_event_cb(tx_template_table,tx_template_selection_changed_cb,
                        LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(tx_template_table,tx_template_pressed_cb,
                        LV_EVENT_PRESSED,NULL);
    lv_group_remove_obj(sstv_canvas);
    lv_group_add_obj(keyboard_group,tx_template_table);
    lv_group_focus_obj(tx_template_table);
    lv_group_set_editing(keyboard_group,true);
    tx_template_preview_pending=true;
}


static bool tx_target_keyboard_cancel_cb(void)
{
    textarea_window_close();
    tx_type_selector_build();
    return true;
}

static bool tx_target_keyboard_ok_cb(void)
{
    const char *value=textarea_window_get();
    if (value) {
        while (*value==' ' || *value=='\t') value++;
        snprintf(tx_target_callsign,sizeof(tx_target_callsign),"%s",value);
        size_t n=strlen(tx_target_callsign);
        while (n && (tx_target_callsign[n-1]==' ' ||
                     tx_target_callsign[n-1]=='\t')) tx_target_callsign[--n]='\0';
        for (size_t i=0;tx_target_callsign[i];i++) {
            if (tx_target_callsign[i]>='a' && tx_target_callsign[i]<='z')
                tx_target_callsign[i]-=('a'-'A');
        }
    }
    textarea_window_close();
    tx_type_selector_build();
    return true;
}

static lv_obj_t *tx_find_keyboard_recursive(lv_obj_t *parent)
{
    if (!parent) return NULL;
    if (lv_obj_check_type(parent,&lv_keyboard_class)) return parent;

    uint32_t count=lv_obj_get_child_cnt(parent);
    for (uint32_t i=0;i<count;i++) {
        lv_obj_t *found=tx_find_keyboard_recursive(lv_obj_get_child(parent,i));
        if (found) return found;
    }
    return NULL;
}

static void tx_target_keyboard_force_lower(void)
{
    if (!tx_target_keyboard_obj)
        tx_target_keyboard_obj=tx_find_keyboard_recursive(lv_scr_act());

    if (tx_target_keyboard_obj &&
        lv_obj_is_valid(tx_target_keyboard_obj)) {
        lv_keyboard_set_mode(tx_target_keyboard_obj,
                             LV_KEYBOARD_MODE_TEXT_LOWER);
    } else {
        tx_target_keyboard_obj=NULL;
    }
}

static void tx_target_keyboard_open_now(void)
{
    /* Same opening path used by the stock CALLSIGN dialog.
     * textarea_window_open() itself selects LV_KEYBOARD_MODE_TEXT_UPPER. */
    textarea_window_open(tx_target_keyboard_ok_cb,tx_target_keyboard_cancel_cb);

    /* Keep the actual LVGL keyboard object.  Some front-panel events arrive
     * after the window is created and can change its page, so Phase 4 keeps
     * forcing TEXT_LOWER for the first 500 ms after opening. */
    tx_target_keyboard_obj=tx_find_keyboard_recursive(lv_scr_act());
    tx_target_keyboard_force_until=lv_tick_get()+500;
    tx_target_keyboard_force_lower();

    lv_obj_t *text=textarea_window_text();
    lv_textarea_set_accepted_chars(text,
        "0123456789/"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz");
    lv_textarea_set_max_length(text,sizeof(tx_target_callsign)-1);
    lv_textarea_set_placeholder_text(text,"Target callsign");
    textarea_window_set(tx_target_callsign);
}



static void tx_color_apply(int row)
{
    if (row<0 || row>=20) row=0;
    tx_color_selected=row;
    tx_last_color_selected=row;
    tx_text_top=tx_color_combos[row].top;
    tx_text_middle=tx_color_combos[row].middle;
    tx_text_bottom=tx_color_combos[row].bottom;
}

static void tx_color_render_selected(void)
{
    if (!tx_color_table) return;
    int16_t row=LV_TABLE_CELL_NONE,col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_color_table,&row,&col);
    if (row==LV_TABLE_CELL_NONE || row<0 || row>=20) return;
    tx_color_apply(row);
    if (tx_message_type == SSTV_TX_MESSAGE_CQ) tx_build_final_frame(true);
    else tx_template_render();
    tx_picture_render_preview();
}

static void tx_color_selection_changed_cb(lv_event_t *e)
{
    (void)e;
    if (tx_ui_state==SSTV_TX_UI_SETUP &&
        tx_phase==SSTV_TX_PHASE_SELECT_COLORS) {
        tx_color_preview_pending=true;
        tx_color_preview_since_ms=lv_tick_get();
    }
}


static void tx_return_to_auto_rx(void)
{
    tx_ui_state=SSTV_TX_UI_RX;
    tx_phase=SSTV_TX_PHASE_NONE;
    btn_tx.label="TX";
    snprintf(tx_mode_label,sizeof(tx_mode_label),"RX Mode\n%s",cont_rx_mode_name());
    btn_cont_mode.label=tx_mode_label;

    pthread_mutex_lock(&live_mutex);
    live_enabled=true;
    live_capturing=false;
    live_decode_pending=false;
    live_ring_pos=live_ring_count=0;
    live_since_check=0;
    live_wait_samples=live_wait_log_mark=0;
    free(live_samples);
    live_samples=NULL;
    live_written=live_target=0;
    pthread_mutex_unlock(&live_mutex);

    set_receiver_ui_listening();
    tx_restore_rx_canvas();
    if (btn_tx.disp_btn) buttons_refresh(&btn_tx);
    if (btn_cont_mode.disp_btn) buttons_refresh(&btn_cont_mode);
    update_rx_controls();
}

static bool tx_start_final_rf_mode(sstv_mode_t mode)
{
    if (!tx_picture_width || !tx_picture_height) {
        set_status("SSTV TX: no final image.");
        return false;
    }

    /* The encoder consumes this exact composed framebuffer.  It is already at
     * the selected mode's native geometry, so the encoder's PNG import does
     * not change the WYSIWYG composition. */
    if (!save_tx_png(SSTV_TX_PNG,tx_final_rgb,
                     tx_picture_width,tx_picture_height)) {
        set_status("SSTV TX: cannot prepare image.");
        return false;
    }

    if (tx_thread_valid) {
        pthread_join(tx_thread,NULL);
        tx_thread_valid=false;
    }

    /* Snapshot the requested mode now.  Normal TX passes cont_mode;
     * REPEAT LAST TX passes the mode stored with the successful backup. */
    tx_mode=mode;
    tx_abort=false;
    tx_started=false;
    tx_key_time_valid=false;
    tx_progress=0;
    tx_result=0;
    tx_running=true;

    tx_ui_state=SSTV_TX_UI_RF;
    tx_phase=SSTV_TX_PHASE_NONE;
    btn_tx.label="Halt TX";
    snprintf(tx_mode_label,sizeof(tx_mode_label),"TX Mode\n%s",cont_mode_name(tx_mode));
    btn_cont_mode.label=tx_mode_label;

    if (status_label) lv_obj_clear_flag(status_label,LV_OBJ_FLAG_HIDDEN);
    {
        char msg[96];
        snprintf(msg,sizeof(msg),tr("Preparing %s TX..."),cont_mode_name(tx_mode));
        set_status(msg);
    }
    if (progress_bar) {
        lv_bar_set_value(progress_bar,0,LV_ANIM_OFF);
        lv_obj_clear_flag(progress_bar,LV_OBJ_FLAG_HIDDEN);
    }
    if (btn_tx.disp_btn) buttons_refresh(&btn_tx);
    if (btn_cont_mode.disp_btn) buttons_refresh(&btn_cont_mode);
    update_rx_controls();

    int rc=pthread_create(&tx_thread,NULL,tx_thread_cb,NULL);
    if (rc != 0) {
        fprintf(stderr,"SSTV TX: pthread_create failed: %d\n",rc);
        tx_running=false;
        tx_result=-31;
        set_status("SSTV TX: thread creation failed.");
        if (progress_bar) lv_obj_add_flag(progress_bar,LV_OBJ_FLAG_HIDDEN);
        tx_return_to_auto_rx();
        return false;
    }

    tx_thread_valid=true;
    fprintf(stderr,"SSTV TX: interactive RF start %s, colors=%d, type=%s\n",
            cont_mode_name(tx_mode),tx_color_selected+1,
            tx_message_type == SSTV_TX_MESSAGE_CQ ? "CQ" :
            (tx_message_type == SSTV_TX_MESSAGE_REPLAY ? "REPLAY" :
             (tx_message_type == SSTV_TX_MESSAGE_IMAGE_ONLY ? "IMAGE ONLY" :
              (tx_message_type == SSTV_TX_MESSAGE_REPEAT_LAST ? "REPEAT LAST" : "REPLY"))));
    return true;
}

static bool tx_start_final_rf(void)
{
    return tx_start_final_rf_mode(cont_mode);
}

static void tx_color_pressed_cb(lv_event_t *e)
{
    (void)e;
    if (tx_ui_state!=SSTV_TX_UI_SETUP ||
        tx_phase!=SSTV_TX_PHASE_SELECT_COLORS || !tx_color_table) return;

    /* Confirm exactly the highlighted row even if deferred preview has not run. */
    tx_color_preview_pending=false;
    tx_color_preview_since_ms=0;
    tx_color_render_selected();

    tx_color_selector_destroy();
    tx_instruction_hide();
    lv_group_add_obj(keyboard_group,sstv_canvas);
    lv_group_focus_obj(sstv_canvas);
    lv_group_set_editing(keyboard_group,true);
    if (status_label) lv_obj_clear_flag(status_label,LV_OBJ_FLAG_HIDDEN);

    /* Phase 5 is the single RF launch point for both CQ and REPLY. */
    tx_start_final_rf();
}

static void tx_color_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *d=lv_event_get_draw_part_dsc(e);
    if (!d || d->part!=LV_PART_ITEMS || d->id<0 || d->id>=20 || !d->draw_area)
        return;

    /* Table cell text is deliberately blank; draw three color swatches. */
    const tx_color_combo_t *c=&tx_color_combos[d->id];
    tx_rgb_t colors[3]={c->top,c->middle,c->bottom};
    lv_area_t a=*d->draw_area;
    int box=22, gap=10;
    int total=3*box+2*gap;
    int x=a.x1+(lv_area_get_width(&a)-total)/2;
    int y=a.y1+(lv_area_get_height(&a)-box)/2;

    for (int i=0;i<3;i++) {
        lv_draw_rect_dsc_t rd;
        lv_draw_rect_dsc_init(&rd);
        rd.bg_opa=LV_OPA_COVER;
        rd.bg_color=lv_color_make(colors[i].r,colors[i].g,colors[i].b);
        rd.border_width=2;
        rd.border_opa=LV_OPA_COVER;
        rd.border_color=lv_color_black();
        lv_area_t r={x+i*(box+gap),y,x+i*(box+gap)+box-1,y+box-1};
        lv_draw_rect(d->draw_ctx,&rd,&r);
    }
}

static void tx_color_selector_destroy(void)
{
    tx_color_preview_pending=false;
    tx_color_preview_since_ms=0;
    if (tx_color_table) {
        lv_group_remove_obj(tx_color_table);
        lv_obj_del(tx_color_table);
        tx_color_table=NULL;
    }
}

static void tx_color_selector_build(void)
{
    tx_color_selector_destroy();
    if (status_label) lv_obj_add_flag(status_label,LV_OBJ_FLAG_HIDDEN);

    tx_instruction_show("TEXT COLORS", "MFK: choose - press: start the transmission - Cancel TX: back");

    /* The style is set before the rows are filled: LVGL sizes the rows then */
    tx_color_table=tx_list_create(20);

    /* Blank rows; the DRAW_PART callback paints three swatches per row. */
    for (int i=0;i<20;i++)
        lv_table_set_cell_value(tx_color_table,i,0," ");

    lv_obj_add_event_cb(tx_color_table,tx_color_draw_cb,LV_EVENT_DRAW_PART_END,NULL);
    lv_obj_add_event_cb(tx_color_table,tx_color_selection_changed_cb,
                        LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(tx_color_table,tx_color_pressed_cb,
                        LV_EVENT_PRESSED,NULL);

    lv_group_remove_obj(sstv_canvas);
    lv_group_add_obj(keyboard_group,tx_color_table);
    lv_group_focus_obj(tx_color_table);
    lv_group_set_editing(keyboard_group,true);

    /* Restore the last color-combination row used in this runtime.  This
     * LVGL version has no lv_table_set_selected_cell(), so drive the table
     * through the same key events used by MFK; LVGL also scrolls it into view. */
    int initial_row=tx_last_color_selected;
    if (initial_row<0 || initial_row>=20) initial_row=0;
    int16_t selected_row=LV_TABLE_CELL_NONE, selected_col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_color_table,&selected_row,&selected_col);
    if (selected_row==LV_TABLE_CELL_NONE) {
        uint32_t key=LV_KEY_DOWN;
        lv_event_send(tx_color_table,LV_EVENT_KEY,&key);
        lv_table_get_selected_cell(tx_color_table,&selected_row,&selected_col);
    }
    while (selected_row!=LV_TABLE_CELL_NONE && selected_row<initial_row) {
        uint32_t key=LV_KEY_DOWN;
        lv_event_send(tx_color_table,LV_EVENT_KEY,&key);
        lv_table_get_selected_cell(tx_color_table,&selected_row,&selected_col);
    }
    tx_color_apply(initial_row);
    tx_color_preview_pending=true;
    tx_color_preview_since_ms=lv_tick_get()-300;
}

static void tx_type_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *d=lv_event_get_draw_part_dsc(e);
    if (!d || d->part!=LV_PART_ITEMS || !d->label_dsc) return;
    if (((d->id==1 || d->id==2) && !tx_target_callsign[0]) ||
        (d->id==4 && !tx_last_valid))
        d->label_dsc->color=TC(text_mute);
}

static void tx_type_update_help(void)
{
    if (!tx_type_table || !frame.status) return;

    int16_t row=LV_TABLE_CELL_NONE,col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_type_table,&row,&col);

    /* One line in the status line of the app look */
    const char *text=SSTV_TX_HINT;
    switch (row) {
        case 0:
            text="A picture, then CQ CQ SSTV and your callsign on it";
            break;
        case 1:
            text=tx_target_callsign[0]
                ? "A picture and a text template, with the target callsign"
                : "Set the target callsign first";
            break;
        case 2:
            text=tx_target_callsign[0]
                ? "As Reply, with a miniature of the last received picture"
                : "Set the target callsign first";
            break;
        case 3:
            text="A picture, sent without any text";
            break;
        case 4:
            text=tx_last_valid
                ? "The last transmission again, same picture and mode"
                : "No transmission to repeat yet";
            break;
        case 5:
            text="It goes in the text of Reply and Replay";
            break;
        default:
            break;
    }

    lv_label_set_text(frame.status,text);
}

static void tx_type_selection_changed_cb(lv_event_t *e)
{
    (void)e;
    /* Remember the selected initial TX action for the next TX entry. */
    if (tx_type_table) {
        int16_t row=LV_TABLE_CELL_NONE,col=LV_TABLE_CELL_NONE;
        lv_table_get_selected_cell(tx_type_table,&row,&col);
        if (row>=0 && row<=5) tx_last_type_selected=row;
    }
    tx_type_update_help();
    /* Phase 1 intentionally leaves the received SSTV canvas untouched. */
}

static void tx_type_pressed_cb(lv_event_t *e)
{
    (void)e;
    if (tx_ui_state != SSTV_TX_UI_SETUP ||
        tx_phase != SSTV_TX_PHASE_SELECT_TYPE || !tx_type_table)
        return;

    int16_t row=LV_TABLE_CELL_NONE,col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_type_table,&row,&col);
    if (row==LV_TABLE_CELL_NONE || row<0 || row>5) return;

    if (row==0) {
        tx_message_type=SSTV_TX_MESSAGE_CQ;
        tx_type_selector_destroy();
        tx_phase=SSTV_TX_PHASE_SELECT_PICTURE;
        tx_picture_selector_build();
        fprintf(stderr,"SSTV TX UI: CALL CQ -> pictures\n");
        return;
    }

    /* REPLY and REPLAY both require a target callsign. */
    if (row==1 || row==2) {
        if (!tx_target_callsign[0]) return;
        tx_message_type=(row==2) ? SSTV_TX_MESSAGE_REPLAY : SSTV_TX_MESSAGE_REPLY;
        tx_type_selector_destroy();
        tx_phase=SSTV_TX_PHASE_SELECT_PICTURE;
        tx_picture_selector_build();
        fprintf(stderr,"SSTV TX UI: %s TO %s -> pictures\n",
                row==2 ? "REPLAY" : "REPLY",tx_target_callsign);
        return;
    }

    if (row==3) {
        tx_message_type=SSTV_TX_MESSAGE_IMAGE_ONLY;
        tx_type_selector_destroy();
        tx_phase=SSTV_TX_PHASE_SELECT_PICTURE;
        tx_picture_selector_build();
        fprintf(stderr,"SSTV TX UI: TX IMAGE ONLY -> pictures\n");
        return;
    }

    if (row==4) {
        if (!tx_last_valid) return;
        tx_message_type=SSTV_TX_MESSAGE_REPEAT_LAST;
        tx_type_selector_destroy();
        tx_instruction_hide();
        tx_picture_width=tx_last_width;
        tx_picture_height=tx_last_height;
        memcpy(tx_final_rgb,tx_last_rgb,(size_t)tx_last_width*tx_last_height*3);
        tx_picture_render_preview();
        lv_group_add_obj(keyboard_group,sstv_canvas);
        lv_group_focus_obj(sstv_canvas);
        lv_group_set_editing(keyboard_group,true);
        if (status_label) lv_obj_clear_flag(status_label,LV_OBJ_FLAG_HIDDEN);
        fprintf(stderr,"SSTV TX UI: REPEAT LAST TX -> RF (%s)\n",
                cont_mode_name(tx_last_mode));
        tx_start_final_rf_mode(tx_last_mode);
        return;
    }

    /* SET TARGET CALLSIGN */
    lv_group_remove_obj(tx_type_table);
    tx_target_keyboard_open_pending=true;
    return;
}

static void tx_type_selector_build(void)
{
    tx_type_selector_destroy();
    tx_phase=SSTV_TX_PHASE_SELECT_TYPE;

    if (status_label) lv_obj_add_flag(status_label,LV_OBJ_FLAG_HIDDEN);

    /* This is now the FIRST TX phase.  Keep the received image visible so
     * the operator can read the remote callsign while choosing the action. */
    tx_restore_rx_canvas();
    tx_instruction_show("SSTV TX", SSTV_TX_HINT);

    tx_type_table=tx_list_create(6);

    char reply[64], replay[64];
    snprintf(reply,sizeof(reply),tx_target_callsign[0] ? "REPLY TO %s" : "REPLY TO...",
             tx_target_callsign);
    snprintf(replay,sizeof(replay),tx_target_callsign[0] ? tr("REPLAY TO %s") : tr("REPLAY TO..."),
             tx_target_callsign);
    lv_table_set_cell_value(tx_type_table,0,0,"CALL CQ");
    lv_table_set_cell_value(tx_type_table,1,0,reply);
    lv_table_set_cell_value(tx_type_table,2,0,replay);
    lv_table_set_cell_value(tx_type_table,3,0,"TX IMAGE ONLY");
    lv_table_set_cell_value(tx_type_table,4,0,"REPEAT LAST TX");
    lv_table_set_cell_value(tx_type_table,5,0,"SET TARGET CALLSIGN");

    lv_obj_add_event_cb(tx_type_table,tx_type_draw_cb,LV_EVENT_DRAW_PART_BEGIN,NULL);
    lv_obj_add_event_cb(tx_type_table,tx_type_selection_changed_cb,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(tx_type_table,tx_type_pressed_cb,LV_EVENT_PRESSED,NULL);

    /* With no target, REPLY/REPLAY remain selectable for navigation but pressing them
     * is deliberately a no-op; the label itself makes the unavailable state clear. */
    lv_group_remove_obj(sstv_canvas);
    lv_group_add_obj(keyboard_group,tx_type_table);
    lv_group_focus_obj(tx_type_table);
    lv_group_set_editing(keyboard_group,true);

    /* Restore the last action row selected during this runtime.  If REPLY/REPLAY was
     * remembered but there is currently no target callsign, fall back to
     * SET TARGET CALLSIGN because both are disabled in that state. */
    int initial_row=tx_last_type_selected;
    if (initial_row<0 || initial_row>5) initial_row=0;
    if ((initial_row==1 || initial_row==2) && !tx_target_callsign[0])
        initial_row=5;
    if (initial_row==4 && !tx_last_valid)
        initial_row=0;

    int16_t selected_row=LV_TABLE_CELL_NONE, selected_col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_type_table,&selected_row,&selected_col);
    if (selected_row==LV_TABLE_CELL_NONE) {
        uint32_t key=LV_KEY_DOWN;
        lv_event_send(tx_type_table,LV_EVENT_KEY,&key);
        lv_table_get_selected_cell(tx_type_table,&selected_row,&selected_col);
    }
    while (selected_row!=LV_TABLE_CELL_NONE && selected_row<initial_row) {
        uint32_t key=LV_KEY_DOWN;
        lv_event_send(tx_type_table,LV_EVENT_KEY,&key);
        lv_table_get_selected_cell(tx_type_table,&selected_row,&selected_col);
    }
    tx_last_type_selected=initial_row;
    tx_type_update_help();

    fprintf(stderr,"SSTV TX UI: initial action selector; target=%s\n",
            tx_target_callsign[0] ? tx_target_callsign : "<not set>");
}

static void tx_picture_selector_destroy(void)
{
    if (tx_picture_table) {
        lv_group_remove_obj(tx_picture_table);
        lv_obj_del(tx_picture_table);
        tx_picture_table = NULL;
    }
    tx_picture_free_files();
    tx_picture_selected = -1;
    tx_picture_pending = -1;
    tx_picture_pending_since_ms = 0;
//    tx_picture_width = tx_picture_height = 0;
    tx_picture_name[0] = '\0';
}

static void tx_picture_display_name(const char *filename,
                                    char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return;

    out[0] = '\0';
    if (!filename)
        return;

    size_t len = strlen(filename);
    if (len >= 4 && strcmp(filename + len - 4, ".png") == 0)
        len -= 4;

    /* The TX browser is deliberately narrow.  Keep every item on one line
     * and use an explicit ellipsis rather than letting LVGL wrap it. */
    const size_t max_chars = 22;
    if (len <= max_chars) {
        snprintf(out, out_size, "%.*s", (int)len, filename);
    } else {
        size_t keep = max_chars - 3;
        snprintf(out, out_size, "%.*s...", (int)keep, filename);
    }
}

static void tx_picture_selector_build(void)
{
    tx_picture_selector_destroy();
    tx_picture_selector_refresh();

    if (status_label)
        lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);

    tx_instruction_show("TX PICTURE", "MFK: choose - press: confirm - TX Mode: mode - Cancel TX: back");

    tx_picture_table = tx_list_create(1);

    if (tx_picture_file_count == 0) {
        lv_table_set_cell_value(tx_picture_table, 0, 0, "No TX pictures found");
        buttons_disabled(&btn_cont_mode, false);
        lv_group_add_obj(keyboard_group, sstv_canvas);
        lv_group_focus_obj(sstv_canvas);
        lv_group_set_editing(keyboard_group, true);
        set_status("No TX pictures found");
        if (frame.status) lv_label_set_text(frame.status, "Put PNG pictures in " SSTV_TX_PICS_DIR);
        return;
    }

    lv_table_set_row_cnt(tx_picture_table, tx_picture_file_count);
    for (size_t i = 0; i < tx_picture_file_count; ++i) {
        char display_name[64];
        tx_picture_display_name(tx_picture_files[i],
                                display_name, sizeof(display_name));
        lv_table_set_cell_value(tx_picture_table, i, 0, display_name);
    }

    lv_obj_add_event_cb(tx_picture_table, tx_picture_selection_changed_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(tx_picture_table, tx_picture_pressed_cb,
                        LV_EVENT_PRESSED, NULL);

    lv_group_remove_obj(sstv_canvas);
    lv_group_add_obj(keyboard_group, tx_picture_table);
    lv_group_focus_obj(tx_picture_table);
    lv_group_set_editing(keyboard_group, true);

    int initial_row=0;
    if (tx_last_picture_file[0]) {
        for (size_t i=0;i<tx_picture_file_count;i++) {
            if (strcmp(tx_picture_files[i],tx_last_picture_file)==0) {
                initial_row=(int)i;
                break;
            }
        }
    }

    /* This firmware's LVGL has get_selected_cell(), but not the newer
     * lv_table_set_selected_cell().  Drive the table through the same LV_KEY
     * interface used by MFK instead; the table updates selection and scrolling
     * itself, so this stays compatible with the installed LVGL version. */
    int16_t selected_row=LV_TABLE_CELL_NONE, selected_col=LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(tx_picture_table,&selected_row,&selected_col);

    if (selected_row==LV_TABLE_CELL_NONE) {
        uint32_t key=LV_KEY_DOWN;
        lv_event_send(tx_picture_table,LV_EVENT_KEY,&key);
        lv_table_get_selected_cell(tx_picture_table,&selected_row,&selected_col);
    }

    while (selected_row!=LV_TABLE_CELL_NONE && selected_row<initial_row) {
        uint32_t key=LV_KEY_DOWN;
        lv_event_send(tx_picture_table,LV_EVENT_KEY,&key);
        lv_table_get_selected_cell(tx_picture_table,&selected_row,&selected_col);
    }

    tx_picture_pending=initial_row;
    tx_picture_pending_since_ms=tx_now_ms()-SSTV_TX_PICTURE_DEBOUNCE_MS;
    tx_picture_load_pending_preview();

    fprintf(stderr,"SSTV TX UI: %zu TX pictures found in %s; initial=%s\n",
            tx_picture_file_count,SSTV_TX_PICS_DIR,tx_picture_files[initial_row]);
}

static int sstv_browser_name_compare(const void *a, const void *b)
{
    const char *const *sa = a;
    const char *const *sb = b;
    return strcmp(*sb, *sa);   /* newest timestamped filename first */
}

static void sstv_browser_free_files(void)
{
    for (unsigned int i = 0; i < sstv_browser_file_count; ++i)
        free(sstv_browser_files[i]);

    free(sstv_browser_files);
    sstv_browser_files = NULL;
    sstv_browser_file_count = 0;
    sstv_browser_file_capacity = 0;
}

static void sstv_browser_refresh(void)
{
    sstv_browser_free_files();

    DIR *dir = opendir(SSTV_SAVE_DIR);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);

        if (len < 5 || strcmp(name + len - 4, ".png") != 0)
            continue;
        if (strncmp(name, "sstv-", 5) != 0)
            continue;

        if (sstv_browser_file_count == sstv_browser_file_capacity) {
            size_t new_capacity = sstv_browser_file_capacity ?
                                  sstv_browser_file_capacity * 2 : 128;
            char **new_files = realloc(sstv_browser_files,
                                       new_capacity * sizeof(*new_files));
            if (!new_files)
                break;

            sstv_browser_files = new_files;
            sstv_browser_file_capacity = new_capacity;
        }

        char *copy = strdup(name);
        if (copy)
            sstv_browser_files[sstv_browser_file_count++] = copy;
    }
    closedir(dir);

    qsort(sstv_browser_files, sstv_browser_file_count,
          sizeof(sstv_browser_files[0]), sstv_browser_name_compare);
}

static void sstv_browser_update_list(void)
{
    if (!sstv_browser_table)
        return;

    if (sstv_browser_file_count == 0) {
        lv_table_set_row_cnt(sstv_browser_table, 1);
        lv_table_set_cell_value(sstv_browser_table, 0, 0, "No saved pictures");
    } else {
        lv_table_set_row_cnt(sstv_browser_table, sstv_browser_file_count);
        for (unsigned int i = 0; i < sstv_browser_file_count; ++i)
            lv_table_set_cell_value(sstv_browser_table, i, 0, sstv_browser_files[i]);
    }
    lv_obj_invalidate(sstv_browser_table);
}

static void sstv_browser_update_buttons(void)
{
    if (sstv_browser_preview) {
        btn_delete_picture.label = "Done";
        buttons_disabled(&btn_delete_picture, false);
    } else {
        btn_delete_picture.label = "Delete\npicture";
        buttons_disabled(&btn_delete_picture,
                         !sstv_browser_open || sstv_browser_file_count == 0);
    }

    if (sstv_browser_open)
        btn_browser.label = "Close\nBrowser";
    else
        btn_browser.label = "RX Pics\nBrowser";

    if (btn_browser.disp_btn)
        buttons_refresh(&btn_browser);
    if (btn_delete_picture.disp_btn)
        buttons_refresh(&btn_delete_picture);
}

static void sstv_browser_free_image(void)
{
    free(sstv_browser_image);
    sstv_browser_image = NULL;
    sstv_browser_image_width = 0;
    sstv_browser_image_height = 0;
    sstv_browser_preview = false;
}

static void sstv_browser_render_image(void)
{
    if (!sstv_browser_buffer || !sstv_browser_image ||
        !sstv_browser_image_width || !sstv_browser_image_height)
        return;

    const unsigned int width = sstv_browser_image_width;
    const unsigned int height = sstv_browser_image_height;
    unsigned int out_w = SSTV_BROWSER_W;
    unsigned int out_h = (unsigned int)(((uint64_t)height * SSTV_BROWSER_W) / width);

    if (out_h > SSTV_BROWSER_H) {
        out_h = SSTV_BROWSER_H;
        out_w = (unsigned int)(((uint64_t)width * SSTV_BROWSER_H) / height);
    }
    if (out_w < 1) out_w = 1;
    if (out_h < 1) out_h = 1;

    const unsigned int x0 = (SSTV_BROWSER_W - out_w) / 2;
    const unsigned int y0 = (SSTV_BROWSER_H - out_h) / 2;
    const lv_color_t black = lv_color_make(0, 0, 0);

    for (unsigned int i = 0; i < SSTV_BROWSER_W * SSTV_BROWSER_H; ++i)
        sstv_browser_buffer[i] = black;

    for (unsigned int oy = 0; oy < out_h; ++oy) {
        unsigned int sy = (unsigned int)(((uint64_t)oy * height) / out_h);
        if (sy >= height) sy = height - 1;

        for (unsigned int ox = 0; ox < out_w; ++ox) {
            unsigned int sx = (unsigned int)(((uint64_t)ox * width) / out_w);
            if (sx >= width) sx = width - 1;

            const uint8_t *p = &sstv_browser_image[
                ((size_t)sy * width + sx) * 3u
            ];
            sstv_browser_buffer[(y0 + oy) * SSTV_BROWSER_W + x0 + ox] =
                lv_color_make(p[0], p[1], p[2]);
        }
    }

    if (sstv_browser_canvas)
        lv_obj_invalidate(sstv_browser_canvas);
}

static bool sstv_browser_open_png(const char *filename)
{
    if (!filename || !*filename)
        return false;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s", SSTV_SAVE_DIR, filename);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "SSTV BROWSER: unable to open %s: %s\n",
                path, strerror(errno));
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                 NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return false;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        fprintf(stderr, "SSTV BROWSER: libpng read error for %s\n", path);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
    png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);

    if (!width || !height || width > 10000 || height > 10000) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    if (bit_depth == 16) png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    color_type = png_get_color_type(png_ptr, info_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_read_update_info(png_ptr, info_ptr);
    png_size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int channels = png_get_channels(png_ptr, info_ptr);
    if (channels < 3 || !rowbytes) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    uint8_t *image = malloc((size_t)width * (size_t)height * 3u);
    uint8_t *row = malloc(rowbytes);
    if (!image || !row) {
        free(image);
        free(row);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    for (png_uint_32 y = 0; y < height; ++y) {
        png_read_row(png_ptr, row, NULL);
        for (png_uint_32 x = 0; x < width; ++x) {
            const uint8_t *sp = row + (size_t)x * (size_t)channels;
            uint8_t *dp = image + ((size_t)y * width + x) * 3u;
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
        }
    }

    png_read_end(png_ptr, NULL);
    free(row);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    sstv_browser_free_image();
    sstv_browser_image = image;
    sstv_browser_image_width = (unsigned int)width;
    sstv_browser_image_height = (unsigned int)height;

    if (!sstv_browser_buffer) {
        sstv_browser_buffer = malloc(
            (size_t)SSTV_BROWSER_W * SSTV_BROWSER_H * sizeof(lv_color_t)
        );
        if (!sstv_browser_buffer) {
            sstv_browser_free_image();
            return false;
        }
    }

    if (!sstv_browser_canvas) {
        sstv_browser_canvas = lv_canvas_create(dialog.obj);
        lv_canvas_set_buffer(sstv_browser_canvas, sstv_browser_buffer,
                             SSTV_BROWSER_W, SSTV_BROWSER_H,
                             LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_size(sstv_browser_canvas, SSTV_BROWSER_W, SSTV_BROWSER_H);
        lv_obj_set_pos(sstv_browser_canvas, APP_FRAME_X + (APP_FRAME_W - SSTV_BROWSER_W) / 2, APP_FRAME_BODY_Y + 2);
        lv_obj_add_event_cb(sstv_browser_canvas,
                            sstv_browser_preview_pressed_cb,
                            LV_EVENT_PRESSED, NULL);
    }

    sstv_browser_render_image();

    if (sstv_browser_table) {
        lv_group_remove_obj(sstv_browser_table);
        lv_obj_add_flag(sstv_browser_table, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_clear_flag(sstv_browser_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_group_add_obj(keyboard_group, sstv_browser_canvas);
    lv_group_focus_obj(sstv_browser_canvas);
    lv_group_set_editing(keyboard_group, true);

    sstv_browser_preview = true;
    sstv_browser_update_buttons();

    fprintf(stderr, "SSTV BROWSER: opened %s (%u x %u)\n",
            path, sstv_browser_image_width, sstv_browser_image_height);
    return true;
}

static void sstv_browser_pressed_cb(lv_event_t *e)
{
    (void)e;
    if (!sstv_browser_table || sstv_browser_file_count == 0)
        return;

    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(sstv_browser_table, &row, &col);

    if (row == LV_TABLE_CELL_NONE || row < 0 ||
        (unsigned int)row >= sstv_browser_file_count)
        return;

    sstv_browser_open_png(sstv_browser_files[row]);
}

static void sstv_browser_return_to_list(void)
{
    if (!sstv_browser_open)
        return;

    if (sstv_browser_canvas) {
        lv_group_remove_obj(sstv_browser_canvas);
        lv_obj_add_flag(sstv_browser_canvas, LV_OBJ_FLAG_HIDDEN);
    }

    sstv_browser_free_image();
    sstv_browser_refresh();

    if (sstv_browser_table) {
        lv_obj_clear_flag(sstv_browser_table, LV_OBJ_FLAG_HIDDEN);
        sstv_browser_update_list();
        lv_group_add_obj(keyboard_group, sstv_browser_table);
        lv_group_focus_obj(sstv_browser_table);
        lv_group_set_editing(keyboard_group, true);
        lv_obj_invalidate(sstv_browser_table);
    }
    sstv_browser_update_buttons();
}

static void sstv_browser_preview_pressed_cb(lv_event_t *e)
{
    (void)e;
    /* MFK press on the preview returns to the file list. */
    sstv_browser_return_to_list();
}

static void sstv_browser_delete_selected(void)
{
    if (!sstv_browser_open || sstv_browser_preview ||
        !sstv_browser_table || sstv_browser_file_count == 0)
        return;

    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;
    lv_table_get_selected_cell(sstv_browser_table, &row, &col);

    if (row == LV_TABLE_CELL_NONE || row < 0 ||
        (unsigned int)row >= sstv_browser_file_count)
        return;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s",
             SSTV_SAVE_DIR, sstv_browser_files[row]);

    if (unlink(path) != 0) {
        fprintf(stderr, "SSTV BROWSER: unable to delete %s: %s\n",
                path, strerror(errno));
        return;
    }

    fprintf(stderr, "SSTV BROWSER: deleted %s\n", path);

    /* Re-scan every time: deleting the last picture immediately changes the
     * table to "No saved pictures", exactly like the WEFAX browser. */
    sstv_browser_refresh();
    sstv_browser_update_list();
    sstv_browser_update_buttons();

    lv_group_focus_obj(sstv_browser_table);
    lv_group_set_editing(keyboard_group, true);
}

static void sstv_browser_build(void)
{
    if (sstv_browser_open)
        return;

    sstv_browser_refresh();

    /* Opaque browser background: deliberately identical to the main SSTV
     * frame, but kept above the live SSTV view so it hides the canvas/text
     * underneath while the saved-picture browser is open. */
    sstv_browser_frame = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(sstv_browser_frame);
    app_frame_body(sstv_browser_frame);   /* the body of the common app look */
    lv_obj_clear_flag(sstv_browser_frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sstv_browser_frame, LV_OBJ_FLAG_CLICKABLE);

    sstv_browser_table = lv_table_create(dialog.obj);
    lv_obj_set_style_border_width(sstv_browser_table, 0, LV_PART_MAIN);

    /* Inside the body of the common app look, like the WEFAX browser */
    lv_obj_set_size(sstv_browser_table, 700, APP_FRAME_BODY_H - 16);
    lv_obj_set_pos(sstv_browser_table, APP_FRAME_X + (APP_FRAME_W - 700) / 2, APP_FRAME_BODY_Y + 8);
    lv_table_set_col_cnt(sstv_browser_table, 1);
    lv_table_set_col_width(sstv_browser_table, 0, 690);

    lv_obj_set_style_bg_color(sstv_browser_table, TC(bg),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sstv_browser_table, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sstv_browser_table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(sstv_browser_table, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(sstv_browser_table, TC(text),
                                LV_PART_ITEMS);
    lv_obj_set_style_text_font(sstv_browser_table, &sony_24, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(sstv_browser_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(sstv_browser_table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(sstv_browser_table, 12, LV_PART_ITEMS);
    lv_obj_set_style_text_color(sstv_browser_table, TC(text_mute),
                                LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(sstv_browser_table, LV_OPA_TRANSP,
                            LV_PART_ITEMS | LV_STATE_EDITED);

    sstv_browser_update_list();
    lv_obj_add_event_cb(sstv_browser_table, sstv_browser_pressed_cb,
                        LV_EVENT_PRESSED, NULL);
    lv_group_add_obj(keyboard_group, sstv_browser_table);
    lv_group_focus_obj(sstv_browser_table);
    lv_group_set_editing(keyboard_group, true);

    sstv_browser_open = true;
    sstv_browser_preview = false;
    sstv_browser_update_buttons();
}

static void sstv_browser_close(void)
{
    if (sstv_browser_table) {
        lv_group_remove_obj(sstv_browser_table);
        lv_obj_del(sstv_browser_table);
        sstv_browser_table = NULL;
    }

    if (sstv_browser_canvas) {
        lv_group_remove_obj(sstv_browser_canvas);
        lv_obj_del(sstv_browser_canvas);
        sstv_browser_canvas = NULL;
    }

    if (sstv_browser_frame) {
        lv_obj_del(sstv_browser_frame);
        sstv_browser_frame = NULL;
    }

    free(sstv_browser_buffer);
    sstv_browser_buffer = NULL;
    sstv_browser_free_image();
    sstv_browser_free_files();

    sstv_browser_open = false;
    sstv_browser_preview = false;

    if (dialog.obj)
        lv_obj_invalidate(dialog.obj);

    sstv_browser_update_buttons();
}

static void browser_cb(button_data_t *data)
{
    (void)data;

    if (sstv_browser_open)
        sstv_browser_close();
    else
        sstv_browser_build();
}

static void delete_picture_cb(button_data_t *data)
{
    (void)data;

    if (sstv_browser_preview)
        sstv_browser_return_to_list();
    else
        sstv_browser_delete_selected();
}

static void cont_mode_cb(button_data_t *data)
{
    (void)data;
    if (tx_ui_state == SSTV_TX_UI_RF) return;
    if (tx_ui_state == SSTV_TX_UI_RX &&
        (live_enabled || live_capturing || cont_capturing || cont_decode_pending || test_running))
        return;
    unsigned count = sizeof(cont_modes)/sizeof(cont_modes[0]);
    unsigned idx = 0;
    while (idx < count && cont_modes[idx] != cont_mode) ++idx;
    if (tx_ui_state == SSTV_TX_UI_RX && cont_autodetect) {
        cont_autodetect = false;            /* RX: Auto, then every mode, then Auto */
        cont_mode = cont_modes[0];
    } else if (tx_ui_state == SSTV_TX_UI_RX && idx + 1 >= count) {
        cont_autodetect = true;
    } else {
        cont_mode = cont_modes[(idx + 1) % count];
    }
    snprintf(tx_mode_label, sizeof(tx_mode_label), "%s Mode\n%s",
             tx_ui_state == SSTV_TX_UI_RX ? "RX" : "TX",
             tx_ui_state == SSTV_TX_UI_RX ? cont_rx_mode_name() : cont_mode_name(cont_mode));
    btn_cont_mode.label = tx_mode_label;
    if (btn_cont_mode.disp_btn) buttons_refresh(&btn_cont_mode);

    if (tx_ui_state == SSTV_TX_UI_SETUP &&
        tx_picture_selected >= 0 &&
        (size_t)tx_picture_selected < tx_picture_file_count) {
        if (tx_picture_load_and_prepare(tx_picture_files[tx_picture_selected]))
            tx_rebuild_current_overlay();
    }
}

static void cont_start_cb(button_data_t *data)
{
    (void)data;
    pthread_mutex_lock(&live_mutex);

    if (cont_capturing) {
        cont_capturing = false;
        if (cont_written > 0) {
            cont_decode_mode = cont_rx_decode_mode();
            cont_decode_pending = true;
            fprintf(stderr, "SSTV CONT: STOP requested after %.1f s (%ld samples); decode pending as %s\n",
                    (double)cont_written/SSTV_SAMPLE_RATE, cont_written, cont_mode_name(cont_decode_mode));
        } else {
            free(cont_samples); cont_samples=NULL; cont_capacity=cont_written=0;
        }
        pthread_mutex_unlock(&live_mutex);
        btn_cont_start.label = "Start\nManual RX";
        if (btn_cont_start.disp_btn) buttons_refresh(&btn_cont_start);
        update_rx_controls();
        return;
    }

    if (live_enabled || live_capturing || live_decode_pending || test_running) {
        pthread_mutex_unlock(&live_mutex);
        return;
    }

    /* Auto: the limit becomes the picture length once the mode is found */
    float cont_limit_seconds = cont_autodetect ? (float)SSTV_CONT_MAX_SECONDS : cont_mode_seconds(cont_mode) + 5.0f;
    if (cont_limit_seconds > (float)SSTV_CONT_MAX_SECONDS)
        cont_limit_seconds = (float)SSTV_CONT_MAX_SECONDS;
    cont_capacity = (long)(cont_limit_seconds * (float)SSTV_SAMPLE_RATE);
    cont_samples = malloc((size_t)cont_capacity * sizeof(float));
    if (!cont_samples) {
        cont_capacity=0;
        pthread_mutex_unlock(&live_mutex);
        fprintf(stderr, "SSTV CONT: ERROR allocating capture buffer\n");
        return;
    }
    cont_written=0;
    cont_limit=cont_capacity;
    cont_locked=false;
    sstv_linesync_init(&cont_ls, cont_autodetect ? SSTV_MODE_UNKNOWN : cont_mode);
    /* forced mode: rows start at the tracker's first sync; Auto: at the lock */
    if (!cont_autodetect) sstv_preview_init(&cont_pv, cont_mode, -1.0);
    cont_pv_on=!cont_autodetect;
    cont_capturing=true;
    pthread_mutex_unlock(&live_mutex);

    if (sstv_canvas) lv_canvas_fill_bg(sstv_canvas, lv_color_white(), LV_OPA_COVER);
    char status[192];
    if (cont_autodetect)
        snprintf(status, sizeof(status),
                 tr("SSTV Manual decoder is running\n\nSearching the line syncs for the SSTV mode\n\nPress Stop Manual RX to render received picture"));
    else
        snprintf(status, sizeof(status),
                 tr("SSTV Manual decoder is running\n\nDecoding %s signal\n\nPress Stop Manual RX to render received picture"),
                 cont_mode_name(cont_mode));
    set_status(status);
    if (progress_bar) lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    btn_cont_start.label = "Stop\nManual RX";
    if (btn_cont_start.disp_btn) buttons_refresh(&btn_cont_start);
    update_rx_controls();
    fprintf(stderr,"SSTV CONT: START - manual %s capture at %d Hz, auto-stop after %.1f s\n",
            cont_rx_mode_name(),SSTV_SAMPLE_RATE,cont_limit_seconds);
}

static void start_cb(button_data_t *data)
{
    (void)data;
    pthread_mutex_lock(&live_mutex);

    if (cont_capturing || cont_decode_pending || cont_decoding) {
        pthread_mutex_unlock(&live_mutex);
        return;
    }

    /* While Auto RX is receiving/decoding a detected picture, this button is
     * "Halt Decoding": discard only the current picture and keep Auto RX on. */
    if (live_capturing || live_decode_pending || live_decoding) {
        live_abort_requested=true;
        live_capturing=false;
        live_decode_pending=false;
        live_ring_pos=live_ring_count=0;
        live_since_check=0;
        live_wait_samples=live_wait_log_mark=0;
        if (!live_decoding) {
            free(live_samples);
            live_samples=NULL;
            live_written=live_target=0;
            live_abort_requested=false;
        }
        live_enabled=true;
        pthread_mutex_unlock(&live_mutex);
        set_receiver_ui_listening();
        fprintf(stderr,"SSTV LIVE: HALT - current picture discarded; awaiting next VIS\n");
        return;
    }

    if (live_enabled || live_capturing) {
        live_enabled=false; live_capturing=false; live_decode_pending=false;
        live_ring_pos=live_ring_count=0; live_since_check=0;
        live_wait_samples=live_wait_log_mark=0;
        free(live_samples); live_samples=NULL; live_written=live_target=0;
        pthread_mutex_unlock(&live_mutex);
        reset_ui_to_standby();
        fprintf(stderr,"SSTV LIVE: STOP - receiver returned to stand-by\n");
        return;
    }

    if (!test_running) {
        live_enabled=true; live_capturing=false; live_decode_pending=false;
        live_abort_requested=false;
        live_ring_pos=live_ring_count=0; live_since_check=0;
        live_wait_samples=live_wait_log_mark=0;
        free(live_samples); live_samples=NULL; live_written=live_target=0;
        fprintf(stderr,"SSTV LIVE: START - listening to RX audio at %d Hz\n", SSTV_SAMPLE_RATE);
        fprintf(stderr,"SSTV LIVE: waiting for 1900/1200/1900 VIS leader...\n");
    }
    pthread_mutex_unlock(&live_mutex);
    set_receiver_ui_listening();
}

static void audio_cb(unsigned int n, float *samples)
{
    if ((!live_enabled && !cont_capturing) || !samples || n==0 || test_running) return;
    pthread_mutex_lock(&live_mutex);

    if (cont_capturing) {
        long room=cont_capacity-cont_written;
        long copy=(long)n < room ? (long)n : room;
        if (copy>0) {
            memcpy(cont_samples+cont_written,samples,(size_t)copy*sizeof(float));
            cont_written += copy;
            sstv_linesync_process(&cont_ls,samples,(size_t)copy);
        }
        const sstv_ls_status_t *ls=sstv_linesync_status(&cont_ls);
        if (!cont_locked && ls->state!=SSTV_LS_SEARCH) {
            cont_locked=true;
            cont_lock_ui_pending=true;
            if (cont_autodetect) {
                /* the whole picture from its first sync, then stop */
                long lim=(long)ls->first_sync+(long)((sstv_linesync_picture_seconds(ls->mode)+5.0)*SSTV_SAMPLE_RATE);
                if (lim<cont_limit) cont_limit=lim;
                sstv_preview_init(&cont_pv,ls->mode,-1.0);
                cont_pv_on=true;
            }
            fprintf(stderr,"SSTV CONT: line syncs locked: %s after %.1f s\n",
                    cont_mode_name(ls->mode),(double)cont_written/SSTV_SAMPLE_RATE);
        }
        if (cont_written >= cont_limit || ls->state==SSTV_LS_LOST) {
            cont_capturing=false;
            cont_decode_mode=cont_rx_decode_mode();
            cont_decode_pending=true;
            fprintf(stderr,"SSTV CONT: %s after %.1f s (clock %+.0f ppm); stopping and decoding %s\n",
                    ls->state==SSTV_LS_LOST ? "end of picture" : "time limit",
                    (double)cont_written/SSTV_SAMPLE_RATE,ls->clock_ppm,cont_mode_name(cont_decode_mode));
        }
        pthread_mutex_unlock(&live_mutex);
        return;
    }

    if (!live_enabled) { pthread_mutex_unlock(&live_mutex); return; }

    if (live_capturing) {
        long room=live_target-live_written;
        long copy=(long)n < room ? (long)n : room;
        if (copy>0) {
            memcpy(live_samples+live_written,samples,(size_t)copy*sizeof(float)); live_written+=copy;
            sstv_linesync_process(&live_ls,samples,(size_t)copy);   /* progressive display */
        }
        static int last_sec=-1;
        int sec=(int)(live_written/SSTV_SAMPLE_RATE);
        if (sec/10 != last_sec/10) {
            last_sec=sec;
            fprintf(stderr,"SSTV LIVE: receiving %s into RAM... %d / %.0f s\n",live_info.name,sec,(double)live_target/SSTV_SAMPLE_RATE);
        }
        if (live_written >= live_target) {
            live_capturing=false; live_decode_pending=true; live_enabled=false;
            fprintf(stderr,"SSTV LIVE: expected %s duration reached; RAM capture complete (%ld samples)\n",
                    live_info.name,live_written);
            pthread_mutex_unlock(&live_mutex); return;
        }

        /* Historical behaviour ends here.  With RESTART OFF we return exactly
         * as before; the parallel VIS detector below is completely bypassed. */
        if (!live_restart_enabled) {
            pthread_mutex_unlock(&live_mutex); return;
        }

        /* RESTART ON: feed the same rolling VIS detector in parallel with the
         * current capture.  The ring is reset whenever a picture starts, so
         * the VIS that started this picture cannot trigger itself again. */
        for (unsigned int i=0;i<n;++i) {
            live_ring[live_ring_pos]=samples[i];
            live_ring_pos=(live_ring_pos+1)%SSTV_PREBUF_SAMPLES;
            if (live_ring_count<SSTV_PREBUF_SAMPLES) ++live_ring_count;
        }
        live_since_check += n;
        if (live_ring_count >= SSTV_VIS_WINDOW_SAMPLES && live_since_check >= SSTV_VIS_CHECK_STEP) {
            live_since_check=0;
            size_t start=(live_ring_pos + SSTV_PREBUF_SAMPLES - SSTV_VIS_WINDOW_SAMPLES)%SSTV_PREBUF_SAMPLES;
            for (size_t i=0;i<SSTV_VIS_WINDOW_SAMPLES;++i)
                live_vis_window[i]=live_ring[(start+i)%SSTV_PREBUF_SAMPLES];

            sstv_info_t restart_info;
            if (live_find_vis(live_vis_window,SSTV_VIS_WINDOW_SAMPLES,&restart_info)) {
                long restart_target=(long)((live_mode_seconds(restart_info.mode)+2.0f)*SSTV_SAMPLE_RATE);
                float *restart_samples=malloc((size_t)restart_target*sizeof(float));
                if (!restart_samples) {
                    fprintf(stderr,"SSTV LIVE: RESTART ignored - ERROR allocating %.1f MB capture buffer\n",
                            (double)restart_target*sizeof(float)/(1024.0*1024.0));
                } else {
                    size_t keep=live_ring_count;
                    if ((long)keep>restart_target) keep=(size_t)restart_target;
                    size_t oldest=(live_ring_pos + SSTV_PREBUF_SAMPLES - live_ring_count)%SSTV_PREBUF_SAMPLES;
                    for (size_t i=0;i<keep;++i)
                        restart_samples[i]=live_ring[(oldest+i)%SSTV_PREBUF_SAMPLES];

                    fprintf(stderr,"SSTV LIVE: RESTART - new VIS=%d -> %s; discarding current %s capture at %.1f s\n",
                            restart_info.vis,restart_info.name,live_info.name,
                            (double)live_written/SSTV_SAMPLE_RATE);

                    free(live_samples);
                    live_samples=restart_samples;
                    live_written=(long)keep;
                    live_target=restart_target;
                    live_info=restart_info;
                    live_canvas_clear_pending=true;
                    live_preview_start();

                    /* Same reply-mode update used by the normal first VIS. */
                    switch (live_info.mode) {
                        case SSTV_MODE_ROBOT8_BW:
                        case SSTV_MODE_ROBOT36:
                        case SSTV_MODE_ROBOT72:
                        case SSTV_MODE_MARTIN_M1:
                        case SSTV_MODE_MARTIN_M2:
                        case SSTV_MODE_SCOTTIE_S1:
                        case SSTV_MODE_SCOTTIE_S2:
                            cont_mode=live_info.mode;
                            live_mode_ui_pending=true;
                            break;
                        default:
                            break;
                    }

                    /* Arm for a later, genuinely new VIS rather than finding
                     * this same header again in the rolling window. */
                    live_ring_pos=0;
                    live_ring_count=0;
                    live_since_check=0;
                    fprintf(stderr,"SSTV LIVE: RESTART capturing %s from new VIS; target %.1f s, prebuffer=%ld samples\n",
                            live_info.name,(double)live_target/SSTV_SAMPLE_RATE,live_written);
                }
            }
        }
        pthread_mutex_unlock(&live_mutex); return;
    }

    for (unsigned int i=0;i<n;++i) {
        live_ring[live_ring_pos]=samples[i];
        live_ring_pos=(live_ring_pos+1)%SSTV_PREBUF_SAMPLES;
        if (live_ring_count<SSTV_PREBUF_SAMPLES) ++live_ring_count;
    }
    live_since_check += n;
    live_wait_samples += n;
    if (live_wait_samples - live_wait_log_mark >= (uint64_t)SSTV_SAMPLE_RATE * 60u) {
        live_wait_log_mark = live_wait_samples;
        fprintf(stderr,"SSTV LIVE: still listening - %llu s of RX audio checked, no valid VIS yet\n",
                (unsigned long long)(live_wait_samples/SSTV_SAMPLE_RATE));
    }
    if (live_ring_count >= SSTV_VIS_WINDOW_SAMPLES && live_since_check >= SSTV_VIS_CHECK_STEP) {
        live_since_check=0;
        size_t start=(live_ring_pos + SSTV_PREBUF_SAMPLES - SSTV_VIS_WINDOW_SAMPLES)%SSTV_PREBUF_SAMPLES;
        for (size_t i=0;i<SSTV_VIS_WINDOW_SAMPLES;++i) live_vis_window[i]=live_ring[(start+i)%SSTV_PREBUF_SAMPLES];
        if (live_find_vis(live_vis_window,SSTV_VIS_WINDOW_SAMPLES,&live_info)) {
            /* Prepare RX/TX Mode for a reply using the detected VIS mode.
             * Scottie DX is RX-only here, so keep the previous TX mode for it. */
            switch (live_info.mode) {
                case SSTV_MODE_ROBOT8_BW:
                case SSTV_MODE_ROBOT36:
                case SSTV_MODE_ROBOT72:
                case SSTV_MODE_MARTIN_M1:
                case SSTV_MODE_MARTIN_M2:
                case SSTV_MODE_SCOTTIE_S1:
                case SSTV_MODE_SCOTTIE_S2:
                    cont_mode=live_info.mode;
                    live_mode_ui_pending=true;
                    break;
                default:
                    break;
            }
            live_target=(long)((live_mode_seconds(live_info.mode)+2.0f)*SSTV_SAMPLE_RATE);
            live_samples=malloc((size_t)live_target*sizeof(float));
            if (!live_samples) {
                fprintf(stderr,"SSTV LIVE: ERROR allocating %.1f MB capture buffer\n",
                        (double)live_target*sizeof(float)/(1024.0*1024.0));
                live_enabled=false;
            } else {
                size_t keep=live_ring_count;
                if ((long)keep>live_target) keep=(size_t)live_target;
                size_t oldest=(live_ring_pos + SSTV_PREBUF_SAMPLES - live_ring_count)%SSTV_PREBUF_SAMPLES;
                for (size_t i=0;i<keep;++i) live_samples[i]=live_ring[(oldest+i)%SSTV_PREBUF_SAMPLES];
                live_written=(long)keep;
                live_capturing=true;
                live_preview_start();
                if (live_restart_enabled) {
                    /* Start the parallel detector from audio AFTER this VIS. */
                    live_ring_pos=0;
                    live_ring_count=0;
                    live_since_check=0;
                }
                /* A valid VIS starts a new picture.  Do not touch LVGL from
                 * the audio callback; ask the UI timer to clear the canvas. */
                live_canvas_clear_pending = true;
                fprintf(stderr,"SSTV LIVE: capturing %s directly to RAM; target %.1f s, buffer %.1f MB, prebuffer=%ld samples\n",
                        live_info.name,(double)live_target/SSTV_SAMPLE_RATE,
                        (double)live_target*sizeof(float)/(1024.0*1024.0),live_written);
            }
        }
    }
    pthread_mutex_unlock(&live_mutex);
}

static void tx_ui_cb(button_data_t *data)
{
    (void)data;

    if (tx_ui_state == SSTV_TX_UI_RF) {
        tx_abort = true;
        set_status("Stopping SSTV TX...");
        return;
    }

    if (tx_ui_state == SSTV_TX_UI_SETUP) {
        tx_color_selector_destroy();
        tx_template_selector_destroy();
        tx_type_selector_destroy();
        tx_picture_selector_destroy();
        tx_instruction_hide();
        tx_phase = SSTV_TX_PHASE_NONE;
        if (status_label)
            lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
        tx_ui_state = SSTV_TX_UI_RX;

        /* TX selectors own keyboard focus while the TX setup is open.
         * On Cancel TX restore the normal RX focus, otherwise VOL events
         * no longer reach key_cb() even though the RX screen is visible. */
        lv_group_add_obj(keyboard_group, sstv_canvas);
        lv_group_focus_obj(sstv_canvas);
        lv_group_set_editing(keyboard_group, true);

        btn_tx.label = "TX";
        snprintf(tx_mode_label, sizeof(tx_mode_label), "RX Mode\n%s", cont_rx_mode_name());
        btn_cont_mode.label = tx_mode_label;

        if (tx_restore_auto_rx) {
            pthread_mutex_lock(&live_mutex);
            live_enabled = true; live_capturing = false; live_decode_pending = false;
            live_ring_pos = live_ring_count = 0; live_since_check = 0;
            live_wait_samples = live_wait_log_mark = 0;
            free(live_samples); live_samples = NULL; live_written = live_target = 0;
            pthread_mutex_unlock(&live_mutex);
            set_receiver_ui_listening();
        } else {
            set_receiver_ui_stopped();
        }
        tx_restore_rx_canvas();
        if (btn_tx.disp_btn) buttons_refresh(&btn_tx);
        if (btn_cont_mode.disp_btn) buttons_refresh(&btn_cont_mode);
        update_rx_controls();
        fprintf(stderr, "SSTV TX UI: setup cancelled; RX restored%s\n",
                tx_restore_auto_rx ? " with Auto RX" : " to stand-by");
        return;
    }

    /* Preserve the exact RX pixels before TX preview changes the canvas. */
    tx_save_rx_canvas();

    /* Remember only whether Auto RX was active.  Manual RX is abandoned
     * without scheduling a decode: TX has priority. */
    tx_restore_auto_rx = live_enabled || live_capturing || live_decode_pending || live_decoding;
    pthread_mutex_lock(&live_mutex);
    live_enabled = false; live_capturing = false; live_decode_pending = false;
    live_ring_pos = live_ring_count = 0; live_since_check = 0;
    live_wait_samples = live_wait_log_mark = 0;
    free(live_samples); live_samples = NULL; live_written = live_target = 0;
    cont_capturing = false; cont_decode_pending = false;
    free(cont_samples); cont_samples = NULL; cont_written = cont_capacity = 0;
    pthread_mutex_unlock(&live_mutex);

    tx_ui_state = SSTV_TX_UI_SETUP;
    tx_phase = SSTV_TX_PHASE_SELECT_TYPE;
    btn_tx.label = "Cancel TX";
    snprintf(tx_mode_label, sizeof(tx_mode_label), "TX Mode\n%s", cont_mode_name(cont_mode));
    btn_cont_mode.label = tx_mode_label;
    set_status("TX setup\n\nChoose TX action");
    if (progress_bar) {
        lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    if (btn_tx.disp_btn) buttons_refresh(&btn_tx);
    if (btn_cont_mode.disp_btn) buttons_refresh(&btn_cont_mode);
    update_rx_controls();
    tx_type_selector_build();
    fprintf(stderr, "SSTV TX UI: entered setup at action selector; previous Auto RX=%d\n", tx_restore_auto_rx ? 1 : 0);
}

/* Web remote: sstv_tx sends a picture of SSTV_TX_PICS_DIR as IMAGE ONLY, val is the
 * index in cont_modes[] */
void dialog_sstv_remote(const char *cmd, int32_t val, const char *text)
{
    if (!strcmp(cmd, "sstv_stop")) {
        if (tx_ui_state == SSTV_TX_UI_RF) tx_abort = true;
        return;
    }
    if (!strcmp(cmd, "app_base")) {   /* the closest calling frequency, as its Freq. list */
        const freq_list_item_t *it = freq_list_base(sstv_freqs, sizeof(sstv_freqs)/sizeof(sstv_freqs[0]), 0,
                                                    cparam_i_get(cfg_fg_freq), NULL);
        if (it) freq_list_tune(it, 0);
        return;
    }
    if (strcmp(cmd, "sstv_tx") || val < 0 || val >= (int32_t)(sizeof(cont_modes)/sizeof(cont_modes[0])))
        return;
    if (tx_ui_state != SSTV_TX_UI_RX || tx_running) {
        msg_update_text_fmt("SSTV TX busy");
        return;
    }
    if (sstv_browser_open) sstv_browser_close();
    cont_mode = cont_modes[val];
    tx_ui_cb(NULL);                 /* into the TX setup, as the TX button */
    tx_type_selector_destroy();
    tx_message_type = SSTV_TX_MESSAGE_IMAGE_ONLY;
    if (!tx_picture_load_and_prepare(text)) {
        msg_update_text_fmt("SSTV TX: can't load %s", text);
        tx_ui_cb(NULL);             /* cancel */
        return;
    }
    snprintf(tx_last_picture_file, sizeof(tx_last_picture_file), "%s", text);
    tx_image_only_start();
}

int dialog_sstv_remote_state(char *buf, size_t size)
{
    sstv_tx_ui_state_t ui = tx_ui_state;
    const char *rx = "OFF";
    long w = 0, n = 0;

    if (live_decoding || live_decode_pending || cont_decoding || cont_decode_pending) {
        rx = "DECODE";
    } else if (live_capturing) {
        rx = "CAPTURE"; w = live_written; n = live_target;
    } else if (cont_capturing) {
        rx = "CAPTURE"; w = cont_written; n = cont_capacity;
    } else if (live_enabled) {
        rx = "LISTEN";
    }
    int prog = ui == SSTV_TX_UI_RF ? tx_progress : n > 0 ? (int)(w * 1000 / n) : 0;
    int32_t base = 0;
    freq_list_base(sstv_freqs, sizeof(sstv_freqs)/sizeof(sstv_freqs[0]), 0, cparam_i_get(cfg_fg_freq), &base);
    return snprintf(buf, size, "{\"ui\":\"%s\",\"rx\":\"%s\",\"vis\":\"%s\",\"prog\":%i,\"base\":%i}",
                    ui == SSTV_TX_UI_RF ? "TX" : ui == SSTV_TX_UI_SETUP ? "SETUP" : "RX", rx,
                    cont_mode_name(ui == SSTV_TX_UI_RF ? tx_mode : cont_mode), prog < 0 ? 0 : prog > 1000 ? 1000 : prog, (int)base);
}
