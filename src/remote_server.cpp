/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Web remote: control socket for the webserver's /remote page and the phone app
 *  (X6100 Link). UNIX socket, one JSON object per line. Requests:
 *    {"cmd":"get"}                 -> one state line
 *    {"cmd":"sub"}                 -> state lines (5/s), spectrum lines (10/s), events (remote_event) and decoded text (remote_text)
 *                                     until the client closes
 *    {"cmd":"freq","val":14074000} {"cmd":"tune","val":-100} {"cmd":"mode","val":"USB"} {"cmd":"band","val":40}
 *    {"cmd":"pre|att|nb|nr","val":0|1} {"cmd":"agc"} (next) {"cmd":"rfgain","val":0..100} {"cmd":"filter","val":Hz}
 *    {"cmd":"nb_level|nb_width","val":0..100} {"cmd":"nr_level","val":0..60} {"cmd":"agc_mode","val":0..3} (OFF SLOW FAST AUTO)
 *    {"cmd":"pwr","val":1..100} (TX power, tenths of a watt)
 *    {"cmd":"location","text":"45.0,7.7"} (the phone's position, for Field Ops)
 *    {"cmd":"app","text":"FT8"} and the app commands (app_cmds[]), with "val" and/or "text"
 *                                  -> {"t":"ok"} or {"t":"err","err":"..."}
 *  Commands are applied on the LVGL thread through the scheduler. There is no PTT: only the
 *  apps transmit. While the radio transmits only the stop/close commands are taken.
 */

#include "ft8/ft2.h"
#include "remote_server.h"
#include "cw_field.h"
#include "panel.h"

#include "cfg/db.h"
#include "cfg/settings_manager.h"
#include "cfg/settings_manager_api.h"
#include "util.h"
#include "wifi.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <string>

extern "C" {
    #include "cJSON.h"
    #include "dialog.h"
    #include "cw_tune_ui.h"
    #include "dialog_cw_kbd.h"
    #include "dialog_field.h"
    #include "dialog_cluster.h"
    #include "dialog_snake.h"
    #include "dialog_ft8.h"
    #include "dialog_js8.h"
    #include "dialog_psk.h"
    #include "dialog_olivia.h"
    #include "dialog_jtty.h"
    #include "dialog_navtex.h"
    #include "dialog_sstv.h"
    #include "dialog_wefax.h"
    #include "dialog_wspr.h"
    #include "gps.h"
    #include "station.h"
    #include "main.h"
    #include "main_screen.h"
    #include "meter.h"
    #include "radio.h"
    #include "rtty.h"
    #include "scheduler.h"
    #include "walkie.h"
    #include "waterfall.h"
    #include "params/params.h"
    #include "backlight.h"
    #include "theme.h"
    #include "tx_info.h"

    #include "lvgl/lvgl.h"
    #include <ctype.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <math.h>
    #include <poll.h>
    #include <pthread.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/socket.h>
    #include <sys/stat.h>
    #include <sys/un.h>
    #include <time.h>
    #include <unistd.h>
}

#ifndef REMOTE_SOCK
#define REMOTE_SOCK     "/tmp/x6100_remote.sock"
#endif
#define MAX_CLIENTS     6
#define MAX_SUBS        3
#define IN_SIZE         512
#define OUT_SIZE        (32 * 1024)
#define NBINS           800
#define FRAME_MS        50      /* below dsp.cpp 66 ms: all its 15 lines a second go out, not every other one (7.5) */
#define STATE_MS        200
#define TUNE_MS         50      /* while the VFO moves, the state goes out this often */
#define IDLE_MS         5000    /* a command client must be done within this */
#define STALL_MS        10000   /* a subscriber that takes nothing for this long is dropped */
#define MAX_PENDING     16      /* commands queued on the LVGL thread */
#define MAX_EVENTS      64      /* events waiting for the socket thread */
#define TEXT_MS         300     /* decoded text (remote_text) waits at most this */
#define TEXT_MAX        200     /* or this many characters */

typedef struct {
    int      fd;
    bool     sub;
    size_t   in_len;
    char     in[IN_SIZE];
    size_t   out_len;
    char     out[OUT_SIZE];
    uint64_t last_ms;   /* connect time, then last write progress */
} client_t;

typedef enum {
    CMD_FREQ, CMD_TUNE, CMD_MODE, CMD_BAND, CMD_PRE, CMD_ATT, CMD_AGC, CMD_NB, CMD_NR, CMD_RFGAIN, CMD_FILTER, CMD_PWR,
    CMD_NB_LEVEL, CMD_NB_WIDTH, CMD_NR_LEVEL, CMD_AGC_MODE, CMD_SQL,
    CMD_APP, CMD_APP_CMD, CMD_LOCATION, CMD_QSO_LOG
} cmd_id_t;

typedef struct {
    cmd_id_t id;
    int32_t  val;
    size_t   app_cmd;   /* CMD_APP_CMD: index in app_cmds[] */
    double   lat, lon;  /* CMD_LOCATION */
    char     text[201];
} cmd_t;

/* x6100_mode_t and x6100_agc_t order */
static const char *mode_names[] = {"LSB", "LSB-D", "USB", "USB-D", "CW", "CW-R", "AM", "NFM"};
static const char *agc_names[]  = {"OFF", "SLOW", "FAST", "AUTO"};

/* "app" values; cont_modes[] order in dialog_sstv.c */
static const char *app_names[]  = {"off", "FT8", "FT4", "JS8", "WSPR", "SSTV", "WeFax", "NavTex", "RTTY", "CW", "Field", "Snake", "PSK31", "PSK63", "Olivia", "FT2", "JTTY"};
static const char *sstv_modes[] = {"R8BW", "R36", "R72", "M1", "M2", "S1", "S2"};

/* Commands of the app that is open: app is current_app() or its start ("FT": FT8 and FT4); a name
 * can have a row for each app. val in min..max (none when min > max). With val < stop they are
 * taken also while the radio transmits. */
static const struct {
    const char *name;
    const char *app;
    void      (*fn)(const char *cmd, int32_t val, const char *text);
    int32_t     min, max, stop;
} app_cmds[] = {
    {"ft8_cq",      "FT",     dialog_ft8_remote,    0, 1,         1},
    {"ft8_reply",   "FT",     dialog_ft8_remote,    1, INT32_MAX, INT32_MIN},
    {"ft8_stop",    "FT",     dialog_ft8_remote,    1, 0,         INT32_MAX},
    {"ft8_auto",    "FT",     dialog_ft8_remote,    0, 1,         INT32_MIN},
    {"ft8_hold",    "FT",     dialog_ft8_remote,    0, 1,         INT32_MIN},
    {"ft8_txfreq",  "FT",     dialog_ft8_remote,    0, 5000,      INT32_MIN},
    {"js8_send",    "JS8",    dialog_js8_remote,    1, 0,         INT32_MIN},
    {"js8_cq",      "JS8",    dialog_js8_remote,    1, 0,         INT32_MIN},
    {"js8_hb",      "JS8",    dialog_js8_remote,    1, 0,         INT32_MIN},
    {"js8_stop",    "JS8",    dialog_js8_remote,    1, 0,         INT32_MAX},
    {"wspr_txpct",  "WSPR",   dialog_wspr_remote,   0, 50,        INT32_MIN},
    {"wspr_txnext", "WSPR",   dialog_wspr_remote,   0, 1,         1},
    {"sstv_tx",     "SSTV",   dialog_sstv_remote,   1, 0,         INT32_MIN},   /* val: sstv_modes[] */
    {"sstv_stop",   "SSTV",   dialog_sstv_remote,   1, 0,         INT32_MAX},
    {"wefax_mode",  "WeFax",  dialog_wefax_remote,  0, 1,         INT32_MIN},
    {"wefax_save",  "WeFax",  dialog_wefax_remote,  1, 0,         INT32_MIN},
    {"wefax_clear", "WeFax",  dialog_wefax_remote,  1, 0,         INT32_MIN},
    {"wefax_tx",    "WeFax",  dialog_wefax_remote,  1, 0,         INT32_MIN},
    {"wefax_stop",  "WeFax",  dialog_wefax_remote,  1, 0,         INT32_MAX},
    {"rtty_send",   "RTTY",   rtty_remote,          1, 0,         INT32_MIN},
    {"rtty_stop",   "RTTY",   rtty_remote,          1, 0,         INT32_MAX},
    {"rtty_rate",   "RTTY",   rtty_remote,          4500, 15000,  INT32_MIN},
    {"rtty_shift",  "RTTY",   rtty_remote,          170, 850,     INT32_MIN},
    {"rtty_center", "RTTY",   rtty_remote,          800, 1600,    INT32_MIN},
    {"rtty_rev",    "RTTY",   rtty_remote,          0, 1,         INT32_MIN},
    {"cw_send",     "CW",     dialog_cw_kbd_remote, 1, 0,         INT32_MAX},   /* also while keying */
    {"cw_stop",     "CW",     dialog_cw_kbd_remote, 1, 0,         INT32_MAX},
    {"cw_wpm",      "CW",     dialog_cw_kbd_remote, 5, 60,        INT32_MIN},
    {"cw_dec",      "CW",     dialog_cw_kbd_remote, 0, 1,         INT32_MIN},
    /* With the CW Keyboard app closed: the main screen's CW line (cw_field.c); "" matches any app */
    {"cw_send",     "",       cw_field_remote,      1, 0,         INT32_MAX},
    {"cw_stop",     "",       cw_field_remote,      1, 0,         INT32_MAX},
    {"cw_wpm",      "",       cw_field_remote,      5, 60,        INT32_MIN},
    {"cw_dec",      "",       cw_field_remote,      0, 1,         INT32_MIN},
    {"cw_clear",    "",       cw_field_remote,      1, 0,         INT32_MIN},
    {"cw_calls",    "",       panel_remote,         1, 0,         INT32_MAX},   /* calls the app found on QRZ */
    {"field_screen", "Field", dialog_field_remote,  1, 0,         INT32_MIN},
    {"field_ref",   "Field",  dialog_field_remote,  1, 0,         INT32_MIN},
    {"field_start", "Field",  dialog_field_remote,  1, 0,         INT32_MIN},
    {"field_end",   "Field",  dialog_field_remote,  1, 0,         INT32_MIN},
    {"field_log",   "Field",  dialog_field_remote,  1, 0,         INT32_MAX},   /* also while transmitting */
    {"field_spot",  "Field",  dialog_field_remote,  1, 0,         INT32_MIN},
    {"field_update", "Field", dialog_field_remote,  1, 0,         INT32_MIN},
    {"app_base",    "FT",     dialog_ft8_remote,    1, 0,         INT32_MIN},
    {"app_base",    "JS8",    dialog_js8_remote,    1, 0,         INT32_MIN},
    {"app_base",    "WSPR",   dialog_wspr_remote,   1, 0,         INT32_MIN},
    {"app_base",    "SSTV",   dialog_sstv_remote,   1, 0,         INT32_MIN},
    {"app_base",    "WeFax",  dialog_wefax_remote,  1, 0,         INT32_MIN},
    {"navtex_mode", "NavTex", dialog_navtex_remote, 0, 1,         INT32_MIN},   /* 0 NAVTEX, 1 DSC */
    {"app_base",    "NavTex", dialog_navtex_remote, 1, 0,         INT32_MIN},
    {"psk_mode",    "PSK",    dialog_psk_remote,    0, 1,         INT32_MIN},   /* 0 PSK31, 1 PSK63 */
    {"psk_freq",    "PSK",    dialog_psk_remote,    200, 3000,    INT32_MIN},
    {"psk_send",    "PSK",    dialog_psk_remote,    1, 0,         INT32_MIN},
    {"psk_stop",    "PSK",    dialog_psk_remote,    1, 0,         INT32_MAX},
    {"olivia_mode", "Olivia", dialog_olivia_remote, 0, 2,         INT32_MIN},   /* 0 8/250, 1 16/500, 2 32/1000 */
    {"olivia_freq", "Olivia", dialog_olivia_remote, 200, 3000,    INT32_MIN},
    {"olivia_send", "Olivia", dialog_olivia_remote, 1, 0,         INT32_MIN},
    {"olivia_stop", "Olivia", dialog_olivia_remote, 1, 0,         INT32_MAX},
    {"jtty_freq",   "JTTY",   dialog_jtty_remote,   200, 2700,    INT32_MIN},
    {"jtty_send",   "JTTY",   dialog_jtty_remote,   1, 0,         INT32_MAX},   /* also while sending: queued */
    {"jtty_stop",   "JTTY",   dialog_jtty_remote,   1, 0,         INT32_MAX},
};

/* The "ax" object of each app (any thread) */
static const struct {
    const char *app;
    int       (*fn)(char *buf, size_t size);
} app_states[] = {
    {"FT", dialog_ft8_remote_state},    {"JS8", dialog_js8_remote_state},     {"WSPR", dialog_wspr_remote_state},
    {"SSTV", dialog_sstv_remote_state}, {"WeFax", dialog_wefax_remote_state}, {"NavTex", dialog_navtex_remote_state},
    {"RTTY", rtty_remote_state},        {"CW", dialog_cw_kbd_remote_state},
    {"Field", dialog_field_remote_state}, {"PSK", dialog_psk_remote_state}, {"Olivia", dialog_olivia_remote_state},
    {"JTTY", dialog_jtty_remote_state},
};

static client_t          clients[MAX_CLIENTS];
static int               wake_fd[2] = {-1, -1};
static std::atomic<int>  subscribers{0};
static std::atomic<int>  pending{0};
static uint64_t          pending_ms;

/* Band id last seen for each "40m ..." name */
static std::mutex                 last_band_mutex;
static std::map<int32_t, int32_t> last_band;

/* Last spectrum line from the DSP thread */
static std::mutex        spec_mutex;
static uint8_t           spec_bins[NBINS];
static uint32_t          spec_center, spec_width;
static bool              spec_new;

/* Event lines from any thread, oldest dropped */
static std::mutex              events_mutex;
static std::deque<std::string> events;

/* Decoded text from any thread by type (remote_text), until the socket thread sends it */
struct text_t {
    std::string text;
    uint64_t    first_ms;
};
static std::mutex                    texts_mutex;
static std::map<std::string, text_t> texts;

static uint64_t now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000;
}

/* LVGL thread */

/* 40 -> "40m", "40m CW", "40m SSB", not "4m" */
static bool band_matches(const std::string &name, int32_t m) {
    char pre[8];
    int  l = snprintf(pre, sizeof(pre), "%im", m);
    return name.compare(0, l, pre) == 0 && (name.size() == (size_t)l || name[l] == ' ');
}

/* Back to the sub-band last seen from the web (40m CW / 40m SSB), else the active band with the
 * lowest start, the first one band up/down reaches. Its own memory is loaded. */
void remote_set_band(int32_t m) {
    BandInfoLoadResult cur = BandsTable::get_by_id(cfg_sm.current_band_id());

    if (cur.rc == SUCCESS && band_matches(cur.value.name, m))
        return;
    int32_t last = BAND_UNDEFINED;
    {
        std::lock_guard<std::mutex> lock(last_band_mutex);
        auto                        it = last_band.find(m);
        if (it != last_band.end())
            last = it->second;
    }
    int32_t  id    = BAND_UNDEFINED;
    uint32_t start = UINT32_MAX;
    for (const BandInfo &b : BandsTable::all_bands()) {
        if (b.type != BAND_ACTIVE || !band_matches(b.name, m) || !cfg_sm.is_valid_hw_freq(b.start_freq))
            continue;
        if (b.id == last) {
            id = b.id;
            break;
        }
        if (b.start_freq < start) {
            id    = b.id;
            start = b.start_freq;
        }
    }
    if (id != BAND_UNDEFINED)
        cfg_sm.p_band_id.set(id);
}

/* "" when none of the remote's apps is open. Any thread. */
static const char *current_app() {
    if (dialog_type_is_run(dialog_ft8))
        return ftx_protocol_name(cfg_sm.p_ft8_protocol.get());
    if (dialog_type_is_run(dialog_js8))
        return "JS8";
    if (dialog_type_is_run(dialog_wspr))
        return "WSPR";
    if (dialog_type_is_run(dialog_sstv))
        return "SSTV";
    if (dialog_type_is_run(dialog_wefax))
        return "WeFax";
    if (dialog_type_is_run(dialog_navtex))
        return "NavTex";
    if (dialog_type_is_run(dialog_cw_kbd))
        return "CW";
    if (dialog_type_is_run(dialog_field))
        return "Field";
    if (dialog_type_is_run(dialog_snake))
        return "Snake";
    if (dialog_type_is_run(dialog_psk))
        return dialog_psk_is_63() ? "PSK63" : "PSK31";
    if (dialog_type_is_run(dialog_olivia))
        return "Olivia";
    if (dialog_type_is_run(dialog_jtty))
        return "JTTY";
    if (rtty_get_state() != RTTY_OFF)
        return "RTTY";
    return "";
}

/* The row's app is open */
static bool app_open(size_t i) {
    const char *app = app_cmds[i].app;

    return !strncmp(current_app(), app, strlen(app));
}

static void open_app(int32_t app) {
    static const press_action_t actions[] = {ACTION_NONE,     ACTION_APP_FT8,   ACTION_APP_FT8,    ACTION_APP_JS8,
                                             ACTION_APP_WSPR, ACTION_APP_SSTV,  ACTION_APP_WEFAX,  ACTION_APP_NAVTEX,
                                             ACTION_APP_RTTY, ACTION_APP_CW_KBD, ACTION_APP_FIELD, ACTION_APP_SNAKE,
                                             ACTION_APP_PSK,  ACTION_APP_PSK,   ACTION_APP_OLIVIA, ACTION_APP_FT8,
                                             ACTION_APP_JTTY};
    const char *cur = current_app();

    if (!app) {
        if (cur[0])
            main_screen_close_app();
        return;
    }
    if (!strcmp(cur, app_names[app]))
        return;
    if (actions[app] == ACTION_APP_FT8)
        cfg_sm.p_ft8_protocol.set(app == 2 ? FTX_PROTOCOL_FT4 : !strcmp(app_names[app], "FT2") ? FTX_PROTOCOL_FT2 : FTX_PROTOCOL_FT8);
    if (actions[app] == ACTION_APP_PSK && dialog_type_is_run(dialog_psk)) {
        dialog_psk_set_mode(!strcmp(app_names[app], "PSK63"));   /* the other one: in place */
        return;
    }
    if (actions[app] == ACTION_APP_PSK)
        dialog_psk_set_mode(!strcmp(app_names[app], "PSK63"));
    main_screen_start_app(actions[app]);
    /* CW: frequency and mode back at the close, as the radio's own apps (restore_radio) */
    if (actions[app] == ACTION_APP_CW_KBD)
        dialog_restore_radio_on_close();

    /* After the start: the app before may have locked the mode. In CW the DSP feeds the CW decoder,
     * not the picture and NavTex decoders. */
    int32_t mode = cfg_sm.cp_cur_mode.get();
    bool    cw   = mode == x6100_mode_cw || mode == x6100_mode_cwr;
    if (main_screen_mode_locked())
        return;
    int32_t to = -1;
    if (actions[app] == ACTION_APP_CW_KBD && !cw)
        to = x6100_mode_cw;
    else if (cw && (actions[app] == ACTION_APP_SSTV || actions[app] == ACTION_APP_WEFAX || actions[app] == ACTION_APP_NAVTEX ||
                    actions[app] == ACTION_APP_PSK || actions[app] == ACTION_APP_OLIVIA ||
                    actions[app] == ACTION_APP_JTTY))
        to = x6100_mode_usb;
    if (to >= 0) {
        cfg_sm.cp_cur_mode.set(to);
        dialog_restore_mode_on_close(mode, to);
    }
}

/* Taken also while the radio transmits */
static bool is_stop(const cmd_t *c) {
    return (c->id == CMD_APP && !c->val) || (c->id == CMD_APP_CMD && c->val < app_cmds[c->app_cmd].stop);
}

static void apply_cmd(void *arg) {
    cmd_t  *c = (cmd_t *)arg;
    int32_t f;

    if (pending > 0)
        pending--;
    if (radio_get_state() != RADIO_RX && !is_stop(c))
        return;
    switch (c->id) {
        case CMD_APP:
            open_app(c->val);
            break;
        case CMD_APP_CMD:
            if (app_open(c->app_cmd))
                app_cmds[c->app_cmd].fn(app_cmds[c->app_cmd].name, c->val, c->text);
            break;
        case CMD_FREQ:
        case CMD_TUNE:
            if (main_screen_freq_locked())
                break;
            f = c->val;
            if (c->id == CMD_TUNE)
                f = align_int(cfg_sm.cp_fg_freq.get() + c->val, abs(c->val));
            if (cfg_sm.is_valid_hw_freq(f))
                main_screen_set_freq(f);
            break;
        case CMD_MODE:
            if (!main_screen_mode_locked())
                cfg_sm.cp_cur_mode.set(c->val);
            break;
        case CMD_BAND:
            if (!main_screen_band_locked())
                remote_set_band(c->val);
            break;
        case CMD_PRE:
            cfg_sm.cp_cur_pre.set(c->val);
            break;
        case CMD_ATT:
            cfg_sm.cp_cur_att.set(c->val);
            break;
        case CMD_AGC:
            main_screen_agc_next();
            break;
        case CMD_NB:
            cfg_sm.p_nb.set(c->val);
            break;
        case CMD_NR:
            cfg_sm.p_nr.set(c->val);
            break;
        case CMD_RFGAIN:
            cfg_sm.p_rfgain.set(c->val);
            break;
        case CMD_SQL:
            cfg_sm.p_squelch.set(c->val);
            break;
        case CMD_PWR:
            cfg_sm.p_pwr.set(c->val / 10.0f);
            break;
        case CMD_FILTER:
            cfg_sm.cp_cur_filter_bw.set(c->val);
            break;
        case CMD_NB_LEVEL:
            cfg_sm.p_nb_level.set(c->val);
            break;
        case CMD_NB_WIDTH:
            cfg_sm.p_nb_width.set(c->val);
            break;
        case CMD_NR_LEVEL:
            cfg_sm.p_nr_level.set(c->val);
            break;
        case CMD_AGC_MODE:
            cfg_sm.cp_cur_agc.set(c->val);
            break;
        case CMD_LOCATION:
            gps_phone_set(c->lat, c->lon);
            station_qth_from_position(c->lat, c->lon);
            break;
        case CMD_QSO_LOG:
            dialog_field_qso_log(c->text);
            break;
    }
}

/* Socket thread */

static void client_close(client_t *c) {
    close(c->fd);
    c->fd = -1;
    if (c->sub) {
        c->sub = false;
        if (--subscribers == 0)
            gps_phone_clear();  /* the QTH goes back to the radio's GPS or the one set */
    }
}

static void client_flush(client_t *c) {
    if (c->fd < 0 || !c->out_len)
        return;
    ssize_t n = send(c->fd, c->out, c->out_len, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (n > 0) {
        c->out_len -= n;
        memmove(c->out, c->out + n, c->out_len);
        c->last_ms = now_ms();
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        client_close(c);
    }
}

/* Whole lines only: a slow client loses lines, never gets half of one */
static void client_send(client_t *c, const char *line, size_t len) {
    if (c->fd < 0 || c->out_len + len > OUT_SIZE)
        return;
    if (!c->out_len)
        c->last_ms = now_ms();
    memcpy(c->out + c->out_len, line, len);
    c->out_len += len;
    client_flush(c);
}

static void client_err(client_t *c, const char *err) {
    char buf[128];
    int  l = snprintf(buf, sizeof(buf), "{\"t\":\"err\",\"err\":\"%s\"}\n", err);
    client_send(c, buf, l);
}

static const char *band_name() {
    static int32_t cached_id = BAND_UNDEFINED - 1;
    static char    name[32];
    int32_t        id = cfg_sm.current_band_id();

    if (id != cached_id) {
        cached_id = id;
        name[0]   = 0;
        BandInfoLoadResult r = BandsTable::get_by_id(id);
        if (r.rc == SUCCESS) {
            /* Edited on the web: keep it plain for the JSON string */
            size_t i;
            for (i = 0; i < r.value.name.size() && i < sizeof(name) - 1; i++) {
                char ch = r.value.name[i];
                name[i] = (ch >= ' ' && ch <= '~' && ch != '"' && ch != '\\') ? ch : '?';
            }
            name[i] = 0;
            std::lock_guard<std::mutex> lock(last_band_mutex);
            int32_t                     m = atoi(name);
            if (m > 0 && band_matches(r.value.name, m))
                last_band[m] = id;
        }
    }
    return name;
}

/* Changes when a log of /mnt/field/logs is written, edited or deleted, by the radio or the
 * web server: the apps reload theirs, so each device sees what another logged. Shared by the
 * clients of one state round (every STATE_MS). */
static uint32_t log_rev() {
    static uint64_t when;
    static uint32_t rev;
    uint64_t        now = get_time();

    if (now - when < STATE_MS / 2)
        return rev;
    when = now;
    rev  = 0;
    DIR *d = opendir("/mnt/field/logs");
    if (!d)
        return rev;
    struct dirent *e;
    struct stat    st;
    char           path[300];
    while ((e = readdir(d))) {
        snprintf(path, sizeof(path), "/mnt/field/logs/%s", e->d_name);
        if (e->d_name[0] != '.' && stat(path, &st) == 0)
            rev = rev * 31 + (uint32_t)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000 + (uint32_t)st.st_size;
    }
    closedir(d);
    return rev;
}

/* This radio's id of the device ping (/mnt/device_id, update_check.c): X6100 Link adds it to its
 * own ping, so the site counts the radio and its app once. "" until the ping thread made it */
static const char *device_id() {
    static char id[40];

    if (!id[0]) {
        FILE *f = fopen("/mnt/device_id", "r");
        if (f) {
            if (fgets(id, sizeof(id), f))
                id[strspn(id, "0123456789abcdef")] = 0;
            fclose(f);
        }
    }
    return id;
}

static int state_line(char *buf, size_t size) {
    int32_t     mode = cfg_sm.cp_cur_mode.get();
    int32_t     agc  = cfg_sm.cp_cur_agc.get();
    const char *app  = current_app();
    char        ax[1600];   /* Field Ops: the nearest references */
    int         l    = -1;

    /* The app's state from its scalars: safe from this thread */
    for (size_t i = 0; app[0] && i < sizeof(app_states) / sizeof(app_states[0]); i++)
        if (!strncmp(app, app_states[i].app, strlen(app_states[i].app))) {
            l = app_states[i].fn(ax, sizeof(ax));
            break;
        }
    if (!app[0])
        l = cw_field_remote_state(ax, sizeof(ax));   /* the main screen's CW line (the Link CW tab) */
    if (l <= 0 || (size_t)l >= sizeof(ax))
        strcpy(ax, "{}");

    /* CW tuning indicator: the offset while a tone is heard (null otherwise) */
    int  cwt_hz;
    char cwt[16] = "null";
    if (cw_tune_recent(&cwt_hz))
        snprintf(cwt, sizeof(cwt), "%d", cwt_hz);

    /* The last spot tuned with the MFK: the app puts its call in the log entry */
    char     spot[16];
    uint32_t spot_seq = dialog_cluster_tuned(spot, sizeof(spot));

    /* Measured TX power and SWR, tenths (the Link TX bar) */
    float tx_pwr, tx_swr;
    tx_info_get(&tx_pwr, &tx_swr);

    return snprintf(buf, size,
                    "{\"t\":\"state\",\"vfo\":\"%s\",\"freq\":%i,\"freq_bg\":%i,\"mode\":\"%s\",\"band\":\"%s\","
                    "\"filter\":%i,\"flo\":%i,\"fhi\":%i,\"rfgain\":%i,\"sql\":%i,\"vol\":%i,\"pre\":%i,\"att\":%i,\"agc\":\"%s\",\"nb\":%i,\"nr\":%i,"
                    "\"nb_level\":%i,\"nb_width\":%i,\"nr_level\":%i,\"pwr\":%i,\"db\":%i,\"tx\":%s,\"app\":\"%s\",\"ax\":%s,"
                    "\"cwt\":%s,\"logrev\":%u,\"did\":\"%s\",\"spot\":\"%s\",\"spotseq\":%u,\"fwd\":%i,\"swr\":%i,\"fw\":\"%s\"}\n",
                    cfg_sm.p_band_current_vfo.get() == X6100_VFO_A ? "A" : "B",
                    cfg_sm.cp_fg_freq.get(), cfg_sm.cp_bg_freq.get(),
                    (mode >= 0 && mode <= x6100_mode_nfm) ? mode_names[mode] : "?", band_name(),
                    cfg_sm.cp_cur_filter_bw.get(), cfg_sm.cp_cur_filter_low.get(), cfg_sm.cp_cur_filter_high.get(),
                    cfg_sm.p_rfgain.get(), cfg_sm.p_squelch.get(), cfg_sm.p_volume.get(),
                    cfg_sm.cp_cur_pre.get() != 0, cfg_sm.cp_cur_att.get() != 0,
                    (agc >= 0 && agc <= x6100_agc_auto) ? agc_names[agc] : "?",
                    cfg_sm.p_nb.get() != 0, cfg_sm.p_nr.get() != 0,
                    cfg_sm.p_nb_level.get(), cfg_sm.p_nb_width.get(), cfg_sm.p_nr_level.get(),
                    (int)lroundf(cfg_sm.p_pwr.get() * 10), meter_get_raw_db(), radio_get_state() == RADIO_RX ? "false" : "true", app, ax, cwt, log_rev(), device_id(), spot, spot_seq,
                    (int)lroundf(tx_pwr * 10), (int)lroundf(tx_swr * 10),
                    strrchr(VERSION, 'v') + 1);   /* "0.125": Link hides what this firmware doesn't have */
}

static void send_state(client_t *c) {
    char buf[2560];     /* Field Ops' ax (1600) and the rest */
    int  l = state_line(buf, sizeof(buf));

    if (l > 0 && (size_t)l < sizeof(buf))
        client_send(c, buf, l);
}

static void send_spectrum() {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t           bins[NBINS];
    uint32_t          center, width;
    char              buf[64 + NBINS * 2];
    int               l;

    {
        std::lock_guard<std::mutex> lock(spec_mutex);
        if (!spec_new)
            return;
        spec_new = false;
        memcpy(bins, spec_bins, NBINS);
        center = spec_center;
        width  = spec_width;
    }
    /* "s": the S-meter rides on every line (15/s) so the app meter follows the audio, not the 5/s state */
    l = snprintf(buf, sizeof(buf), "{\"t\":\"spec\",\"c\":%u,\"w\":%u,\"s\":%d,\"d\":\"", center, width, meter_get_raw_db());
    for (size_t i = 0; i < NBINS; i += 3) {     /* base64, the last group padded */
        uint32_t v = bins[i] << 16 | (i + 1 < NBINS ? bins[i + 1] << 8 : 0) | (i + 2 < NBINS ? bins[i + 2] : 0);
        buf[l++] = b64[v >> 18 & 63];
        buf[l++] = b64[v >> 12 & 63];
        buf[l++] = i + 1 < NBINS ? b64[v >> 6 & 63] : '=';
        buf[l++] = i + 2 < NBINS ? b64[v & 63] : '=';
    }
    l += snprintf(buf + l, sizeof(buf) - l, "\"}\n");
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].sub)
            client_send(&clients[i], buf, l);
}

static void send_events() {
    std::deque<std::string> out;

    {
        std::lock_guard<std::mutex> lock(events_mutex);
        out.swap(events);
    }
    for (const std::string &line : out)
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].sub)
                client_send(&clients[i], line.data(), line.size());
}

/* Decoded text that is due: at a newline, TEXT_MS old or TEXT_MAX long; dropped without subscribers */
static void send_texts(uint64_t now) {
    std::deque<std::string> out;
    bool                    subs = subscribers > 0;

    {
        std::lock_guard<std::mutex> lock(texts_mutex);
        for (auto &it : texts) {
            text_t &t = it.second;
            if (t.text.empty() || (subs && t.text.find('\n') == std::string::npos && t.text.size() < TEXT_MAX &&
                                   now - t.first_ms < TEXT_MS))
                continue;
            if (subs) {
                cJSON *o = cJSON_CreateObject();
                cJSON_AddStringToObject(o, "t", it.first.c_str());
                cJSON_AddStringToObject(o, "text", t.text.c_str());
                char *line = cJSON_PrintUnformatted(o);
                cJSON_Delete(o);
                if (line) {
                    out.emplace_back(line);
                    out.back() += '\n';
                    cJSON_free(line);
                }
            }
            t.text.clear();
        }
    }
    for (const std::string &line : out)
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].sub)
                client_send(&clients[i], line.data(), line.size());
}

/* A picture in tx_pics: [A-Za-z0-9][A-Za-z0-9._ -]*.png */
static bool png_name_ok(const char *s) {
    size_t n = strlen(s);

    if (n < 5 || !isalnum((unsigned char)s[0]) || strcmp(s + n - 4, ".png"))
        return false;
    for (size_t i = 0; i < n; i++)
        if (!isalnum((unsigned char)s[i]) && !strchr("._ -", s[i]))
            return false;
    return true;
}

static bool get_int(cJSON *req, int32_t min, int32_t max, int32_t *out) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(req, "val");

    /* NaN and inf fail the range check */
    if (!cJSON_IsNumber(v) || !(v->valuedouble >= min && v->valuedouble <= max) || v->valuedouble != floor(v->valuedouble))
        return false;
    *out = (int32_t)v->valuedouble;
    return true;
}

/* Validated command, or the error for the client */
static const char *parse_cmd(cJSON *req, const char *name, cmd_t *cmd) {
    static const struct {
        const char *name;
        cmd_id_t    id;
        int32_t     min, max;
    } ints[] = {
        {"freq", CMD_FREQ, SettingsManager::HF_MIN_FREQ, 500000000},
        {"tune", CMD_TUNE, -10000, 10000},
        {"band", CMD_BAND, 1, 999},
        {"pre", CMD_PRE, 0, 1},
        {"att", CMD_ATT, 0, 1},
        {"nb", CMD_NB, 0, 1},
        {"nr", CMD_NR, 0, 1},
        {"rfgain", CMD_RFGAIN, 0, 100},
        {"sql", CMD_SQL, 0, 100},
        {"filter", CMD_FILTER, 50, 6000},
        {"pwr", CMD_PWR, 1, 100},
        {"nb_level", CMD_NB_LEVEL, 0, 100},
        {"nb_width", CMD_NB_WIDTH, 0, 100},
        {"nr_level", CMD_NR_LEVEL, 0, 60},
        {"agc_mode", CMD_AGC_MODE, 0, x6100_agc_auto},   /* agc_names[] */
    };

    cJSON *text = cJSON_GetObjectItemCaseSensitive(req, "text");
    cJSON *val  = cJSON_GetObjectItemCaseSensitive(req, "val");

    cmd->text[0] = 0;
    if (text) {
        if (!cJSON_IsString(text) || strlen(text->valuestring) >= sizeof(cmd->text))
            return "Invalid text";
        strcpy(cmd->text, text->valuestring);
    }
    if (!strcmp(name, "location")) {
        if (sscanf(cmd->text, "%lf,%lf", &cmd->lat, &cmd->lon) != 2 || !(fabs(cmd->lat) <= 90) ||
            !(fabs(cmd->lon) <= 180))   /* NaN fails too */
            return "Invalid position";
        cmd->id = CMD_LOCATION;
        return NULL;
    }
    if (!strcmp(name, "qso_log")) {
        if (!cmd->text[0])
            return "Invalid text";
        cmd->id = CMD_QSO_LOG;
        return NULL;
    }
    if (!strcmp(name, "app")) {
        const char *s = text ? cmd->text : cJSON_IsString(val) ? val->valuestring : "";
        for (int32_t i = 0; i < (int32_t)(sizeof(app_names) / sizeof(app_names[0])); i++)
            if (!strcmp(s, app_names[i])) {
                cmd->id  = CMD_APP;
                cmd->val = i;
                return NULL;
            }
        return "Unknown app";
    }
    bool app_cmd = false;
    for (size_t i = 0; i < sizeof(app_cmds) / sizeof(app_cmds[0]); i++) {
        if (strcmp(name, app_cmds[i].name))
            continue;
        app_cmd = true;
        if (!app_open(i))
            continue;
        cmd->id      = CMD_APP_CMD;
        cmd->app_cmd = i;
        cmd->val     = 0;
        if (!strcmp(name, "sstv_tx")) {
            cmd->val = -1;
            for (int32_t k = 0; k < (int32_t)(sizeof(sstv_modes) / sizeof(sstv_modes[0])); k++)
                if (cJSON_IsString(val) && !strcmp(val->valuestring, sstv_modes[k]))
                    cmd->val = k;
            if (cmd->val < 0)
                return "Invalid mode";
            if (!png_name_ok(cmd->text))
                return "Invalid file name";
        } else if (!strcmp(name, "wefax_tx")) {
            if (!png_name_ok(cmd->text))
                return "Invalid file name";
        } else if (app_cmds[i].min <= app_cmds[i].max && !get_int(req, app_cmds[i].min, app_cmds[i].max, &cmd->val)) {
            return "Invalid value";
        }
        if (!strcmp(name, "wspr_txpct") && cmd->val != 0 && cmd->val != 10 && cmd->val != 20 && cmd->val != 33 &&
            cmd->val != 50)   /* the steps of the TX % button */
            return "Invalid value";
        if (!strcmp(name, "js8_send") && (!cmd->text[0] || strlen(cmd->text) > 160))   /* TX_TEXT_MAX, dialog_js8.c */
            return "Invalid text";
        if ((!strcmp(name, "field_screen") || !strcmp(name, "field_ref") || !strcmp(name, "field_log")) &&
            !cmd->text[0])
            return "Invalid text";
        if ((!strcmp(name, "rtty_send") || !strcmp(name, "cw_send") || !strcmp(name, "psk_send") ||
             !strcmp(name, "olivia_send")) && !cmd->text[0])
            return "Invalid text";
        static const int32_t rtty_rates[] = {4500, 4545, 5000, 5600, 7500, 10000, 11000, 15000};   /* rtty_change_rate() */
        if (!strcmp(name, "rtty_rate") && std::find(std::begin(rtty_rates), std::end(rtty_rates), cmd->val) == std::end(rtty_rates))
            return "Invalid value";
        if (!strcmp(name, "rtty_shift") && cmd->val != 170 && cmd->val != 425 && cmd->val != 450 && cmd->val != 850)
            return "Invalid value";
        if (!strcmp(name, "rtty_center") && cmd->val % 10)
            return "Invalid value";
        return NULL;
    }
    if (app_cmd)
        return !strcmp(name, "app_base") && current_app()[0] ? "No base" : "app not open";
    if (!strcmp(name, "agc")) {
        cmd->id = CMD_AGC;
        return NULL;
    }
    if (!strcmp(name, "mode")) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(req, "val");
        for (int32_t i = 0; i < (int32_t)(sizeof(mode_names) / sizeof(mode_names[0])); i++)
            if (cJSON_IsString(v) && !strcmp(v->valuestring, mode_names[i])) {
                cmd->id  = CMD_MODE;
                cmd->val = i;
                return main_screen_mode_locked() ? "Mode locked by an app" : NULL;
            }
        return "Invalid mode";
    }
    for (size_t i = 0; i < sizeof(ints) / sizeof(ints[0]); i++) {
        if (strcmp(name, ints[i].name))
            continue;
        if (!get_int(req, ints[i].min, ints[i].max, &cmd->val) || (ints[i].id == CMD_TUNE && !cmd->val))
            return "Invalid value";
        cmd->id = ints[i].id;
        if (cmd->id == CMD_FREQ && !cfg_sm.is_valid_hw_freq(cmd->val))
            return "Frequency out of range";
        if ((cmd->id == CMD_FREQ || cmd->id == CMD_TUNE) && main_screen_freq_locked())
            return "Frequency locked";
        if (cmd->id == CMD_BAND && main_screen_band_locked())
            return "Band locked by an app";
        return NULL;
    }
    return "Unknown command";
}

/* Screen options for the web configurator (Display page): {"cmd":"disp_get"} answers
 * {"t":"disp",...}; {"cmd":"disp_set","text":name,"val":v} (the theme: "theme":<name>) is
 * applied on the GUI thread, as the Settings page does it */
static const struct {
    const char         *name;
    Parameter<int32_t> *p;
    int32_t             min, max;
} disp_ints[] = {
    {"wf_mode", &cfg_sm.p_wf_mode, 0, 1},
    {"auto_level", &cfg_sm.p_auto_level_enabled, 0, 1},
    {"wf_speed", &cfg_sm.p_waterfall_speed, 1, 5},
    {"band_plan", &cfg_sm.p_band_plan, 0, 1},
    {"cluster_spots_wf", &cfg_sm.p_cluster_spots_wf, 0, 1},
    {"cluster_column", &cfg_sm.p_cluster_column, 0, 1},
    {"cluster_order", &cfg_sm.p_cluster_order, 0, 1},           /* Newest first (X6100 Link) */
    {"cluster_all_bands", &cfg_sm.p_cluster_all_bands, 0, 1},
    {"smeter_needle", &cfg_sm.p_smeter_needle, 0, 1},
    {"smeter_color", &cfg_sm.p_smeter_color, 0, 2},
    {"wf_palette", &cfg_sm.p_wf_palette, 0, 6},
    {"spectrum_line_color", &cfg_sm.p_spectrum_line_color, -1, 0xffffff},
    {"spectrum_fill_color", &cfg_sm.p_spectrum_fill_color, -1, 0xffffff},
    {"spectrum_fill_gradient", &cfg_sm.p_spectrum_fill_gradient, 0, 1},
    {"smeter_bar_color", &cfg_sm.p_smeter_bar_color, -1, 0xffffff},
    {"swr_color", &cfg_sm.p_swr_color, -1, 0xffffff},
    {"smeter_lamp", &cfg_sm.p_smeter_lamp, 0, 1},
    {"spectrum_show", &cfg_sm.p_spectrum_show, 0, 1},   /* live: main_screen.c follows them */
    {"spectrum_height", &cfg_sm.p_spectrum_height, 60, 160},
    {"digi_text_compact", &cfg_sm.p_digi_text_compact, 0, 1},
    {"knob_info", &cfg_sm.p_knob_info, 0, 1},
    {"walkie", &cfg_sm.p_walkie, 0, 1},                 /* live: walkie.c follows it */
};

static const struct {
    const char    *name;
    params_bool_t *b;
} disp_bools[] = {
    {"wf_center_line", &params.waterfall_center_line},
    {"wf_zoom", &params.waterfall_zoom},
    {"wf_smooth", &params.waterfall_smooth_scroll},
    {"mag_freq", &params.mag_freq},
    {"mag_info", &params.mag_info},
    {"mag_alc", &params.mag_alc},
    {"spectrum_filled", &params.spectrum_filled},
    {"spectrum_peak", &params.spectrum_peak},
};

typedef struct {
    char    name[24];
    char    text[40];
    int32_t val;
} disp_set_t;

static void disp_apply(void *arg) {
    disp_set_t *d = (disp_set_t *)arg;

    for (auto &o : disp_ints)
        if (!strcmp(d->name, o.name))
            o.p->set(d->val);
    for (auto &o : disp_bools)
        if (!strcmp(d->name, o.name))
            params_bool_set(o.b, d->val);
    if (!strcmp(d->name, "auto_level_offset")) {
        cfg_sm.p_auto_level_offset.set(d->val / 10.0f);
    } else if (!strcmp(d->name, "bright_normal") || !strcmp(d->name, "bright_idle")) {
        bool normal = d->name[7] == 'n';
        params_lock();
        if (normal)
            params.brightness_normal = d->val;
        else
            params.brightness_idle = d->val;
        params_unlock(normal ? &params.dirty.brightness_normal : &params.dirty.brightness_idle);
        if (normal)
            backlight_set_brightness(params.brightness_normal);
    } else if (!strcmp(d->name, "bright_timeout")) {
        params_lock();
        params.brightness_timeout = d->val;
        params_unlock(&params.dirty.brightness_timeout);
        backlight_tick();
    } else if (!strcmp(d->name, "bright_buttons")) {
        backlight_set_buttons((buttons_light_t)d->val);
    } else if (!strcmp(d->name, "theme")) {
        /* as Settings: saved and applied live */
        params_str_set(&params.theme_name, d->text);
        params_flush();
        cfg_api_flush_all();
        main_screen_set_theme(d->text);
    }
}

static const char *disp_set(cJSON *req, disp_set_t *d) {
    cJSON *t = cJSON_GetObjectItemCaseSensitive(req, "text");
    if (!cJSON_IsString(t) || strlen(t->valuestring) >= sizeof(d->name))
        return "Invalid option";
    memset(d, 0, sizeof(*d));
    strcpy(d->name, t->valuestring);
    if (!strcmp(d->name, "theme")) {
        cJSON *n = cJSON_GetObjectItemCaseSensitive(req, "theme");

        if (radio_get_state() != RADIO_RX)
            return "Radio is transmitting";     /* the open app closes */
        if (walkie_on())
            return "Not in walkie mode";        /* its screen holds the buttons */
        char   names[64][32];
        int    count = theme_list(names, 64);

        for (int i = 0; cJSON_IsString(n) && i < count; i++)
            if (!strcmp(n->valuestring, names[i])) {
                snprintf(d->text, sizeof(d->text), "%s", names[i]);
                return NULL;
            }
        return "Unknown theme";
    }
    int32_t min = 0, max = 1;
    bool    known = false;
    for (auto &o : disp_ints)
        if (!strcmp(d->name, o.name)) {
            min = o.min, max = o.max, known = true;
        }
    for (auto &o : disp_bools)
        if (!strcmp(d->name, o.name))
            known = true;
    if (!strcmp(d->name, "auto_level_offset"))
        min = -150, max = 150, known = true;   /* tenths of dB */
    if (!strcmp(d->name, "bright_normal") || !strcmp(d->name, "bright_idle"))
        min = -1, max = 9, known = true;
    if (!strcmp(d->name, "bright_timeout"))
        min = 5, max = 120, known = true;
    if (!strcmp(d->name, "bright_buttons"))
        min = 0, max = 2, known = true;
    if (!known)
        return "Unknown option";
    if (!get_int(req, min, max, &d->val))
        return "Invalid value";
    return NULL;
}

static void disp_get(client_t *c) {
    char   buf[4096];
    int    l = snprintf(buf, sizeof(buf), "{\"t\":\"disp\"");
    char   names[64][32];
    int    count = theme_list(names, 64);

    for (auto &o : disp_ints)
        l += snprintf(buf + l, sizeof(buf) - l, ",\"%s\":%d", o.name, (int)o.p->get());
    for (auto &o : disp_bools)
        l += snprintf(buf + l, sizeof(buf) - l, ",\"%s\":%d", o.name, o.b->x ? 1 : 0);
    l += snprintf(buf + l, sizeof(buf) - l,
                  ",\"auto_level_offset\":%d,\"bright_normal\":%d,\"bright_idle\":%d,\"bright_timeout\":%d,"
                  "\"bright_buttons\":%d,\"theme\":\"%s\",\"themes\":[",
                  (int)lroundf(cfg_sm.p_auto_level_offset.get() * 10), params.brightness_normal, params.brightness_idle,
                  params.brightness_timeout, (int)params.brightness_buttons, params.theme_name.x);
    for (int i = 0; i < count && l < (int)sizeof(buf) - 40; i++)
        l += snprintf(buf + l, sizeof(buf) - l, "%s\"%s\"", i ? "," : "", names[i]);
    l += snprintf(buf + l, sizeof(buf) - l, "]}\n");
    if (l > 0 && l < (int)sizeof(buf))
        client_send(c, buf, l);
}

/* Ethernet for the web configurator (Network page): {"cmd":"eth_get"} answers {"t":"eth",...};
 * {"cmd":"eth_set","dhcp":0|1,"ip":..,"mask":..,"gw":..,"dns":..} is checked as Settings checks it
 * (wifi.cpp) and applied on the GUI thread */
static const char *eth_keys[ETH_FIELDS] = {"ip", "mask", "gw", "dns"};

static void eth_get(client_t *c) {
    eth_info_t in;
    char       buf[512];

    eth_get_info(&in);
    int l = snprintf(buf, sizeof(buf), "{\"t\":\"eth\",\"present\":%d,\"connected\":%d,\"state\":\"%s\",\"iface\":\"%s\","
                     "\"addr\":\"%s\",\"gateway\":\"%s\",\"dhcp\":%d",
                     in.present, in.connected, in.state, in.iface, in.addr, in.gw, in.cfg.dhcp);
    for (int i = 0; i < ETH_FIELDS; i++)
        l += snprintf(buf + l, sizeof(buf) - l, ",\"%s\":\"%s\"", eth_keys[i], in.cfg.field[i]);
    l += snprintf(buf + l, sizeof(buf) - l, "}\n");
    client_send(c, buf, l);
}

static const char *eth_set(cJSON *req, eth_cfg_t *cfg) {
    cJSON *dhcp = cJSON_GetObjectItemCaseSensitive(req, "dhcp");

    memset(cfg, 0, sizeof(*cfg));
    if (!cJSON_IsNumber(dhcp) && !cJSON_IsBool(dhcp))
        return "Invalid mode";
    cfg->dhcp = cJSON_IsTrue(dhcp) || (cJSON_IsNumber(dhcp) && dhcp->valueint);
    for (int i = 0; i < ETH_FIELDS; i++) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(req, eth_keys[i]);

        if (v && !cJSON_IsString(v))
            return "Invalid value";
        if (v && strlen(v->valuestring) >= sizeof(cfg->field[i]))
            return "Value too long";
        if (v)
            strcpy(cfg->field[i], v->valuestring);
    }
    return eth_check(cfg);
}

static void eth_apply_cb(void *arg) {
    eth_apply((eth_cfg_t *)arg);
}

static void handle_line(client_t *c, const char *line) {
    cJSON      *req  = cJSON_Parse(line);
    cJSON      *name = cJSON_GetObjectItemCaseSensitive(req, "cmd");
    const char *err  = NULL;
    cmd_t       cmd;

    if (!cJSON_IsString(name)) {
        err = "Bad request";
    } else if (!strcmp(name->valuestring, "get")) {
        send_state(c);
    } else if (!strcmp(name->valuestring, "disp_get")) {
        disp_get(c);
    } else if (!strcmp(name->valuestring, "disp_set")) {
        disp_set_t d;
        if (!(err = disp_set(req, &d))) {
            scheduler_put(disp_apply, &d, sizeof(d));
            client_send(c, "{\"t\":\"ok\"}\n", 11);
        }
    } else if (!strcmp(name->valuestring, "eth_get")) {
        eth_get(c);
    } else if (!strcmp(name->valuestring, "eth_set")) {
        eth_cfg_t cfg;
        if (!(err = eth_set(req, &cfg))) {
            scheduler_put(eth_apply_cb, &cfg, sizeof(cfg));
            client_send(c, "{\"t\":\"ok\"}\n", 11);
        }
    } else if (!strcmp(name->valuestring, "sub")) {
        if (!c->sub && subscribers >= MAX_SUBS) {
            err = "Too many remote pages open";
        } else if (!c->sub) {
            c->sub = true;
            subscribers++;
            send_state(c);
        }
    } else if (!(err = parse_cmd(req, name->valuestring, &cmd))) {
        uint64_t now = now_ms();
        /* A command the scheduler dropped never comes back: don't stay busy for it */
        if (pending >= MAX_PENDING && now - pending_ms > 2000)
            pending = 0;
        if (radio_get_state() != RADIO_RX && !is_stop(&cmd)) {
            err = "Radio is transmitting";
        } else if (pending >= MAX_PENDING) {
            err = "Busy, try again";
        } else {
            pending++;
            pending_ms = now;
            scheduler_put(apply_cmd, &cmd, sizeof(cmd));
            client_send(c, "{\"t\":\"ok\"}\n", 11);
        }
    }
    if (err)
        client_err(c, err);
    cJSON_Delete(req);
}

static void client_read(client_t *c) {
    ssize_t n = recv(c->fd, c->in + c->in_len, IN_SIZE - c->in_len, MSG_DONTWAIT);

    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
        client_close(c);
        return;
    }
    if (n < 0)
        return;
    c->in_len += n;
    char *line = c->in, *end = c->in + c->in_len, *nl;
    while (c->fd >= 0 && (nl = (char *)memchr(line, '\n', end - line))) {
        *nl = 0;
        handle_line(c, line);
        line = nl + 1;
    }
    c->in_len = end - line;
    memmove(c->in, line, c->in_len);
    if (c->fd >= 0 && c->in_len == IN_SIZE)
        client_close(c);    /* no line fits: not our client */
}

static void *server_thread(void *arg) {
    int           listen_fd = *(int *)arg;
    struct pollfd pfd[2 + MAX_CLIENTS];
    int           slot[2 + MAX_CLIENTS];
    uint64_t      next_state = 0;
    int32_t       sent_freq  = 0;

    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;
    while (true) {
        int n   = 0;
        pfd[n++] = (struct pollfd){.fd = listen_fd, .events = POLLIN};
        pfd[n++] = (struct pollfd){.fd = wake_fd[0], .events = POLLIN};
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (clients[i].fd >= 0) {
                slot[n]  = i;
                pfd[n++] = (struct pollfd){.fd = clients[i].fd, .events = (short)(POLLIN | (clients[i].out_len ? POLLOUT : 0))};
            }
        /* Sleeps until a connection when nobody is connected */
        if (poll(pfd, n, n > 2 ? 50 : -1) < 0) {
            if (errno != EINTR)
                usleep(100000);
            continue;
        }
        if (pfd[0].revents & POLLIN) {
            int fd = accept4(listen_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (fd >= 0) {
                int i;
                for (i = 0; i < MAX_CLIENTS && clients[i].fd >= 0; i++) {}
                if (i < MAX_CLIENTS) {
                    clients[i].fd      = fd;
                    clients[i].sub     = false;
                    clients[i].in_len  = 0;
                    clients[i].out_len = 0;
                    clients[i].last_ms = now_ms();
                } else {
                    close(fd);
                }
            }
        }
        if (pfd[1].revents & POLLIN) {
            char buf[64];
            while (read(wake_fd[0], buf, sizeof(buf)) > 0) {}
            send_spectrum();
            send_events();
        }
        for (int k = 2; k < n; k++) {
            client_t *c = &clients[slot[k]];
            if (pfd[k].revents & POLLOUT)
                client_flush(c);
            if (c->fd >= 0 && (pfd[k].revents & POLLIN))
                client_read(c);
            else if (c->fd >= 0 && (pfd[k].revents & (POLLERR | POLLHUP | POLLNVAL)))
                client_close(c);
        }

        uint64_t now = now_ms();
        send_texts(now);
        int32_t  freq  = cfg_sm.cp_fg_freq.get();
        bool     state = subscribers > 0
            && (now >= next_state || (freq != sent_freq && now + STATE_MS >= next_state + TUNE_MS));
        if (state) {
            next_state = now + STATE_MS;
            sent_freq  = freq;
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            client_t *c = &clients[i];
            if (c->fd < 0)
                continue;
            if (c->sub ? (c->out_len && now - c->last_ms > STALL_MS) : now - c->last_ms > IDLE_MS)
                client_close(c);
            else if (c->sub && state)
                send_state(c);
        }
    }
    return NULL;
}

void remote_scope_data(const float *psd_db, size_t n, uint32_t center_hz, uint32_t width_hz, bool tx) {
    static uint64_t last_ms;

    if (!subscribers.load(std::memory_order_relaxed) || !n)
        return;
    uint64_t now = now_ms();
    if (now - last_ms < FRAME_MS)
        return;
    last_ms = now;

    /* Same center and levels as the GUI waterfall (waterfall_data) */
    int32_t lo = cfg_sm.cp_mode_lo_offset.get();
    if (center_hz == 0)
        center_hz = cfg_sm.cp_fg_freq.get() - cfg_sm.p_band_if_shift.get() + lo;
    else if (tx)
        center_hz += lo;
    float min, max;
    waterfall_get_levels(tx, &min, &max);

    /* Max-pool to NBINS, 0..255 over the waterfall levels */
    uint8_t bins[NBINS];
    for (size_t i = 0; i < NBINS; i++) {
        size_t from = i * n / NBINS, to = LV_MAX((i + 1) * n / NBINS, from + 1);
        float  v    = psd_db[from];
        for (size_t j = from + 1; j < to && j < n; j++)
            v = LV_MAX(v, psd_db[j]);
        v       = (v - min) * 255.0f / (max - min);
        bins[i] = v > 0.0f ? (v < 255.0f ? v : 255) : 0;    /* NaN -> 0 */
    }
    {
        /* Never wait for the socket thread here */
        std::unique_lock<std::mutex> lock(spec_mutex, std::try_to_lock);
        if (!lock.owns_lock())
            return;
        memcpy(spec_bins, bins, NBINS);
        spec_center = center_hz;
        spec_width  = width_hz;
        spec_new    = true;
    }
    char b = 0;
    if (write(wake_fd[1], &b, 1) < 0) {
        /* pipe full: the socket thread is already woken */
    }
}

bool remote_subscribed() {
    return subscribers.load(std::memory_order_relaxed) > 0;
}

void remote_event(cJSON *o) {
    char *line = subscribers.load(std::memory_order_relaxed) ? cJSON_PrintUnformatted(o) : NULL;

    cJSON_Delete(o);
    if (!line)
        return;
    {
        std::lock_guard<std::mutex> lock(events_mutex);
        if (events.size() >= MAX_EVENTS)
            events.pop_front();
        events.emplace_back(line);
        events.back() += '\n';
    }
    cJSON_free(line);
    char b = 0;
    if (write(wake_fd[1], &b, 1) < 0) {
        /* pipe full: the socket thread is already woken */
    }
}

void remote_text(const char *type, const char *s, size_t n) {
    bool wake;

    if (!subscribers.load(std::memory_order_relaxed) || !n)
        return;
    {
        std::lock_guard<std::mutex> lock(texts_mutex);
        text_t                     &t = texts[type];
        if (t.text.empty())
            t.first_ms = now_ms();
        for (size_t i = 0; i < n; i++)
            if (s[i] != '\r')   /* plain for the JSON string */
                t.text += (s[i] == '\n' || (s[i] >= ' ' && s[i] <= '~')) ? s[i] : '?';
        wake = t.text.size() >= TEXT_MAX || memchr(s, '\n', n);
    }
    char b = 0;
    if (wake && write(wake_fd[1], &b, 1) < 0) {
        /* pipe full: the socket thread is already woken */
    }
}

void remote_server_init() {
    static int         listen_fd;
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    pthread_t          th;

    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", REMOTE_SOCK);
    unlink(REMOTE_SOCK);
    listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd < 0 || bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(listen_fd, 8) < 0) {
        LV_LOG_ERROR("remote: can't listen on %s", REMOTE_SOCK);
        if (listen_fd >= 0)
            close(listen_fd);
        return;
    }
    chmod(REMOTE_SOCK, 0600);   /* the webserver runs as root */
    if (pipe2(wake_fd, O_NONBLOCK | O_CLOEXEC) < 0) {
        LV_LOG_ERROR("remote: pipe failed");
        close(listen_fd);
        return;
    }
    pthread_create(&th, NULL, server_thread, &listen_fd);
    pthread_detach(th);
}
