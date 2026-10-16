/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "dialog_ft8.h"

#include "ft8/worker.h"
#include "ft8/qso.h"
#include "ft8/utils.h"
#include "lvgl/lvgl.h"
#include "dialog.h"
#include "dialog_field.h"
#include "styles.h"
#include "params/params.h"
#include "cfg/cfg_api.h"
#include "cfg/digital_modes.h"
#include "radio.h"
#include "audio.h"
#include "dsp.h"
#include "keyboard.h"
#include "events.h"
#include "buttons.h"
#include "main_screen.h"
#include "freq_strip.h"
#include "qth/qth.h"
#include "station.h"
#include "msg.h"
#include "util.h"
#include "recorder.h"
#include "textarea_window.h"
#include "dsp.h"

#include "ft8/audio_worker.h"
#include "ft8/ft2.h"
#include "ft8/cq_scheduler.h"
#include "ft8/table_view.h"
#include "ft8_udp.h"
#include "dxcc.h"
#include "js8_map.h"
#include "main.h"
#include "pskreporter.h"
#include "flags.h"
#include "ft8/tx_worker.h"
#include "ft8/hound.h"
#include "tx_player.h"
#include "widgets/lv_waterfall.h"
#include "widgets/lv_finder.h"

#include "ft8/worker.h"
#include "adif.h"
#include "qso_log.h"
#include "remote_server.h"
#include "scheduler.h"
#include "theme.h"
#include "cluster/client.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <strings.h>

#define SAMPLE_RATE     (AUDIO_CAPTURE_RATE / AUDIO_DECIM)

#define WIDTH           771
#define WF_HEIGHT       (DIALOG_H - 26)   /* 13 px margins above and below */
#define WF_VISIBLE      40     /* waterfall clear above the log: ~16 s, a slot, to find a clear spot */

#define UNKNOWN_SNR     99

#define MAX_PWR         TX_PLAYER_MAX_PWR_W

#define FT8_WIDTH_HZ    50
#define FT4_WIDTH_HZ    83
#define FT2_WIDTH_HZ    167

#define MAX_TX_START_DELAY 1.5f

#define CUSTOM_MIN_HZ    1800000  /* custom dial frequency: 160m ... */
#define CUSTOM_MAX_HZ    54000000 /* ... to the top of 6m */

/* ---- FT8 / FT4 / FT2 (FT2 from SP2KMO's upstream 76d98ce2) ---------------
 * cfg_ft8_protocol: FTX_PROTOCOL_FT4 (0), FTX_PROTOCOL_FT8 (1) or FTX_PROTOCOL_FT2 (2, ft8/ft2.h) */

static const char *ftx_mode_name(void) {
    return ftx_protocol_name(param_i_get(cfg_ft8_protocol));
}

static bool ftx_is_ft2(void) {
    return param_i_get(cfg_ft8_protocol) == (int)FTX_PROTOCOL_FT2;
}

static int ftx_width_hz(void) {
    switch (param_i_get(cfg_ft8_protocol)) {
        case FTX_PROTOCOL_FT8: return FT8_WIDTH_HZ;
        case FTX_PROTOCOL_FT4: return FT4_WIDTH_HZ;
        default:               return FT2_WIDTH_HZ;
    }
}

/* A transmission may start this late into its slot: FT2 lasts 2.52 s and must end inside 3.75 s */
static float ftx_max_tx_start(void) {
    return ftx_is_ft2() ? 0.8f : MAX_TX_START_DELAY;
}

static qso_log_mode_t ftx_log_mode(void) {
    switch (param_i_get(cfg_ft8_protocol)) {
        case FTX_PROTOCOL_FT8: return MODE_FT8;
        case FTX_PROTOCOL_FT4: return MODE_FT4;
        default:               return MODE_FT2;
    }
}

typedef enum {
    RX_PROCESS,
    TX_PROCESS,
} ft8_state_t;

/* ft8_cell_type_t and cell_data_t live in ft8/table_view.h */

/* slot_info_t lives in ft8/audio_worker.h */

static ft8_state_t state = RX_PROCESS;
static SubjectInt *tx_enabled;
static SubjectInt *cq_enabled;
static bool        tx_time_slot;
/* UTC start of the slot being received, "HH:MM:SS": in front of each row of it (worker thread) */
static char        slot_time[9] = "--:--:--";

static ftx_tx_msg_t tx_msg;

/* DXpedition Hound mode (FT8 only, off each time the app opens) */
static bool    hound_on;
static hound_t hound;

/* The lv_table widget is owned by ft8/table_view; expose its handle as
 * `table` so existing fade/group/anim call sites need no rename. */
#define table (table_view_obj())

static lv_timer_t *timer = NULL;
static lv_anim_t   fade;
static bool        fade_run        = false;
static bool        disable_buttons = false;

static lv_obj_t *finder;
static lv_obj_t *waterfall;

/* Audio capture, decimation, FFT, decoder thread and stop-flag plumbing
 * all live in ft8/audio_worker.c. The dialog just creates/destroys the
 * worker and exposes a handful of callbacks. */
static audio_worker_t *audio_worker = NULL;

static adif_log         ft8_log;
static FTxQsoProcessor *qso_processor;

static double cur_lat, cur_lon;

static int32_t filter_low, filter_high;

static float base_gain_offset;

/* Decodes the web remote can answer (ft8_reply), by the id of their ft8_rx event */
#define REMOTE_RX_N 64

static struct {
    int32_t        id;
    bool           odd;
    ftx_msg_meta_t meta;
} remote_rx[REMOTE_RX_N];
static int32_t         remote_rx_id;
static pthread_mutex_t remote_rx_mutex = PTHREAD_MUTEX_INITIALIZER;

static void construct_cb(lv_obj_t *parent);
static void key_cb(lv_event_t * e);
static void destruct_cb();
static void audio_cb(unsigned int n, float *samples);
static void rotary_cb(int32_t diff);

/* audio_worker callbacks (run on worker thread). UI mutations must go
 * through scheduler_put() to land on the LVGL task. */
static void on_message_cb(const char *text, int snr, float freq_hz, float time_sec,
                          const slot_info_t *info, void *ctx);
static void on_psd_cb(const float *psd, uint16_t nfft, float sec_since_slot_start,
                      const slot_info_t *info, void *ctx);
static void on_slot_end_cb(const slot_info_t *info, void *ctx);
static void on_tick_cb(const slot_info_t *info, bool new_slot,
                       float sec_since_slot_start, void *ctx);

static const char * cq_all_label_getter();
static const char * protocol_label_getter();
static const char * tx_cq_label_getter();
static const char * tx_call_label_getter();
static const char * hold_freq_label_getter();
static const char * auto_label_getter();

static void show_cq_all_cb(struct button_data_t *btn_data);
static void mode_ft4_ft8_cb(struct button_data_t *btn_data);
static void tx_cq_en_dis_cb(struct button_data_t *btn_data);
static void tx_call_en_dis_cb(struct button_data_t *btn_data);

static void hold_tx_freq_cb(struct button_data_t *btn_data);
static void mode_auto_cb(struct button_data_t *btn_data);
static void cq_modifier_cb(struct button_data_t *btn_data);
static void time_sync(struct button_data_t *btn_data);

static void force_save_qso(struct button_data_t *btn_data);

static void on_table_press(const cell_data_t *cell_data);
static void on_table_close(void);
static void on_table_vol_change(int32_t delta);

static void keyboard_open();
static bool keyboard_cancel_cb();
static bool keyboard_ok_cb();
static void keyboard_close();

static void add_info(const char * fmt, ...);
static void add_tx_text(const char * text);
static bool get_time_slot(struct timespec now, float *time_since_start);


// button label is current state, press action and name - next state

static buttons_page_t btn_page_1;
static buttons_page_t btn_page_2;
static buttons_page_t btn_page_3;

static void map_cb(struct button_data_t *btn_data);
static void heard_map_cb(struct button_data_t *btn_data);
static lv_obj_t *map_obj;   /* QSO or heard map, when open */
static lv_obj_t *freq_list; /* Freq: the presets, when open */

static buttons_page_t btn_page_4;

static const char * freq_label_getter();
static const char * hound_label_getter();
static const char * retries_label_getter();
static void freq_cb(struct button_data_t *btn_data);
static void freq_hold_cb(struct button_data_t *btn_data);
static void hound_cb(struct button_data_t *btn_data);
static void retries_cb(struct button_data_t *btn_data);

static button_data_t button_page_1 = { .type=BTN_TEXT, .label = "(FT8 1:4)", .press = button_next_page_cb, .next=&btn_page_2};
static button_data_t button_show_cq_all = { .type=BTN_TEXT_FN, .label_fn = cq_all_label_getter, .press = show_cq_all_cb};
static button_data_t button_mode_ft4_ft8 = { .type=BTN_TEXT_FN, .label_fn = protocol_label_getter, .press = mode_ft4_ft8_cb};
static button_data_t button_tx_cq_en_dis = { .type=BTN_TEXT_FN, .label_fn = tx_cq_label_getter, .press = tx_cq_en_dis_cb };
static button_data_t button_tx_call_en_dis = { .type=BTN_TEXT_FN, .label_fn = tx_call_label_getter, .press = tx_call_en_dis_cb};

static button_data_t button_page_2 = { .type=BTN_TEXT, .label = "(FT8 2:4)", .press = button_next_page_cb, .next=&btn_page_3};
static button_data_t button_hold_freq = { .type=BTN_TEXT_FN, .label_fn = hold_freq_label_getter, .press = hold_tx_freq_cb };
static button_data_t button_auto_en_dis = { .type=BTN_TEXT_FN, .label_fn = auto_label_getter, .press = mode_auto_cb };
static button_data_t button_force_save = { .type=BTN_TEXT, .label = "Force QSO\nsave", .press = force_save_qso };

static button_data_t button_page_3 = { .type=BTN_TEXT, .label = "(FT8 3:4)", .press = button_next_page_cb, .next=&btn_page_4};
static button_data_t button_cq_mod = { .type=BTN_TEXT, .label = "CQ\nModifier", .press = cq_modifier_cb };
static button_data_t button_time_sync = { .type=BTN_TEXT, .label = "Time\nSync", .press = time_sync };
static button_data_t button_map = { .type=BTN_TEXT, .label = "Log\nMap", .press = map_cb };
static button_data_t button_heard_map = { .type=BTN_TEXT, .label = "Live\nMap", .press = heard_map_cb };

static button_data_t button_page_4 = { .type=BTN_TEXT, .label = "(FT8 4:4)", .press = button_next_page_cb, .next=&btn_page_1};
static button_data_t button_freq = { .type=BTN_TEXT_FN, .label_fn = freq_label_getter, .press = freq_cb, .hold = freq_hold_cb };
static button_data_t button_hound = { .type=BTN_TEXT_FN, .label_fn = hound_label_getter, .press = hound_cb };
static button_data_t button_retries = { .type=BTN_TEXT_FN, .label_fn = retries_label_getter, .press = retries_cb };

static buttons_page_t btn_page_1 = {
    {&button_page_1, &button_show_cq_all, &button_mode_ft4_ft8, &button_tx_cq_en_dis, &button_tx_call_en_dis}
};

static buttons_page_t btn_page_2 = {
    {&button_page_2, &button_hold_freq, &button_auto_en_dis, &button_force_save, &btn_dig_pwr}
};

static buttons_page_t btn_page_3 = {
    {&button_page_3, &button_cq_mod, &button_time_sync, &button_map, &button_heard_map}
};

static buttons_page_t btn_page_4 = {
    {&button_page_4, &button_freq, &button_hound, &button_retries, NULL}
};

static dialog_t dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = audio_cb,
    .rotary_cb = rotary_cb,
    .key_cb = key_cb,
    .restore_radio = true,
};

dialog_t *dialog_ft8 = &dialog;

/* ---- WSJT-X datagrams over UDP (ft8_udp.c, from SP2KMO's upstream PR #275) ----
 * Status whenever something a logger shows changes (RX/TX, audio offset, dial, the station
 * worked) and with each heartbeat: one 250 ms timer compares and sends. */

#define UDP_POLL_MS         250
#define UDP_HEARTBEAT_POLLS 60      /* 15 s, as WSJT-X */

static lv_timer_t *udp_timer;
static uint32_t    udp_polls;
static int         udp_last_state = -1;
static int32_t     udp_last_df    = -1;
static int64_t     udp_last_freq  = -1;
static char        udp_last_dx[16];

static const char *udp_mode_str(void) {
    return ftx_mode_name();
}

/* The station worked: the pending TX message starts with it ("DXCALL MYCALL ..."), but CQ */
static void udp_current_dx_call(char *out, size_t out_sz) {
    size_t i = 0;

    while (tx_msg.msg[i] && tx_msg.msg[i] != ' ' && i < out_sz - 1) {
        out[i] = tx_msg.msg[i];
        i++;
    }
    out[i] = '\0';
    if (strcmp(out, "CQ") == 0) out[0] = '\0';
}

static void udp_status_send(void) {
    char     dx_call[16];
    uint32_t df = params.ft8_tx_freq.x;

    udp_current_dx_call(dx_call, sizeof(dx_call));
    ft8_udp_send_status((uint64_t)cparam_i_get(cfg_fg_freq), udp_mode_str(), params.callsign.x, station_qth(),
                        dx_call, df, df, state == TX_PROCESS, state != TX_PROCESS);
    udp_last_state = (int)state;
    udp_last_df    = (int32_t)df;
    udp_last_freq  = (int64_t)cparam_i_get(cfg_fg_freq);
    snprintf(udp_last_dx, sizeof(udp_last_dx), "%s", dx_call);
}

static void udp_timer_cb(lv_timer_t *t) {
    char dx_call[16];

    (void)t;
    if (++udp_polls >= UDP_HEARTBEAT_POLLS) {
        udp_polls = 0;
        ft8_udp_send_heartbeat();
        udp_status_send();
        return;
    }
    udp_current_dx_call(dx_call, sizeof(dx_call));
    if ((int)state != udp_last_state || (int32_t)params.ft8_tx_freq.x != udp_last_df ||
        (int64_t)cparam_i_get(cfg_fg_freq) != udp_last_freq || strcmp(dx_call, udp_last_dx) != 0) {
        udp_status_send();
    }
}

static void save_qso(const char *remote_callsign, const char *remote_grid, const int r_snr, const int s_snr) {
    time_t now = time(NULL);

    char * canonized_call = util_canonize_callsign(remote_callsign, false);
    qso_log_record_t qso = qso_log_record_create(
        params.callsign.x,
        canonized_call,
        now, ftx_log_mode(),
        s_snr, r_snr, cparam_i_get(cfg_fg_freq), NULL, NULL,
        station_qth(), remote_grid
    );
    free(canonized_call);

    adif_add_qso(ft8_log, qso);

    // Save QSO to sqlite log
    qso_log_record_save(qso);
    dialog_field_day_log(&qso);
    ft8_udp_log_qso(now, qso.remote_call, remote_grid, (uint64_t)cparam_i_get(cfg_fg_freq), udp_mode_str(), s_snr,
                    r_snr, (int)param_f_get(cfg_pwr), params.callsign.x, station_qth());

    if (strlen(remote_grid) >= 4) {
        double lat, lon, dist;
        qth_str_to_pos(remote_grid, &lat, &lon);
        dist = qth_pos_dist(lat, lon, cur_lat, cur_lon);
        msg_schedule_long_text_fmt("Saved QSO de %s %d %d (%.0f km)", remote_callsign, s_snr, r_snr, dist);
    } else {
        msg_schedule_long_text_fmt("Saved QSO de %s %d %d", remote_callsign, s_snr, r_snr);
    }

    lv_finder_clear_cursor(finder);

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "t", "ft8_qso");
    cJSON_AddStringToObject(o, "call", remote_callsign);
    cJSON_AddStringToObject(o, "grid", remote_grid);
    cJSON_AddNumberToObject(o, "sent", s_snr);
    cJSON_AddNumberToObject(o, "rcvd", r_snr);
    remote_event(o);
}

/* Activators spotted on POTA now: the cluster store, which x6100-pota-spots feeds (worker thread only) */
static bool pota_spotted(const char *call) {
    static cluster_spot_t spots[CLUSTER_MAX_SPOTS];
    static int            n;
    static uint32_t       gen;
    int                   k = cluster_client_snapshot(spots, CLUSTER_MAX_SPOTS, &gen);

    if (k >= 0) n = k;
    for (int i = 0; i < n; i++)
        if (!strcasecmp(spots[i].dx, call) && (cluster_spot_activity(&spots[i]) & CLUSTER_ACT_POTA)) return true;
    return false;
}

static void worker_init() {
    qso_processor = ftx_qso_processor_init(params.callsign.x, station_qth(),
                                           save_qso,
                                           param_i_get(cfg_ft8_max_repeats));
    ftx_qso_processor_set_is_pota(qso_processor, pota_spotted);

    audio_worker_cb_t cb = {
        .on_message  = on_message_cb,
        .on_psd      = on_psd_cb,
        .on_slot_end = on_slot_end_cb,
        .on_tick     = on_tick_cb,
        .ctx         = NULL,
    };
    audio_worker = audio_worker_create(
        SAMPLE_RATE,
        param_i_get(cfg_ft8_protocol),
        filter_low, filter_high,
        &cb);
    if (audio_worker) {
        audio_worker_start(audio_worker);
    }
}

static void worker_done() {
    state = RX_PROCESS;

    if (audio_worker) {
        audio_worker_destroy(audio_worker);
        audio_worker = NULL;
    }
    radio_set_modem(false);

    if (qso_processor) {
        ftx_qso_processor_delete(qso_processor);
        qso_processor = NULL;
    }
    lv_finder_clear_cursor(finder);
    tx_msg.msg[0] = '\0';

    pthread_mutex_lock(&remote_rx_mutex);   /* other band or protocol */
    memset(remote_rx, 0, sizeof(remote_rx));
    pthread_mutex_unlock(&remote_rx_mutex);
}

/* Table widget lifecycle, draw, scroll and message insertion all live
 * in ft8/table_view.c. The dialog only owns the surrounding state. */

/* QSO map: the world, the station and a path to each QSO of the FT8/FT4 log (js8_map.c) */
static void map_close(void) {
    if (!map_obj) return;
    lv_obj_del_async(map_obj);
    map_obj = NULL;
    lv_group_add_obj(keyboard_group, table);
    lv_group_focus_obj(table);
    lv_group_set_editing(keyboard_group, true);
}

static void map_key_cb(lv_event_t *e) {
    if (*((uint32_t *)lv_event_get_param(e)) == LV_KEY_ESC) map_close();
}

/* What the heard map keeps of a decode: enough to call it as a press on its row does */
typedef struct {
    ftx_msg_meta_t meta;
    bool           odd;
} map_rx_t;

_Static_assert(sizeof(map_rx_t) <= sizeof(((js8_map_station_t *)0)->data), "map_rx_t too big for the map");

static void on_table_press(const cell_data_t *cell_data);

/* MFK press on a station of the Live Map: back to the list and call it, as from its row */
static void map_pick(const js8_map_station_t *st) {
    const map_rx_t *rx = (const map_rx_t *)st->data;
    cell_data_t     cd = {.cell_type = CELL_RX_MSG, .odd = rx->odd, .meta = rx->meta};
    map_close();
    on_table_press(&cd);
}

/* Either map button closes the map that is open */
static void map_open(bool heard) {
    if (map_obj) {
        map_close();
        return;
    }
    map_obj = heard ? js8_map_create(dialog.obj, WIDTH, WF_HEIGHT, station_qth(), "/mnt/ft_log.adi",
                                     js8_map_heard_fill, false, map_pick)
                    : js8_map_create(dialog.obj, WIDTH, WF_HEIGHT, station_qth(), "/mnt/ft_log.adi", NULL, true, NULL);
    if (!map_obj) {
        msg_update_text_fmt("Not enough memory for the map");
        return;
    }
    lv_obj_set_pos(map_obj, 13, 13);
    lv_group_remove_obj(table);
    lv_obj_add_event_cb(map_obj, map_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, map_obj);
    lv_group_focus_obj(map_obj);
    lv_group_set_editing(keyboard_group, true);   /* the MFK steps through the stations */
}

static void map_cb(struct button_data_t *btn_data) {
    (void)btn_data;
    map_open(false);
}

/* The stations decoded in the last 15 minutes, from their grid */
static void heard_map_cb(struct button_data_t *btn_data) {
    (void)btn_data;
    map_open(true);
}

static void key_cb(lv_event_t * e) {
    uint32_t key = *((uint32_t *) lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct(&dialog);
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

static void destruct_cb() {
    // TODO: check free mem
    map_obj = NULL;     /* deleted with the dialog */
    freq_list = NULL;
    keyboard_close();
    worker_done();
    /* After the decoder thread, which sends too */
    if (udp_timer) {
        lv_timer_del(udp_timer);
        udp_timer = NULL;
    }
    ft8_udp_deinit();
    table_view_destroy();

    dsp_set_waterfall_enabled(true);
    dsp_set_spectrum_enabled(true);


    main_screen_lock_mode(false);
    main_screen_lock_ab(false);
    main_screen_lock_freq(false);
    freq_strip_set_offset(-1);
    main_screen_lock_band(false);

    radio_set_pwr(param_f_get(cfg_pwr));
    adif_log_close(ft8_log);
}

static bool load_band(int8_t dir) {
    cfg_digital_type_t type;
    switch (param_i_get(cfg_ft8_protocol)) {
        case FTX_PROTOCOL_FT8:
            type = CFG_DIG_TYPE_FT8;
            lv_finder_set_width(finder, FT8_WIDTH_HZ);
            break;

        case FTX_PROTOCOL_FT4:
            type = CFG_DIG_TYPE_FT4;
            lv_finder_set_width(finder, FT4_WIDTH_HZ);
            break;

        default:    /* FT2 */
            type = CFG_DIG_TYPE_FT2;
            lv_finder_set_width(finder, FT2_WIDTH_HZ);
            break;
    }
    bool res = cfg_digital_load(dir, type);
    if (res) {
        msg_update_text_fmt("%s", cfg_digital_label_get());
    }
    return res;
}

/* "14.091" from 14091000 Hz: MHz, 3 decimals at least */
static void format_mhz(int32_t hz, char *buf, size_t size) {
    snprintf(buf, size, "%d.%06d", (int)(hz / 1000000), (int)(hz % 1000000));
    char *end = buf + strlen(buf) - 1;
    while (*end == '0' && end > strchr(buf, '.') + 3) *end-- = '\0';
}

/* What the info rows call where we are: the preset's label, or "FT8 Custom 14.091" */
static const char *where_label(void) {
    static char buf[40];
    if (!params.ft8_custom_on.x) return cfg_digital_label_get();
    char mhz[16];
    format_mhz(params.ft8_custom_hz.x, mhz, sizeof(mhz));
    snprintf(buf, sizeof(buf), "%s Custom %s", ftx_mode_name(), mhz);
    return buf;
}

/* The band's preset, or the custom frequency (in the preset's mode) */
static void load_base(void) {
    load_band(0);
    if (!params.ft8_custom_on.x) return;
    if (params.ft8_custom_hz.x < CUSTOM_MIN_HZ || params.ft8_custom_hz.x > CUSTOM_MAX_HZ) {
        params_bool_set(&params.ft8_custom_on, false);
        return;
    }
    int32_t mode = cparam_i_get(cfg_cur_mode);
    cparam_i_set(cfg_fg_freq, params.ft8_custom_hz.x);
    if (cparam_i_get(cfg_cur_mode) != mode) cparam_i_set(cfg_cur_mode, mode); /* another band loads its own mode */
    msg_update_text_fmt("%s", where_label());
}

/// @brief Clean waterfall and table
static void clean_screen() {
    table_view_reset();
    lv_waterfall_clear_data(waterfall);

    int32_t *c = malloc(sizeof(int32_t));
    *c = LV_KEY_UP;

    lv_event_send(table, LV_EVENT_KEY, c);
}

/* After any change of dial frequency */
static void retuned(void) {
    hound_stop(&hound);
    js8_map_heard_clear();

    worker_done();
    worker_init();
    clean_screen();
}

static void band_cb(lv_event_t * e) {
    int8_t dir;

    if (lv_event_get_code(e) == EVENT_BAND_UP) {
        dir = 1;
    } else {
        dir = -1;
    }

    if (!load_band(dir)) return;
    if (params.ft8_custom_on.x) params_bool_set(&params.ft8_custom_on, false); /* back to the presets */
    retuned();
}

static void msg_timer(lv_timer_t *t) {
    lv_anim_set_values(&fade, lv_obj_get_style_opa_layered(table, 0), LV_OPA_COVER);
    lv_anim_start(&fade);
    timer = NULL;
}

static void fade_anim(void * obj, int32_t v) {
    lv_obj_set_style_opa_layered(obj, v, 0);
}

static void fade_ready(lv_anim_t * a) {
    fade_run = false;
}

static void set_freq(uint32_t freq) {
    if (freq > filter_high) {
        freq = filter_high;
    }

    if (freq < filter_low) {
        freq = filter_low;
    }

    params_uint16_set(&params.ft8_tx_freq, freq);

    lv_finder_set_value(finder, freq);
    lv_obj_invalidate(finder);
    freq_strip_set_offset(freq);
}

static void rotary_cb(int32_t diff) {
    /* Faster turns move further: 4-5 clicks within 30 ms move 5 Hz each, 6 or more
     * 10 Hz. The knob's detents arrive batched per GUI read (10 ms), so a single
     * diff almost never got past 3: count the clicks per 30 ms, as the JS8 app
     * does (from randal007's upstream ft8-knob-speed b2c3e1ac) */
    static uint32_t window_ms;
    static uint32_t window_clicks;

    if (lv_tick_elaps(window_ms) >= 30) {
        window_ms     = lv_tick_get();
        window_clicks = 0;
    }
    if (map_obj) {   /* the map is open: it zooms */
        js8_map_zoom(diff);
        return;
    }
    window_clicks += abs(diff);
    if (window_clicks > 3) {
        diff *= (window_clicks < 6) ? 5 : 10;
    }
    uint32_t f = params.ft8_tx_freq.x + diff;
    f = limit(f, filter_low, filter_high - ftx_width_hz());

    set_freq(f);

    if (!fade_run) {
        fade_run = true;
        lv_anim_set_values(&fade, lv_obj_get_style_opa_layered(table, 0), LV_OPA_TRANSP);
        lv_anim_start(&fade);
    }

    if (timer) {
        lv_timer_reset(timer);
    } else {
        timer = lv_timer_create(msg_timer, 1000, NULL);
        lv_timer_set_repeat_count(timer, 1);
    }

}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    /* FT8 dialog owns the full screen + audio pipe; main_screen's spectrum
     * and waterfall are not visible, so skip their DSP cost entirely. The
     * companion lv_obj_invalidate() in tx_info handles the side effect of
     * losing the spectrum redraw that previously refreshed PWR/SWR bars. */
    dsp_set_waterfall_enabled(false);
    dsp_set_spectrum_enabled(false);

    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_UP, NULL);
    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_DOWN, NULL);

    // Fill subjects for buttons
    button_show_cq_all.subj = (Subject*)cfg_ft8_show_all;
    button_mode_ft4_ft8.subj = (Subject*)cfg_ft8_protocol;
    button_hold_freq.subj = (Subject*)cfg_ft8_hold_freq;
    button_auto_en_dis.subj = (Subject*)cfg_ft8_auto;
    button_retries.subj = (Subject*)cfg_ft8_max_repeats;

    if (!cq_enabled) {
        cq_enabled = subject_i_create(false);
        button_tx_cq_en_dis.subj = (Subject*)cq_enabled;
    } else {
        subject_i_set(cq_enabled, false);
    }
    if (!tx_enabled) {
        tx_enabled = subject_i_create(true);
        button_tx_call_en_dis.subj = (Subject*)tx_enabled;
    }
    buttons_load_page(&btn_page_1);

    /* Audio pipeline (cbuffer/firdecim/spgramcf/decoder thread) is created
     * lazily inside worker_init() -> audio_worker_create(). */

    /* Waterfall */

    waterfall = lv_waterfall_create(dialog.obj);

    lv_obj_add_style(waterfall, &waterfall_style, 0);
    lv_obj_clear_flag(waterfall, LV_OBJ_FLAG_SCROLLABLE);

    lv_waterfall_set_palette(waterfall, (lv_color_t*)wf_palette, 256);
    lv_waterfall_set_size(waterfall, WIDTH, WF_HEIGHT);
    lv_waterfall_set_min(waterfall, 0);    /* above the noise floor (on_psd_cb), as JS8 */
    lv_waterfall_set_max(waterfall, 30);

    lv_obj_set_pos(waterfall, 13, 13);

    /* Freq finder */

    finder = lv_finder_create(waterfall);

    lv_finder_set_width(finder, 50);
    lv_finder_set_value(finder, params.ft8_tx_freq.x);
    freq_strip_set_offset(params.ft8_tx_freq.x);

    lv_obj_set_size(finder, WIDTH, WF_HEIGHT);
    lv_obj_set_pos(finder, 0, 0);

    lv_obj_set_style_radius(finder, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(finder, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(finder, LV_OPA_0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(finder, bg_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(finder, LV_OPA_50, LV_PART_INDICATOR);

    lv_obj_set_style_border_width(finder, 1, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(finder, TC(text), LV_PART_INDICATOR);
    lv_obj_set_style_border_opa(finder, LV_OPA_50, LV_PART_INDICATOR);

    /* Table */

    table_view_build(dialog.obj, 13, 13 + WF_VISIBLE, WIDTH, WF_HEIGHT - WF_VISIBLE);   /* see-through: the others visible under it */
    table_view_set_press_cb(on_table_press);
    table_view_actions_t tv_actions = {
        .on_close      = on_table_close,
        .on_vol_change = on_table_vol_change,
    };
    table_view_set_actions(&tv_actions);

    /* Fade */

    lv_anim_init(&fade);
    lv_anim_set_var(&fade, table);
    lv_anim_set_time(&fade, 250);
    lv_anim_set_exec_cb(&fade, fade_anim);
    lv_anim_set_ready_cb(&fade, fade_ready);

    /* * */

    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);

    load_base();
    hound_on = false;
    memset(&hound, 0, sizeof(hound));

    filter_low = cparam_i_get(cfg_cur_filter_low);
    filter_high = cparam_i_get(cfg_cur_filter_high);

    lv_finder_set_range(finder, filter_low, filter_high);

    qth_str_to_pos(station_qth(), &cur_lat, &cur_lon);

    main_screen_lock_ab(true);
    main_screen_lock_mode(true);
    main_screen_lock_freq(true);
    main_screen_lock_band(true);

    worker_init();

    /* Logger */
    ft8_log = adif_log_init("/mnt/ft_log.adi");

    /* WSJT-X datagrams over UDP (GridTracker, JTAlert...): /mnt/ft8_udp.conf */
    ft8_udp_init();
    udp_polls      = 0;
    udp_last_state = -1;
    ft8_udp_send_heartbeat();
    udp_status_send();
    udp_timer = lv_timer_create(udp_timer_cb, UDP_POLL_MS, NULL);

    if (param_f_get(cfg_pwr) > MAX_PWR) {
        radio_set_pwr(MAX_PWR);
        msg_schedule_text_fmt("Power is limited to %0.0fW", MAX_PWR);
    }

    // setup gain offset
    base_gain_offset = tx_player_base_gain_offset();
}

/* Buttons */

static const char *freq_label_getter() {
    static char buf[32];
    if (!params.ft8_custom_on.x) return "Freq:\nPreset";
    char mhz[16];
    format_mhz(params.ft8_custom_hz.x, mhz, sizeof(mhz));
    snprintf(buf, sizeof(buf), "Freq:\n%s", mhz);
    return buf;
}

static const char *retries_label_getter() {
    static char buf[32];
    snprintf(buf, sizeof(buf), "Retries:\n%i", (int)param_i_get(cfg_ft8_max_repeats));
    return buf;
}

/* How many times a message goes out before the QSO is given up: the next step, round */
static void retries_cb(struct button_data_t *btn_data) {
    static const int steps[] = {3, 4, 5, 6, 8, 10, 12, 15};
    int              cur = param_i_get(cfg_ft8_max_repeats), next = steps[0];

    if (disable_buttons) return;
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++)
        if (steps[i] > cur) {
            next = steps[i];
            break;
        }
    param_i_set(cfg_ft8_max_repeats, next);
    ftx_qso_processor_set_max_repeats(qso_processor, next);
}

static const char *hound_label_getter() {
    return hound_on ? "Hound:\nOn" : "Hound:\nOff";
}

/* DXpedition Hound: on or off, a QSO or a CQ in progress stops */
static void hound_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    if (!hound_on && param_i_get(cfg_ft8_protocol) != FTX_PROTOCOL_FT8) {
        msg_schedule_text_fmt("Hound mode is for FT8 only");
        return;
    }
    hound_on = !hound_on;
    if (state == TX_PROCESS) state = RX_PROCESS;
    subject_i_set(cq_enabled, false);
    tx_msg.msg[0] = '\0';
    memset(&hound, 0, sizeof(hound));
    ftx_qso_processor_reset(qso_processor);
    lv_finder_clear_cursor(finder);
    buttons_refresh(&button_hound);
    if (hound_on) {
        add_info("Hound on: select the Fox's CQ and press, calls go above %d Hz", HOUND_MIN_TX_HZ);
        msg_schedule_text_fmt("Hound on: select the Fox's CQ and press");
    } else {
        add_info("Hound off");
    }
}

/* The dial frequency from the keyboard: kHz or MHz, empty for the presets */
static bool kb_freq;

/* Freq: the mode's preset frequencies, one for each band, from the digital_modes table */
static cfg_digital_preset_t presets[48];

static void freq_close(void) {
    if (!freq_list) return;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(freq_list); i++) lv_group_remove_obj(lv_obj_get_child(freq_list, i));
    lv_obj_del_async(freq_list);    /* called from one of its buttons */
    freq_list = NULL;
    lv_group_add_obj(keyboard_group, table);
    lv_group_focus_obj(table);
    lv_group_set_editing(keyboard_group, true);
}

static void preset_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);

    freq_close();
    if (i < 0) return;   /* Close */
    params_bool_set(&params.ft8_custom_on, false);
    cparam_i_set(cfg_fg_freq, presets[i].freq);
    load_band(0);
    retuned();
    buttons_refresh(&button_freq);
}

static void preset_key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    if (key == LV_KEY_ESC) freq_close();
    else if (key == LV_KEY_LEFT || key == LV_KEY_UP) lv_group_focus_prev(keyboard_group);
    else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) lv_group_focus_next(keyboard_group);
}

static void preset_focused_cb(lv_event_t *e) {
    lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_OFF);
}

static lv_obj_t *preset_add(const char *label, int i) {
    lv_obj_t *b = lv_list_add_btn(freq_list, NULL, label);
    lv_obj_set_style_bg_color(b, TC(row), 0);
    lv_obj_set_style_bg_color(b, TC(focus_bg), LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(b, TC(text), 0);
    lv_obj_set_style_pad_ver(b, 6, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(b, preset_focused_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(b, preset_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_add_event_cb(b, preset_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    return b;
}

static void freq_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    if (freq_list) {    /* Freq again closes it */
        freq_close();
        return;
    }
    if (state == TX_PROCESS) {
        msg_schedule_text_fmt("Not while sending");
        return;
    }
    if (map_obj) return;

    cfg_digital_type_t type = param_i_get(cfg_ft8_protocol) == FTX_PROTOCOL_FT8 ? CFG_DIG_TYPE_FT8
                            : param_i_get(cfg_ft8_protocol) == FTX_PROTOCOL_FT4 ? CFG_DIG_TYPE_FT4 : CFG_DIG_TYPE_FT2;
    int n = cfg_digital_list(type, presets, ARRAY_SIZE(presets));

    lv_group_remove_obj(table);
    freq_list = lv_list_create(dialog.obj);
    lv_obj_set_size(freq_list, 400, WF_HEIGHT - 10);
    lv_obj_align(freq_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(freq_list, &sony_24, 0);
    lv_obj_set_style_bg_color(freq_list, TC(list), 0);
    lv_obj_set_style_border_color(freq_list, TC(text), 0);
    char line[64];
    snprintf(line, sizeof(line), tr("%s frequencies - hold Freq to type"), ftx_mode_name());
    lv_obj_set_style_text_font(lv_list_add_text(freq_list, line), &sony_22, 0);

    lv_obj_t *cur = NULL;
    for (int i = 0; i < n; i++) {
        char mhz[16];
        format_mhz(presets[i].freq, mhz, sizeof(mhz));
        snprintf(line, sizeof(line), "%s  %s", mhz, presets[i].label);
        lv_obj_t *b = preset_add(line, i);
        if (!params.ft8_custom_on.x && presets[i].freq == cfg_digital_freq_get()) {
            lv_obj_set_style_text_color(b, TC(ok), 0);   /* the one in use */
            cur = b;
        }
    }
    lv_obj_t *close = preset_add("Close", -1);
    lv_obj_set_style_text_color(close, TC(action), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(cur ? cur : close);
}

/* Hold Freq: the dial frequency from the keyboard */
static void freq_hold_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    freq_close();
    if (state == TX_PROCESS) {
        msg_schedule_text_fmt("Not while sending");
        return;
    }
    if (map_obj) return;
    lv_group_remove_obj(table);
    textarea_window_open(keyboard_ok_cb, keyboard_cancel_cb);
    lv_obj_t *text = textarea_window_text();
    lv_textarea_set_accepted_chars(text, "0123456789.");
    lv_textarea_set_placeholder_text(text, " 14091 or 14.091, empty: presets");
    if (params.ft8_custom_hz.x) {
        char mhz[16];
        format_mhz(params.ft8_custom_hz.x, mhz, sizeof(mhz));
        textarea_window_set(mhz);
    }
    kb_freq         = true;
    disable_buttons = true;
    msg_schedule_text_fmt("Dial frequency (14091 or 14.091), empty for the presets");
}

const char *cq_all_label_getter() {
    static char buf[32];
    sprintf(buf, "Show:\n%s", param_i_get(cfg_ft8_show_all) ? tr("All"): "CQ");
    return buf;
}

const char *protocol_label_getter() {
    static char buf[32];
    sprintf(buf, "Mode:\n%s", ftx_mode_name());
    return buf;
}

const char *tx_cq_label_getter() {
    static char buf[32];
    sprintf(buf, "TX CQ:\n%s", subject_i_get(cq_enabled) ? tr("Enabled"): tr("Disabled"));
    return buf;
}

const char *tx_call_label_getter() {
    static char buf[32];
    sprintf(buf, "TX Call:\n%s", subject_i_get(tx_enabled) ? tr("Enabled"): tr("Disabled"));
    return buf;
}

const char *hold_freq_label_getter() {
    static char buf[32];
    sprintf(buf, "Hold Freq:\n%s", param_i_get(cfg_ft8_hold_freq) ? tr("Enabled"): tr("Disabled"));
    return buf;
}

const char *auto_label_getter() {
    static char buf[32];
    sprintf(buf, "Auto:\n%s", param_i_get(cfg_ft8_auto) ? tr("Enabled"): tr("Disabled"));
    return buf;
}

static void show_cq_all_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    param_i_set(cfg_ft8_show_all, !param_i_get(cfg_ft8_show_all));
}

static void mode_ft4_ft8_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;

    /* FT8 -> FT4 -> FT2 -> FT8 */
    int proto = param_i_get(cfg_ft8_protocol);
    if (proto == FTX_PROTOCOL_FT8)
        proto = FTX_PROTOCOL_FT4;
    else if (proto == FTX_PROTOCOL_FT4)
        proto = FTX_PROTOCOL_FT2;
    else {
        proto = FTX_PROTOCOL_FT8;
    }
    param_i_set(cfg_ft8_protocol, proto);
    freq_strip_set_app(ftx_protocol_name(proto));
    subject_i_set(cq_enabled, false);

    worker_done();
    worker_init();
    clean_screen();
    if (params.ft8_custom_on.x) params_bool_set(&params.ft8_custom_on, false); /* FT4/FT2 elsewhere */
    if (proto != FTX_PROTOCOL_FT8) hound_on = false;
    load_band(0);
}

static void mode_auto_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    bool new_val = !param_i_get(cfg_ft8_auto);
    param_i_set(cfg_ft8_auto, new_val);
    ftx_qso_processor_set_auto(qso_processor, new_val);
}

static void hold_tx_freq_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    param_i_set(cfg_ft8_hold_freq, !param_i_get(cfg_ft8_hold_freq));
}

static void tx_cq_en_dis_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    if (hound_on && !subject_i_get(cq_enabled)) {
        msg_schedule_text_fmt("No CQ in Hound mode");
        return;
    }

    if (!subject_i_get(cq_enabled)){
        if (strlen(params.callsign.x) == 0) {
            msg_schedule_text_fmt("Call sign required");
            return;
        }
        subject_i_set(cq_enabled, true);
        subject_i_set(tx_enabled, true);

        cq_make_message(params.callsign.x, station_qth(), params.ft8_cq_modifier.x, tx_msg.msg);

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        float time_since_slot_start;
        tx_time_slot = !get_time_slot(now, &time_since_slot_start);
        if (time_since_slot_start < ftx_max_tx_start()) {
            tx_time_slot = !tx_time_slot;
        }

        if (tx_msg.msg[2] == '_') {
            msg_schedule_text_fmt("Next TX: CQ %s", tx_msg.msg + 3);
        } else {
            msg_schedule_text_fmt("Next TX: %s", tx_msg.msg);
        }
        tx_msg.repeats = param_i_get(cfg_ft8_max_repeats);
        ftx_qso_processor_reset(qso_processor);
        lv_finder_clear_cursor(finder);
    } else {
        if (state == TX_PROCESS) {
            state = RX_PROCESS;
        }
        subject_i_set(cq_enabled, false);
        tx_msg.msg[0] = '\0';
    }
}

static void tx_call_en_dis_cb(struct button_data_t *btn_data) {
    if (disable_buttons)
        return;

    if (!subject_i_get(tx_enabled)) {
        if (strlen(params.callsign.x) == 0) {
            msg_schedule_text_fmt("Call sign required");
            return;
        }
        subject_i_set(tx_enabled, true);
    } else {
        if (state == TX_PROCESS) {
            state = RX_PROCESS;
        }
        subject_i_set(tx_enabled, false);
    }
}

static void tx_call_off() {
    state = RX_PROCESS;
    subject_i_set(tx_enabled, false);
}

static void cq_modifier_cb(struct button_data_t *btn_data) {
    if (disable_buttons) return;
    keyboard_open();
}

static void time_sync(struct button_data_t *btn_data) {
    time_t now = time(NULL);
    uint8_t sec = now % 60;
    float drift, slot_time;
    slot_time = ftx_slot_time(param_i_get(cfg_ft8_protocol));
    drift = fmodf(sec + slot_time / 2, slot_time) - slot_time / 2;
    struct timespec tp;

    now -= (int) drift;
    tp.tv_sec = now;
    tp.tv_nsec = 0;

    int res = clock_settime(CLOCK_REALTIME, &tp);
    if (res != 0)
    {
        LV_LOG_ERROR("Can't set system time: %s\n", strerror(errno));
        return;
    }
}

static void force_save_qso(struct button_data_t *btn_data) {
    if (ftx_qso_processor_can_save_qso(qso_processor)) {
        ftx_qso_processor_force_save_qso(qso_processor);
    } else {
        msg_schedule_text_fmt("Can't save incomplete QSO");
    }
}

static void on_table_press(const cell_data_t *cell_data) {
    if (state == TX_PROCESS) {
        tx_call_off();
        return;
    }

    if ((cell_data == NULL) ||
        (cell_data->cell_type == CELL_TX_MSG) ||
        (cell_data->cell_type == CELL_RX_INFO) ||
        (cell_data->cell_type == CELL_START_QSO)
    ) {
        msg_schedule_text_fmt("What should I do about it?");
        return;
    }

    if (hound_on) {
        if (!cell_data->meta.call_de[0] || strlen(params.callsign.x) == 0) {
            msg_schedule_text_fmt(cell_data->meta.call_de[0] ? "Call sign required" : "Select the Fox's CQ");
            return;
        }
        hound_call(&hound, params.callsign.x, station_qth(), cell_data->meta.call_de, cell_data->meta.grid,
                   time(NULL), tx_msg.msg);
        tx_msg.repeats = -1;    /* until the Fox answers or the 2 minutes are over */
        tx_time_slot   = true;  /* the Fox sends first, the Hounds second */
        lv_finder_set_cursor(finder, cell_data->meta.freq_hz);
        subject_i_set(tx_enabled, true);
        add_info("Hound: calling %s above %d Hz", hound.fox, HOUND_MIN_TX_HZ);
        msg_schedule_text_fmt("Next TX: %s", tx_msg.msg);
        return;
    }

    ftx_qso_processor_start_qso(qso_processor, (ftx_msg_meta_t *)&cell_data->meta, &tx_msg);
    if (strlen(tx_msg.msg) > 0) {
        lv_finder_set_cursor(finder, cell_data->meta.freq_hz);
        if (!param_i_get(cfg_ft8_hold_freq)) {
            set_freq(cell_data->meta.freq_hz);
        }
        tx_time_slot = !cell_data->odd;
        subject_i_set(tx_enabled, true);
        {
            cell_data_t cd;
            cd.cell_type = CELL_START_QSO;
            snprintf(cd.text, sizeof(cd.text), tr("Start QSO with %s"), cell_data->meta.call_de);
            scheduler_put(table_view_add_msg_cb, &cd, sizeof(cell_data_t));
        }
        msg_schedule_text_fmt("Next TX: %s", tx_msg.msg);
    } else {
        msg_schedule_text_fmt("Invalid message");
        tx_call_off();
    }
}

static void on_table_close(void) {
    dialog_destruct(&dialog);
}

static void on_table_vol_change(int32_t delta) {
    radio_change_vol(delta);
}

static void keyboard_open() {
    lv_group_remove_obj(table);
    textarea_window_open(keyboard_ok_cb, keyboard_cancel_cb);
    lv_obj_t *text = textarea_window_text();

    lv_textarea_set_accepted_chars(text,
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    );

    if (strlen(params.ft8_cq_modifier.x) > 0) {
        textarea_window_set(params.ft8_cq_modifier.x);
    } else {
        lv_obj_t *text = textarea_window_text();
        lv_textarea_set_placeholder_text(text, " CQ modifier");
    }
    disable_buttons = true;
}

static void keyboard_close() {
    textarea_window_close();
    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);
    disable_buttons = false;
    kb_freq = false;
}

static bool keyboard_cancel_cb() {
    keyboard_close();
    return true;
}

static bool freq_ok(void) {
    const char *text = textarea_window_get();
    if (text[0]) {
        char   *end;
        double  v  = strtod(text, &end);
        int32_t hz = (int32_t)(v < 100 ? v * 1e6 + 0.5 : v * 1e3 + 0.5);
        if (end == text || *end || hz < CUSTOM_MIN_HZ || hz > CUSTOM_MAX_HZ) {
            msg_schedule_text_fmt("Type 1800 - 54000 kHz, e.g. 14091");
            return false;
        }
        params_int32_set(&params.ft8_custom_hz, hz);
    }
    params_bool_set(&params.ft8_custom_on, text[0] != '\0');
    keyboard_close();
    load_base();
    retuned();
    buttons_refresh(&button_freq);
    return true;
}

static bool keyboard_ok_cb() {
    if (kb_freq) return freq_ok();
    char *cq_mod = (char *)textarea_window_get();
    if ((strlen(cq_mod) > 0) && !is_cq_modifier(cq_mod)) {
        msg_schedule_text_fmt("Unsupported CQ modifier");
        return false;
    }
    params_str_set(&params.ft8_cq_modifier, cq_mod);
    keyboard_close();
    return true;
}

static void audio_cb(unsigned int n, float *samples) {
    if (state == RX_PROCESS) {
        audio_worker_feed(audio_worker, n, samples);
    }
}

static bool get_time_slot(struct timespec now, float *sec_since_start) {
    bool cur_odd;
    float sec = (now.tv_sec % 60) + now.tv_nsec / 1.0e9f;

    float slot_time = ftx_slot_time(param_i_get(cfg_ft8_protocol));

    cur_odd = (int)(sec / slot_time) % 2;
    *sec_since_start = fmodf(sec, slot_time);
    return cur_odd;
}


/* CQ message composition has moved to ft8/cq_scheduler.c
 * (see cq_make_message()). */

/* TX waveform synthesis + per-block ALC gain correction live in
 * ft8/tx_worker.c. The dialog only owns the per-session abort flag. */
static bool tx_should_abort_cb(void *ctx) {
    (void)ctx;
    return state != TX_PROCESS;
}

/**
 * Add INFO message to the table
 */
static void add_info(const char * fmt, ...) {
    va_list     args;
    cell_data_t  cell_data;
    cell_data.cell_type = CELL_RX_INFO;

    va_start(args, fmt);
    vsnprintf(cell_data.text, sizeof(cell_data.text), tr(fmt), args);
    va_end(args);

    scheduler_put(table_view_add_msg_cb, &cell_data, sizeof(cell_data_t));
}

/**
 * Add TX message to the table
 */
static void add_tx_text(const char * text) {
    cell_data_t  cell_data;
    cell_data.cell_type = CELL_TX_MSG;

    char clean[sizeof(cell_data.text)];
    struct timespec now;

    snprintf(clean, sizeof(clean), "%s", text);
    if (strncmp(clean, "CQ_", 3) == 0) {
        clean[2] = ' ';
    }
    clock_gettime(CLOCK_REALTIME, &now);
    strftime(cell_data.text, 10, "%H:%M:%S ", gmtime(&now.tv_sec));
    snprintf(cell_data.text + 9, sizeof(cell_data.text) - 9, "%s", clean);
    scheduler_put(table_view_add_msg_cb, &cell_data, sizeof(cell_data_t));

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "t", "ft8_tx");
    cJSON_AddStringToObject(o, "text", clean);
    remote_event(o);
}

/* Every decode, for the web remote */
static void remote_rx_event(int16_t snr, const char *text, bool odd, const ftx_msg_meta_t *meta) {
    static const char *types[] = {"CQ", "GRID", "REPORT", "R_REPORT", "RR73", "73", "OTHER"};
    int32_t            id;

    pthread_mutex_lock(&remote_rx_mutex);
    id = ++remote_rx_id;
    remote_rx[id % REMOTE_RX_N].id   = id;
    remote_rx[id % REMOTE_RX_N].odd  = odd;
    remote_rx[id % REMOTE_RX_N].meta = *meta;
    pthread_mutex_unlock(&remote_rx_mutex);

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "t", "ft8_rx");
    cJSON_AddNumberToObject(o, "id", id);
    cJSON_AddBoolToObject(o, "odd", odd);
    cJSON_AddNumberToObject(o, "snr", snr);
    cJSON_AddNumberToObject(o, "dt", roundf(meta->time_sec * 10.0f) / 10.0f);
    cJSON_AddNumberToObject(o, "freq", roundf(meta->freq_hz));
    cJSON_AddStringToObject(o, "text", text);
    cJSON_AddStringToObject(o, "call", meta->call_de);
    cJSON_AddStringToObject(o, "grid", meta->grid);
    cJSON_AddStringToObject(o, "type", meta->type <= FXT_MSG_TYPE_OTHER ? types[meta->type] : "OTHER");
    cJSON_AddBoolToObject(o, "to_me", meta->to_me);
    remote_event(o);
}

/* Hound: the Fox's report gets R+report, its RR73 logs the QSO */
static void hound_rx_text(const char *text, int snr, float freq_hz, bool odd) {
    char          tx[sizeof(tx_msg.msg)];
    hound_stage_t was = hound.stage;
    hound_event_t ev  = hound_rx(&hound, text, snr, (int)(freq_hz + 0.5f), odd, time(NULL), tx);

    if (ev == HOUND_REPLY) {
        strcpy(tx_msg.msg, tx);
        tx_msg.repeats = -1;
        tx_time_slot   = true;
        lv_finder_set_cursor(finder, freq_hz);
        add_info("Hound: %s sent %+03d, answering R%+03d at %d Hz", hound.fox, hound.rcvd, hound.sent, hound.fox_freq);
        msg_schedule_text_fmt("Next TX: %s", tx_msg.msg);
    } else if (ev == HOUND_LOGGED) {
        tx_msg.msg[0] = '\0';
        save_qso(hound.fox, hound.fox_grid, hound.rcvd, hound.sent);
        add_info("Hound: QSO with %s logged", hound.fox);
    } else if (was != HOUND_IDLE && hound.stage == HOUND_IDLE) {
        tx_msg.msg[0] = '\0';
        add_info("Hound: %s sent RR73 but its report was missed, not logged", hound.fox);
    }
}

/* For the RX rows */
static const char *hound_state(void) {
    static char buf[40];
    switch (hound.stage) {
        case HOUND_CALLING: snprintf(buf, sizeof(buf), tr("calling %s"), hound.fox); break;
        case HOUND_REPORT:  snprintf(buf, sizeof(buf), tr("R%+03d to %s"), hound.sent, hound.fox); break;
        default:            snprintf(buf, sizeof(buf), tr("select the Fox")); break;
    }
    return buf;
}

/**
 * Parse and add RX messages to the table
 */
static void add_rx_text(int16_t snr, const char * text, slot_info_t *s_info, float freq_hz, float time_sec) {

    ftx_msg_meta_t meta;
    memset(&meta, 0, sizeof(meta));     /* the parser fills call_de only for the messages it knows */
    meta.freq_hz = freq_hz;
    meta.time_sec = time_sec;
    char * old_msg = strdup(tx_msg.msg);
    ftx_tx_msg_t unused;
    ftx_qso_processor_add_rx_text(qso_processor, text, snr, &meta, hound_on ? &unused : &tx_msg);

    if (hound_on) {
        hound_rx_text(text, snr, freq_hz, s_info->odd);
    } else if ((strlen(tx_msg.msg) > 0) && (strcmp(old_msg, tx_msg.msg) != 0)) {
        lv_finder_set_cursor(finder, meta.freq_hz);
        if (!param_i_get(cfg_ft8_hold_freq)) {
            set_freq(freq_hz);
        }
        tx_time_slot = !s_info->odd;
        msg_schedule_text_fmt("Next TX: %s", tx_msg.msg);
        if (subject_i_get(cq_enabled)) {
            subject_i_set(cq_enabled, false);
        }
    }
    free(old_msg);
    remote_rx_event(snr, text, s_info->odd, &meta);

    /* The heard map and PSK Reporter: the sender of any message we can parse */
    if (meta.type != FXT_MSG_TYPE_OTHER && meta.call_de[0]) {
        map_rx_t rx = {meta, s_info->odd};
        js8_map_heard(meta.call_de, meta.grid, snr, meta.to_me, &rx, sizeof(rx));
        if (param_i_get(cfg_pskreporter)) {
            pskr_spot(params.callsign.x, station_qth(), VERSION, meta.call_de, meta.grid,
                      (uint32_t)cparam_i_get(cfg_fg_freq) + (uint32_t)(freq_hz + 0.5f), snr,
                      ftx_mode_name());
        }
    }

    ft8_cell_type_t cell_type;
    if (meta.to_me) {
        cell_type = CELL_RX_TO_ME;
    } else if (meta.type == FTX_MSG_TYPE_CQ) {
        cell_type = CELL_RX_CQ;
    } else if (!param_i_get(cfg_ft8_show_all)) {
        return;
    } else {
        cell_type = CELL_RX_MSG;
    }

    cell_data_t  cell_data;
    if (meta.type == FTX_MSG_TYPE_CQ) {
        cell_data.worked_type = qso_log_search_worked(
            meta.call_de,
            ftx_log_mode(),
            qso_log_freq_to_band(cparam_i_get(cfg_fg_freq))
        );
    }

    cell_data.cell_type = cell_type;
    cell_data.pota = meta.call_de[0] && (!strncmp(text, "CQ POTA ", 8) || pota_spotted(meta.call_de));
    snprintf(cell_data.text, sizeof(cell_data.text), "%s %s", slot_time, text);
    cell_data.meta = meta;
    /* The sender's country and flag, from the call (cty.dat), as JTDX (from SP2KMO's upstream PR #282) */
    cell_data.country[0]    = '\0';
    cell_data.cty_prefix[0] = '\0';
    cell_data.flag          = -1;
    const char *cty_name, *cty_prefix;
    if (meta.type != FXT_MSG_TYPE_OTHER && meta.call_de[0] && dxcc_lookup(meta.call_de, &cty_name, &cty_prefix)) {
        snprintf(cell_data.country, sizeof(cell_data.country), "%s", cty_name);
        snprintf(cell_data.cty_prefix, sizeof(cell_data.cty_prefix), "%s", cty_prefix);
        for (size_t i = 0; i < sizeof(flag_of) / sizeof(flag_of[0]); i++)
            if (!strcmp(flag_of[i].prefix, cty_prefix)) {
                cell_data.flag = (int16_t)flag_of[i].flag;
                break;
            }
    }
    cell_data.odd = s_info->odd;
    if (station_qth()[0] != 0) {
        if (strlen(meta.grid) > 0) {
            double lat, lon;
            qth_str_to_pos(meta.grid, &lat, &lon);
            cell_data.dist = qth_pos_dist(lat, lon, cur_lat, cur_lon);
        } else {
            cell_data.dist = 0;
        }
    } else {
        cell_data.dist = 0;
    }
    scheduler_put(table_view_add_msg_cb, (void*)&cell_data, sizeof(cell_data_t));
}

/* ---- audio_worker callbacks (run on the worker thread) ---------------- */

static void on_message_cb(const char *text, int snr, float freq_hz, float time_sec,
                          const slot_info_t *info, void *ctx) {
    (void)ctx;
    /* To WSJT-X listeners first (worker thread, non-blocking socket) */
    ft8_udp_send_decode(true, param_i_get(cfg_ft8_protocol) == FTX_PROTOCOL_FT8 ? "~" : "+", text, (int32_t)snr,
                        time_sec, (uint32_t)(freq_hz + 0.5f));
    add_rx_text((int16_t)snr, text, (slot_info_t *)info, freq_hz, time_sec);
}

/**
 * Helper function to update UI in main thread
 */

struct waterfall_data {
    float *psd;
    size_t size;
};

static void waterfall_add_data(void *data) {
    struct waterfall_data *wf_data = (struct waterfall_data *)data;
    lv_waterfall_add_data(waterfall, wf_data->psd, wf_data->size);
    free(wf_data->psd);
}

static int cmp_float(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

static void on_psd_cb(const float *psd, uint16_t nfft, float sec_since_slot_start,
                      const slot_info_t *info, void *ctx) {
    (void)sec_since_slot_start;
    (void)info;
    (void)ctx;
    if (!psd || !nfft) return;

    uint32_t low_bin  = (uint32_t)nfft / 2u + (uint32_t)nfft * filter_low  / SAMPLE_RATE;
    uint32_t high_bin = (uint32_t)nfft / 2u + (uint32_t)nfft * filter_high / SAMPLE_RATE;
    if (high_bin > nfft) high_bin = nfft;
    if (low_bin >= high_bin) return;

    // Schedule waterfall update in main thread
    struct waterfall_data wf_data;
    wf_data.size = high_bin - low_bin;
    wf_data.psd = (float *)calloc(sizeof(float), wf_data.size);
    memcpy((void*)wf_data.psd, (void*)&psd[low_bin], wf_data.size * sizeof(float));

    /* Relative to the noise floor (the 30th percentile of the row, smoothed over ~2 s), as
     * JS8: the fixed -27..0 dB left it near black at the radio's audio level */
    static float floor_db;
    static bool  floor_set;
    float       *sorted = malloc(wf_data.size * sizeof(float));
    if (sorted) {
        memcpy(sorted, wf_data.psd, wf_data.size * sizeof(float));
        qsort(sorted, wf_data.size, sizeof(float), cmp_float);
        floor_db  = floor_set ? floor_db + 0.05f * (sorted[wf_data.size * 3 / 10] - floor_db) : sorted[wf_data.size * 3 / 10];
        floor_set = true;
        free(sorted);
    }
    for (size_t i = 0; i < wf_data.size; i++) wf_data.psd[i] -= floor_db;

    scheduler_put(waterfall_add_data, &wf_data, sizeof(wf_data));
}

static void on_slot_end_cb(const slot_info_t *info, void *ctx) {
    (void)info;
    (void)ctx;
    if (qso_processor) {
        ftx_qso_processor_start_new_slot(qso_processor);
    }
}

static void on_tick_cb(const slot_info_t *info, bool new_slot,
                       float sec_since_slot_start, void *ctx) {
    (void)ctx;

    bool have_tx_msg = tx_msg.msg[0] != '\0';

    if ((sec_since_slot_start < ftx_max_tx_start()) && have_tx_msg) {
        if ((tx_time_slot == info->odd) && subject_i_get(tx_enabled)) {
            if (hound_on) {
                if (hound_timed_out(&hound, time(NULL))) {
                    tx_msg.msg[0] = '\0';
                    add_info("Hound: no answer from %s, TX stopped", hound.fox);
                    return;
                }
                int f = hound_tx_freq(&hound, params.ft8_tx_freq.x, (unsigned)rand());
                if (f != params.ft8_tx_freq.x) set_freq(f);
            }
            state = TX_PROCESS;
            add_tx_text(tx_msg.msg);
            tx_worker_run(tx_msg.msg, AUDIO_PLAY_RATE, base_gain_offset,
                          tx_should_abort_cb, NULL);
            state = RX_PROCESS;
            if (tx_msg.repeats > 0) {
                tx_msg.repeats--;
            }
            if (tx_msg.repeats == 0) {
                if (strncmp(tx_msg.msg, "CQ", 2) == 0) {
                    subject_i_set(cq_enabled, false);
                }
                tx_msg.msg[0] = '\0';
            }
            return;
        }
    }

    if (new_slot) {
        state = RX_PROCESS;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        struct tm *ts = gmtime(&now.tv_sec);
        snprintf(slot_time, sizeof(slot_time), "%02i:%02i:%02i", ts->tm_hour, ts->tm_min, ts->tm_sec);
        static unsigned ft2_slots;

        /* FT2: an RX line every 3.75 s would fill the table; one every 4th slot (15 s), as FT8 */
        if ((!have_tx_msg || !subject_i_get(tx_enabled)) && (!ftx_is_ft2() || (ft2_slots++ % 4) == 0)) {
            if (hound_on)
                add_info("RX %s %s  Hound: %s", where_label(), slot_time, hound_state());
            else
                add_info("RX %s %s", where_label(), slot_time);
        }
    }
}

/* ---- Web remote --------------------------------------------------------- */

void dialog_ft8_remote(const char *cmd, int32_t val, const char *text) {
    (void)text;
    if (!strcmp(cmd, "ft8_cq")) {
        if (val != subject_i_get(cq_enabled))
            tx_cq_en_dis_cb(NULL);
    } else if (!strcmp(cmd, "ft8_stop")) {
        tx_call_off();
        subject_i_set(cq_enabled, false);
        tx_msg.msg[0] = '\0';
        hound_stop(&hound);
    } else if (!strcmp(cmd, "ft8_auto")) {
        if (val != param_i_get(cfg_ft8_auto))
            mode_auto_cb(NULL);
    } else if (!strcmp(cmd, "ft8_hold")) {
        if (val != param_i_get(cfg_ft8_hold_freq))
            hold_tx_freq_cb(NULL);
    } else if (!strcmp(cmd, "ft8_txfreq")) {
        set_freq(limit(val, filter_low, filter_high - ftx_width_hz()));
    } else if (!strcmp(cmd, "ft8_reply")) {
        cell_data_t cd = {.cell_type = CELL_RX_MSG};
        bool        found;

        pthread_mutex_lock(&remote_rx_mutex);
        found = remote_rx[val % REMOTE_RX_N].id == val;
        if (found) {
            cd.odd  = remote_rx[val % REMOTE_RX_N].odd;
            cd.meta = remote_rx[val % REMOTE_RX_N].meta;
        }
        pthread_mutex_unlock(&remote_rx_mutex);
        if (found)
            on_table_press(&cd);
        else
            msg_update_text_fmt("FT8: that message is too old");
    } else if (!strcmp(cmd, "app_base")) {
        load_base();
    }
}

int dialog_ft8_remote_state(char *buf, size_t size) {
    return snprintf(buf, size, "{\"cq\":%i,\"txen\":%i,\"txing\":%i,\"auto\":%i,\"hold\":%i,\"txfreq\":%i,\"base\":%i}",
                    cq_enabled ? subject_i_get(cq_enabled) != 0 : 0, tx_enabled ? subject_i_get(tx_enabled) != 0 : 0,
                    state == TX_PROCESS, param_i_get(cfg_ft8_auto) != 0, param_i_get(cfg_ft8_hold_freq) != 0,
                    params.ft8_tx_freq.x,
                    (int)(params.ft8_custom_on.x ? params.ft8_custom_hz.x : cfg_digital_freq_get()));
}
