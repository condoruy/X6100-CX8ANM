/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: Zello en el X6100. x6100-zello se conecta al canal (Zello Channel API) y entrega el audio
 *  que llega como PCM 16 kHz, que acá se pasa a 44,1 kHz y va al parlante (o a los auriculares
 *  Bluetooth) como la radio por Internet. Para hablar: el PTT del costado de la radio o el botón Hablar;
 *  la voz sale del micrófono de mano (sin apretar su PTT) y va a Zello, nunca al aire.
 *
 *  Usuario, clave, llaves de desarrollador y canales: web de la radio, página Zello (/mnt/zello.conf).
 */
#include "dialog_zello.h"

#include "audio.h"
#include "buttons.h"
#include "dialog.h"
#include "events.h"
#include "keyboard.h"
#include "msg.h"
#include "radio.h"
#include "scheduler.h"
#include "styles.h"
#include "theme.h"
#include "i18n.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#define CONF_FILE   "/mnt/zello.conf"
#define STATUS_FILE "/tmp/x6100-zello.status"
#define TX_SOCK     "/tmp/x6100-zello.sock"
#define HELPER      "/usr/sbin/x6100-zello"
#define ZRATE       16000
#define MAX_CH      20

static char  channels[MAX_CH][64];
static int   channels_n;
static char  cur_channel[64];
static char  user[64];

static volatile bool  run_on;           /* conectado (o conectándose) */
static volatile bool  run_alive;        /* el hilo todavía no terminó */
static volatile pid_t run_pid;
static bool           restart_pending;

static volatile bool  run_bt;           /* el audio va a los auriculares Bluetooth */
static volatile bool  tx_on;            /* hablando por Zello */
static volatile int   tx_fd = -1;
static bool           tx_by_ptt;

static lv_obj_t   *title_lbl, *state_lbl, *talk_lbl, *table;
static lv_timer_t *status_timer;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void power_cb(button_data_t *b);
static void talk_cb(button_data_t *b);
static void reload_cb(button_data_t *b);

static button_data_t btn_power  = {.type = BTN_TEXT, .label = "Connect /\nDisconnect", .press = power_cb};
static button_data_t btn_talk   = {.type = BTN_TEXT, .label = "Talk", .press = talk_cb};
static button_data_t btn_reload = {.type = BTN_TEXT, .label = "Reload\nchannels", .press = reload_cb};

static buttons_page_t btn_page = {{&btn_power, &btn_talk, &btn_reload}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = NULL,
    .btn_page     = &btn_page,
    .key_cb       = NULL,
};

dialog_t *dialog_zello = &dialog;

/* ---------- configuración ---------- */

static void unquote(char *v) {
    size_t n = strlen(v);
    if (n >= 2 && (v[0] == '"' || v[0] == '\'') && v[n - 1] == v[0]) {
        memmove(v, v + 1, n - 2);
        v[n - 2] = 0;
    }
}

static void load_conf(void) {
    FILE *f = fopen(CONF_FILE, "r");
    char  line[1024];

    channels_n = 0;
    user[0]    = 0;
    if (!f) return;
    char saved[64] = "";
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *v = eq + 1;
        unquote(v);
        if (!strcmp(line, "USER")) {
            snprintf(user, sizeof(user), "%s", v);
        } else if (!strcmp(line, "CHANNEL")) {
            snprintf(saved, sizeof(saved), "%s", v);
        } else if (!strcmp(line, "CHANNELS")) {
            for (char *p = strtok(v, "|"); p && channels_n < MAX_CH; p = strtok(NULL, "|")) {
                while (*p == ' ') p++;
                if (*p) snprintf(channels[channels_n++], sizeof(channels[0]), "%s", p);
            }
        }
    }
    fclose(f);
    if (!cur_channel[0]) snprintf(cur_channel, sizeof(cur_channel), "%s", saved[0] ? saved : (channels_n ? channels[0] : ""));
}

/* ---------- audio que llega: 16 kHz -> 44,1 kHz ---------- */

static void end_child(pid_t pid) {
    kill(-pid, SIGTERM);
    for (int i = 0; i < 40; i++) {
        if (waitpid(pid, NULL, WNOHANG) == pid) return;
        usleep(50000);
    }
    kill(-pid, SIGKILL);
    waitpid(pid, NULL, 0);
}

static void run_ended(void *arg);

static void *run_fn(void *arg) {
    char *channel = arg;
    int   fds[2];

    if (pipe(fds) != 0) {
        free(channel);
        run_on = run_alive = false;
        scheduler_put_noargs(run_ended);
        return NULL;
    }
    unlink(STATUS_FILE);
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) dup2(dn, 2);
        execl(HELPER, HELPER, "run", channel, (char *)NULL);
        _exit(127);
    }
    free(channel);
    close(fds[1]);
    if (pid < 0) {
        close(fds[0]);
        run_on = run_alive = false;
        scheduler_put_noargs(run_ended);
        return NULL;
    }
    setpgid(pid, pid);
    run_pid = pid;

    bool bt = audio_play_bt_begin();
    run_bt  = bt;
    if (!bt) radio_set_play_mode(AUDIO_PLAY_ON);

    static int16_t in[1024];
    static int16_t out[1024 * 3 + 8];
    double         pos  = 0;        /* posición en la entrada, en muestras de 16 kHz */
    int16_t        prev = 0;
    size_t         have = 0;
    const double   step = (double)ZRATE / AUDIO_PLAY_RATE;

    while (run_on) {
        ssize_t r = read(fds[0], (char *)in + have, sizeof(in) - have);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        have += r;
        size_t n = have / 2, o = 0;
        /* interpolación lineal entre la última muestra del bloque anterior y las nuevas */
        while (pos < n) {
            double  fl = floor(pos);
            int     i  = (int)fl;
            int16_t a  = i == 0 ? prev : in[i - 1];
            int16_t b  = in[i];
            double  t  = pos - fl;
            out[o++]   = (int16_t)(a + (b - a) * t);
            pos += step;
        }
        pos -= n;
        prev = in[n - 1];
        if (o && audio_play_until(out, o, &run_on, 3000) == -2) break;
        if (have & 1) ((char *)in)[0] = ((char *)in)[have - 1];
        have &= 1;
    }
    run_pid = 0;
    end_child(pid);
    close(fds[0]);

    audio_play_flush();
    if (bt) audio_play_bt_end();
    else radio_set_play_mode(AUDIO_PLAY_OFF);
    run_on    = false;
    run_alive = false;
    scheduler_put_noargs(run_ended);
    return NULL;
}

static void launch(void);
static void show_all(void);
static void tx_stop(void);

static void stop_run(void) {
    tx_stop();
    run_on = false;
    pid_t pid = run_pid;
    if (pid > 0) kill(-pid, SIGTERM);
}

static void run_ended(void *arg) {
    (void)arg;
    if (restart_pending && !run_alive && dialog.run) {
        restart_pending = false;
        launch();
    }
    if (dialog.run) show_all();
}

static void launch(void) {
    pthread_t t;

    if (!cur_channel[0]) {
        msg_update_text_fmt("%s", tr("Add your Zello channels on the radio's web page (Zello page)"));
        return;
    }
    run_on    = true;
    run_alive = true;
    if (pthread_create(&t, NULL, run_fn, strdup(cur_channel)) == 0) {
        pthread_detach(t);
    } else {
        run_on = run_alive = false;
    }
}

static void connect_to(const char *channel) {
    snprintf(cur_channel, sizeof(cur_channel), "%s", channel);
    if (run_alive) {
        stop_run();
        restart_pending = true;
    } else {
        launch();
    }
    show_all();
}

/* ---------- hablar: micrófono de mano -> 16 kHz -> x6100-zello ---------- */

typedef struct { float b0, b1, b2, a1, a2, z1, z2; } bq_t;

static bq_t lp[2], hp;

static void bq_set(bq_t *f, bool high, float hz, float q) {
    float w = 2.0f * (float)M_PI * hz / AUDIO_CAPTURE_RATE, c = cosf(w), a = sinf(w) / (2.0f * q), a0 = 1.0f + a;
    if (high) {
        f->b0 = (1.0f + c) / 2.0f / a0; f->b1 = -(1.0f + c) / a0; f->b2 = f->b0;
    } else {
        f->b0 = (1.0f - c) / 2.0f / a0; f->b1 = (1.0f - c) / a0; f->b2 = f->b0;
    }
    f->a1 = -2.0f * c / a0;
    f->a2 = (1.0f - a) / a0;
    f->z1 = f->z2 = 0;
}

static inline float bq_run(bq_t *f, float x) {
    float y = f->b0 * x + f->z1;
    f->z1   = f->b1 * x - f->a1 * y + f->z2;
    f->z2   = f->b2 * x - f->a2 * y;
    return y;
}

static double cap_pos;

bool dialog_zello_capturing(void) {
    return tx_on && tx_fd >= 0;
}

/* Hilo de audio: filtro 250-3400 Hz y paso a 16 kHz; lo que no entra en el socket se pierde (nunca espera) */
void dialog_zello_put_mic(size_t nsamples, const int16_t *samples) {
    int16_t      out[nsamples / 2 + 4];
    size_t       o    = 0;
    const double step = (double)AUDIO_CAPTURE_RATE / ZRATE;
    int          fd   = tx_fd;

    if (fd < 0) return;
    for (size_t i = 0; i < nsamples; i++) {
        float y = bq_run(&hp, samples[i]);
        y       = bq_run(&lp[1], bq_run(&lp[0], y));
        if (cap_pos <= 0) {
            out[o++] = (int16_t)(y > 32767 ? 32767 : y < -32767 ? -32767 : y);
            cap_pos += step;
        }
        cap_pos -= 1.0;
    }
    if (o) {
        ssize_t r = send(fd, out, o * 2, MSG_DONTWAIT | MSG_NOSIGNAL);
        (void)r;
    }
}

static void tx_start(bool by_ptt) {
    if (tx_on) return;
    if (!run_on) {
        msg_update_text_fmt("%s", tr("Connect to a Zello channel first"));
        return;
    }
    if (radio_get_state() != RADIO_RX) return;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    snprintf(a.sun_path, sizeof(a.sun_path), "%s", TX_SOCK);
    if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
        if (fd >= 0) close(fd);
        msg_update_text_fmt("%s", tr("Zello is not ready to talk yet"));
        return;
    }
    bq_set(&hp, true, 250.0f, 0.707f);
    bq_set(&lp[0], false, 3400.0f, 0.707f);
    bq_set(&lp[1], false, 3400.0f, 0.707f);
    cap_pos   = 0;
    tx_by_ptt = by_ptt;
    tx_fd     = fd;
    tx_on     = true;
    radio_set_play_mode(AUDIO_PLAY_VOICE_REC);    /* la base manda el micrófono de mano */
    show_all();
}

static void tx_stop(void) {
    if (!tx_on) return;
    tx_on   = false;
    int fd  = tx_fd;
    tx_fd   = -1;
    usleep(20000);                                /* el hilo de audio suelta el socket */
    if (fd >= 0) close(fd);
    radio_set_play_mode(run_on && !run_bt ? AUDIO_PLAY_ON : AUDIO_PLAY_OFF);
    if (dialog.run) show_all();
}

bool dialog_zello_ptt(bool press) {
    if (!dialog.run) return false;
    if (press) tx_start(true);
    else if (tx_by_ptt) tx_stop();
    return true;
}

/* ---------- pantalla ---------- */

static void show_all(void) {
    if (!table) return;
    lv_label_set_text_fmt(title_lbl, "#%06x ZELLO#  #%06x %s#", theme_hex(TC_accent), theme_hex(TC_text_dim),
                          user[0] ? user : tr("no user"));
    lv_table_set_row_cnt(table, channels_n ? channels_n : 1);
    if (!channels_n) {
        lv_table_set_cell_value(table, 0, 0, tr("No channels: add them on the radio's web page (Zello page)"));
        lv_table_set_cell_value(table, 0, 1, "");
    }
    for (int i = 0; i < channels_n; i++) {
        bool cur = run_on && !strcmp(channels[i], cur_channel);
        lv_table_set_cell_value_fmt(table, i, 0, "%s %s", cur ? LV_SYMBOL_PLAY : " ", channels[i]);
        lv_table_set_cell_value(table, i, 1, cur ? tr("connected") : "");
    }
}

static void status_cb(lv_timer_t *t) {
    (void)t;
    char  st[160] = "", ch[128] = "", talker[96] = "", tx[8] = "";
    FILE *f = fopen(STATUS_FILE, "r");

    if (f) {
        if (fgets(st, sizeof(st), f)) st[strcspn(st, "\n")] = 0;
        if (fgets(ch, sizeof(ch), f)) ch[strcspn(ch, "\n")] = 0;
        if (fgets(talker, sizeof(talker), f)) talker[strcspn(talker, "\n")] = 0;
        if (fgets(tx, sizeof(tx), f)) tx[strcspn(tx, "\n")] = 0;
        fclose(f);
    }
    if (!state_lbl) return;
    char *users = strchr(ch, '\t');
    if (users) *users++ = 0;
    if (!run_on) {
        lv_label_set_text_fmt(state_lbl, "#%06x %s#", theme_hex(TC_text_dim),
                              tr("Disconnected: choose a channel and press the MFK or Connect"));
    } else if (!strncmp(st, "error", 5)) {
        lv_label_set_text_fmt(state_lbl, "#%06x %s#", theme_hex(TC_err), st[5] ? st + 6 : tr("Error"));
    } else if (!strcmp(st, "online")) {
        lv_label_set_text_fmt(state_lbl, "#%06x %s %s#  %s%s%s", theme_hex(TC_accent), LV_SYMBOL_PLAY, cur_channel,
                              users && users[0] ? users : "", users && users[0] ? " " : "", users && users[0] ? tr("online") : "");
    } else {
        lv_label_set_text_fmt(state_lbl, "#%06x %s %s#", theme_hex(TC_text_dim), tr("Connecting..."), cur_channel);
    }
    if (tx_on) {
        lv_label_set_text_fmt(talk_lbl, "#%06x %s %s#", theme_hex(TC_err), "TX", tr("Talking: speak into the hand mic"));
    } else if (run_on && talker[0]) {
        lv_label_set_text_fmt(talk_lbl, "#%06x %s %s#", theme_hex(TC_accent), LV_SYMBOL_PLAY, talker);
    } else {
        lv_label_set_text_fmt(talk_lbl, "#%06x %s#", theme_hex(TC_text_dim),
                              run_on ? tr("Side PTT or Talk: speak into the hand mic (without its PTT)") : "");
    }
}

static int selected(void) {
    uint16_t row = 0, col = 0;
    if (!table || !channels_n) return -1;
    lv_table_get_selected_cell(table, &row, &col);
    return row == LV_TABLE_CELL_NONE || row >= channels_n ? -1 : row;
}

static void power_cb(button_data_t *b) {
    (void)b;
    if (run_on || run_alive) {
        restart_pending = false;
        stop_run();
        show_all();
        return;
    }
    int i = selected();
    if (i >= 0) snprintf(cur_channel, sizeof(cur_channel), "%s", channels[i]);
    launch();
    show_all();
}

static void talk_cb(button_data_t *b) {
    (void)b;
    if (tx_on) tx_stop();
    else tx_start(false);
}

static void reload_cb(button_data_t *b) {
    (void)b;
    load_conf();
    show_all();
    msg_update_text_fmt("%s: %d", tr("Zello channels"), channels_n);
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct();
            break;
        case LV_KEY_ENTER: {
            int i = selected();
            if (i >= 0) {
                if (run_on && !strcmp(channels[i], cur_channel)) power_cb(NULL);
                else connect_to(channels[i]);
            }
            break;
        }
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

/* Si la radio sale al aire (PTT del micrófono de mano), se deja de hablar por Zello */
static void radio_tx_cb(lv_event_t *e) {
    (void)e;
    tx_stop();
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);
    lv_obj_add_event_cb(dialog.obj, radio_tx_cb, EVENT_RADIO_TX, NULL);

    title_lbl = lv_label_create(dialog.obj);
    lv_label_set_recolor(title_lbl, true);
    lv_obj_set_style_text_font(title_lbl, &sony_22, 0);
    lv_obj_set_pos(title_lbl, 14, 6);

    state_lbl = lv_label_create(dialog.obj);
    lv_label_set_recolor(state_lbl, true);
    lv_label_set_long_mode(state_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(state_lbl, 770);
    lv_obj_set_style_text_font(state_lbl, &sony_24, 0);
    lv_obj_set_style_text_color(state_lbl, TC(text), 0);
    lv_obj_set_pos(state_lbl, 14, 36);

    talk_lbl = lv_label_create(dialog.obj);
    lv_label_set_recolor(talk_lbl, true);
    lv_label_set_long_mode(talk_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(talk_lbl, 770);
    lv_obj_set_style_text_font(talk_lbl, &sony_22, 0);
    lv_obj_set_style_text_color(talk_lbl, TC(text_value), 0);
    lv_obj_set_pos(talk_lbl, 14, 68);

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_set_pos(table, 10, 100);
    lv_obj_set_size(table, 780, DIALOG_H - 108);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_col_width(table, 0, 590);
    lv_table_set_col_width(table, 1, 180);
    lv_obj_set_style_border_width(table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, TC(text), LV_PART_ITEMS);
    lv_obj_set_style_text_font(table, &sony_22, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(table, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, TC(focus_text), LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_color(table, TC(focus), LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_set_style_bg_opa(table, 128, LV_PART_ITEMS | LV_STATE_EDITED);
    lv_obj_add_event_cb(table, key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);

    buttons_unload_page();
    buttons_load_page(&btn_page);

    load_conf();
    show_all();
    for (int i = 0; i < channels_n; i++)
        if (!strcmp(channels[i], cur_channel)) {
            lv_table_t *t = (lv_table_t *)table;    /* la tabla recuerda la celda elegida */
            t->row_act    = i;
            t->col_act    = 0;
            lv_obj_invalidate(table);
        }

    status_timer = lv_timer_create(status_cb, 400, NULL);
    status_cb(NULL);
}

static void destruct_cb(void) {
    restart_pending = false;
    stop_run();
    if (status_timer) lv_timer_del(status_timer);
    status_timer = NULL;
    title_lbl = state_lbl = talk_lbl = table = NULL;
}
