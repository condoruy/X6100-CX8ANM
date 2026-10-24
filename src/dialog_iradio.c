/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: radio por Internet. Las emisoras salen del directorio abierto radio-browser.info
 *  (favoritas, las de un país, las más escuchadas o una búsqueda); x6100-iradio baja la emisora y la
 *  decodifica a PCM 44,1 kHz, que va al parlante (o a los auriculares Bluetooth) como el grabador.
 *  Al transmitir, la emisora se corta.
 */
#include "dialog_iradio.h"

#include "audio.h"
#include "buttons.h"
#include "dialog.h"
#include "events.h"
#include "keyboard.h"
#include "msg.h"
#include "radio.h"
#include "scheduler.h"
#include "styles.h"
#include "textarea_window.h"
#include "theme.h"
#include "i18n.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define DIR_IRADIO  "/mnt/iradio"
#define FAV_FILE    DIR_IRADIO "/favorites.tsv"
#define CC_FILE     DIR_IRADIO "/country"
#define STATUS_FILE "/tmp/x6100-iradio.status"
#define HELPER      "/usr/sbin/x6100-iradio"
#define MAX_ST      150

typedef struct {
    char name[64];
    char url[256];
    char codec[9];
    char kbps[8];
    char cc[3];
} station_t;

typedef enum { L_FAV, L_COUNTRY, L_TOP, L_SEARCH, L_COUNT } list_kind_t;

static station_t   list[MAX_ST];
static int         list_n;
static list_kind_t kind = L_FAV;
static char        country[3] = "UY";
static char        query[48];
static char        list_err[96];

static station_t   playing;             /* la que suena */
static volatile bool play_on;
static volatile pid_t play_pid;
static pthread_t   play_thread;
static bool        play_thread_on;

/* la lista se baja en otro hilo; gen descarta las respuestas viejas */
static station_t   fetch_buf[MAX_ST];
static int         fetch_n;
static char        fetch_err[96];
static int         fetch_gen, list_gen;
static bool        fetching;
static pthread_mutex_t fetch_mux = PTHREAD_MUTEX_INITIALIZER;

static lv_obj_t   *title_lbl, *now_lbl, *info_lbl, *table;
static lv_timer_t *status_timer;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void key_cb(lv_event_t *e);
static void play_cb(button_data_t *b);
static void stop_cb(button_data_t *b);
static void fav_cb(button_data_t *b);
static void list_cb(button_data_t *b);
static void search_cb(button_data_t *b);

static button_data_t btn_play   = {.type = BTN_TEXT, .label = "Play", .press = play_cb};
static button_data_t btn_stop   = {.type = BTN_TEXT, .label = "Stop", .press = stop_cb};
static button_data_t btn_fav    = {.type = BTN_TEXT, .label = "Favorite", .press = fav_cb};
static button_data_t btn_list   = {.type = BTN_TEXT, .label = "List", .press = list_cb};
static button_data_t btn_search = {.type = BTN_TEXT, .label = "Search", .press = search_cb};

static buttons_page_t btn_page = {{&btn_play, &btn_stop, &btn_fav, &btn_list, &btn_search}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = NULL,
    .btn_page     = &btn_page,
    .key_cb       = NULL,
};

dialog_t *dialog_iradio = &dialog;

/* ---------- archivos ---------- */

/* nombre \t url \t códec \t kbps \t país */
static bool parse_line(char *line, station_t *s) {
    char *f[5] = {0};
    int   n    = 0;

    line[strcspn(line, "\r\n")] = 0;
    for (char *p = line; n < 5; n++) {
        f[n] = p;
        p    = strchr(p, '\t');
        if (!p) { n++; break; }
        *p++ = 0;
    }
    if (n < 2 || !f[0][0] || strncmp(f[1], "http", 4)) return false;
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "%s", f[0]);
    snprintf(s->url, sizeof(s->url), "%s", f[1]);
    if (n > 2 && f[2]) snprintf(s->codec, sizeof(s->codec), "%s", f[2]);
    if (n > 3 && f[3]) snprintf(s->kbps, sizeof(s->kbps), "%s", f[3]);
    if (n > 4 && f[4]) snprintf(s->cc, sizeof(s->cc), "%s", f[4]);
    return true;
}

static int load_favs(station_t *out, int max) {
    FILE *f = fopen(FAV_FILE, "r");
    char  line[400];
    int   n = 0;

    if (!f) return 0;
    while (n < max && fgets(line, sizeof(line), f)) {
        if (parse_line(line, &out[n])) n++;
    }
    fclose(f);
    return n;
}

static void save_favs(station_t *s, int n) {
    mkdir(DIR_IRADIO, 0755);
    FILE *f = fopen(FAV_FILE ".tmp", "w");
    if (!f) return;
    for (int i = 0; i < n; i++)
        fprintf(f, "%s\t%s\t%s\t%s\t%s\n", s[i].name, s[i].url, s[i].codec, s[i].kbps, s[i].cc);
    fclose(f);
    rename(FAV_FILE ".tmp", FAV_FILE);
}

static bool is_fav(const char *url) {
    static station_t favs[MAX_ST];
    int n = load_favs(favs, MAX_ST);
    for (int i = 0; i < n; i++)
        if (!strcmp(favs[i].url, url)) return true;
    return false;
}

static void load_country(void) {
    FILE *f = fopen(CC_FILE, "r");
    char  cc[8] = "";

    if (f) {
        if (fgets(cc, sizeof(cc), f) && isalpha((unsigned char)cc[0]) && isalpha((unsigned char)cc[1])) {
            country[0] = toupper((unsigned char)cc[0]);
            country[1] = toupper((unsigned char)cc[1]);
            country[2] = 0;
        }
        fclose(f);
    }
}

static void save_country(void) {
    mkdir(DIR_IRADIO, 0755);
    FILE *f = fopen(CC_FILE, "w");
    if (f) {
        fprintf(f, "%s\n", country);
        fclose(f);
    }
}

/* ---------- la lista en pantalla ---------- */

static const char *kind_name(void) {
    static char buf[80];
    switch (kind) {
        case L_FAV:     return tr("Favorites");
        case L_COUNTRY: snprintf(buf, sizeof(buf), "%s %s", tr("Country"), country); return buf;
        case L_TOP:     return tr("Most listened worldwide");
        default:        snprintf(buf, sizeof(buf), "%s: %s", tr("Search"), query); return buf;
    }
}

static void show_title(void) {
    if (!title_lbl) return;
    lv_label_set_text_fmt(title_lbl, "#%06x %s#   #%06x %s#", theme_hex(TC_text_head), tr("INTERNET RADIO"),
                          theme_hex(TC_accent), kind_name());
}

static void show_list(void) {
    if (!table) return;
    lv_table_set_row_cnt(table, 1);
    if (fetching && kind != L_FAV) {
        lv_table_set_cell_value(table, 0, 0, tr("Loading stations..."));
        lv_table_set_cell_value(table, 0, 1, "");
        return;
    }
    if (!list_n) {
        lv_table_set_cell_value(table, 0, 0, list_err[0] ? list_err :
                                kind == L_FAV ? tr("No favorites yet: List shows Uruguay's stations, Favorite saves one")
                                              : tr("No stations"));
        lv_table_set_cell_value(table, 0, 1, "");
        return;
    }
    lv_table_set_row_cnt(table, list_n);
    for (int i = 0; i < list_n; i++) {
        char name[80], info[32];
        bool on = play_on && !strcmp(list[i].url, playing.url);
        snprintf(name, sizeof(name), "%s%s%s", on ? LV_SYMBOL_PLAY " " : "", kind != L_FAV && is_fav(list[i].url) ? "* " : "",
                 list[i].name);
        snprintf(info, sizeof(info), "%s %s%s %s", list[i].codec, list[i].kbps, list[i].kbps[0] ? "k" : "", list[i].cc);
        lv_table_set_cell_value(table, i, 0, name);
        lv_table_set_cell_value(table, i, 1, info);
    }
}

static int selected(void) {
    uint16_t row = 0, col = 0;
    if (!table || !list_n) return -1;
    lv_table_get_selected_cell(table, &row, &col);
    return row == LV_TABLE_CELL_NONE || row >= list_n ? -1 : row;
}

/* ---------- bajar la lista ---------- */

static void fetch_done(void *arg) {
    (void)arg;
    pthread_mutex_lock(&fetch_mux);
    if (fetch_gen == list_gen && kind != L_FAV) {
        memcpy(list, fetch_buf, sizeof(station_t) * fetch_n);
        list_n = fetch_n;
        snprintf(list_err, sizeof(list_err), "%s", fetch_err);
        fetching = false;
    }
    pthread_mutex_unlock(&fetch_mux);
    if (dialog.run) show_list();
}

static void *fetch_thread(void *arg) {
    char *cmd = arg;
    int   gen;
    pthread_mutex_lock(&fetch_mux);
    gen = list_gen;
    pthread_mutex_unlock(&fetch_mux);

    static station_t got[MAX_ST];
    int   n = 0;
    char  err[96] = "", line[512];
    FILE *p = popen(cmd, "r");

    free(cmd);
    if (!p) {
        snprintf(err, sizeof(err), "%s", tr("Cannot run the station finder"));
    } else {
        while (fgets(line, sizeof(line), p)) {
            if (!strncmp(line, "error ", 6)) {
                line[strcspn(line, "\r\n")] = 0;
                snprintf(err, sizeof(err), "%s", line + 6);
            } else if (n < MAX_ST && parse_line(line, &got[n])) {
                n++;
            }
        }
        pclose(p);
    }
    pthread_mutex_lock(&fetch_mux);
    if (gen == list_gen) {
        memcpy(fetch_buf, got, sizeof(station_t) * n);
        fetch_n   = n;
        fetch_gen = gen;
        snprintf(fetch_err, sizeof(fetch_err), "%s", n ? "" : err[0] ? err : tr("No stations"));
    }
    pthread_mutex_unlock(&fetch_mux);
    scheduler_put_noargs(fetch_done);
    return NULL;
}

/* Lo que se puede pasar al shell entre comillas simples: letras, números y poco más */
static void safe_text(const char *in, char *out, size_t size) {
    size_t n = 0;
    for (; *in && n + 1 < size; in++) {
        unsigned char c = *in;
        if (c >= 0x80 || isalnum(c) || strchr(" -_.,&+", c)) out[n++] = c;
    }
    out[n] = 0;
}

static void load_list(void) {
    char cmd[200];

    list_err[0] = 0;
    pthread_mutex_lock(&fetch_mux);
    list_gen++;
    pthread_mutex_unlock(&fetch_mux);
    if (kind == L_FAV) {
        fetching = false;
        list_n   = load_favs(list, MAX_ST);
        show_title();
        show_list();
        return;
    }
    switch (kind) {
        case L_COUNTRY: snprintf(cmd, sizeof(cmd), HELPER " country %s 2>/dev/null", country); break;
        case L_TOP:     snprintf(cmd, sizeof(cmd), HELPER " top 2>/dev/null"); break;
        default:        snprintf(cmd, sizeof(cmd), HELPER " search '%s' 2>/dev/null", query); break;
    }
    fetching = true;
    list_n   = 0;
    show_title();
    show_list();

    pthread_t t;
    if (pthread_create(&t, NULL, fetch_thread, strdup(cmd)) == 0) {
        pthread_detach(t);
    } else {
        fetching = false;
        snprintf(list_err, sizeof(list_err), "%s", tr("Cannot run the station finder"));
        show_list();
    }
}

/* ---------- reproducción ---------- */

static void stop_play(void) {
    play_on = false;
    pid_t pid = play_pid;
    if (pid > 0) kill(-pid, SIGTERM);      /* todo el grupo: el ayudante y su decodificador */
    if (play_thread_on) {
        pthread_join(play_thread, NULL);
        play_thread_on = false;
    }
}

static void play_ended(void *arg) {
    (void)arg;
    if (dialog.run) show_list();
}

static void *play_fn(void *arg) {
    (void)arg;
    int fds[2];

    if (pipe(fds) != 0) {
        play_on = false;
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
        execl(HELPER, HELPER, "play", playing.url, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);
    if (pid < 0) {
        close(fds[0]);
        play_on = false;
        return NULL;
    }
    play_pid = pid;

    /* En los auriculares Bluetooth si están; si no, el parlante como el grabador */
    bool bt = audio_play_bt_begin();
    if (!bt) radio_set_play_mode(AUDIO_PLAY_ON);

    static int16_t buf[2048];
    size_t         have = 0;     /* bytes en buf */
    while (play_on) {
        ssize_t r = read(fds[0], (char *)buf + have, sizeof(buf) - have);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        have += r;
        size_t samples = have / 2;
        if (samples && play_on) audio_play(buf, samples);
        if (have & 1) ((char *)buf)[0] = ((char *)buf)[have - 1];
        have &= 1;
    }
    close(fds[0]);
    kill(-pid, SIGTERM);
    waitpid(pid, NULL, 0);
    play_pid = 0;

    audio_play_flush();
    if (bt) audio_play_bt_end();
    else radio_set_play_mode(AUDIO_PLAY_OFF);
    play_on = false;
    scheduler_put_noargs(play_ended);
    return NULL;
}

static void start_play(const station_t *s) {
    if (radio_get_state() != RADIO_RX) return;
    stop_play();
    playing = *s;
    play_on = true;
    if (pthread_create(&play_thread, NULL, play_fn, NULL) == 0) {
        play_thread_on = true;
    } else {
        play_on = false;
    }
    show_list();
}

/* Lo que cuenta el ayudante: estado, códec, título */
static void status_cb(lv_timer_t *t) {
    (void)t;
    char  st[128] = "", detail[96] = "", title[160] = "";
    FILE *f = fopen(STATUS_FILE, "r");

    if (f) {
        if (fgets(st, sizeof(st), f)) st[strcspn(st, "\n")] = 0;
        if (fgets(detail, sizeof(detail), f)) detail[strcspn(detail, "\n")] = 0;
        if (fgets(title, sizeof(title), f)) title[strcspn(title, "\n")] = 0;
        fclose(f);
    }
    if (!now_lbl) return;
    if (!playing.name[0]) {
        lv_label_set_text(now_lbl, tr("Choose a station and press Play (or the MFK knob)"));
        lv_label_set_text(info_lbl, "");
        return;
    }
    lv_label_set_text_fmt(now_lbl, "#%06x %s %s#", theme_hex(play_on ? TC_accent : TC_text_dim),
                          play_on ? LV_SYMBOL_PLAY : LV_SYMBOL_STOP, playing.name);
    if (!strncmp(st, "error", 5)) {
        lv_label_set_text_fmt(info_lbl, "#%06x %s#", theme_hex(TC_err), st[5] ? st + 6 : tr("Error"));
    } else if (!play_on) {
        lv_label_set_text(info_lbl, tr("Stopped"));
    } else if (!strcmp(st, "playing")) {
        lv_label_set_text_fmt(info_lbl, "#%06x %s#  %s", theme_hex(TC_text_dim), detail, title);
    } else {
        lv_label_set_text_fmt(info_lbl, "#%06x %s#  %s", theme_hex(TC_text_dim), tr("Connecting..."), detail);
    }
}

/* ---------- botones y teclas ---------- */

static void play_cb(button_data_t *b) {
    (void)b;
    int i = selected();
    if (i < 0) {
        msg_update_text_fmt("%s", tr("Choose a station first"));
        return;
    }
    if (radio_get_state() != RADIO_RX) return;
    start_play(&list[i]);
}

static void stop_cb(button_data_t *b) {
    (void)b;
    stop_play();
    show_list();
}

static void fav_cb(button_data_t *b) {
    (void)b;
    static station_t favs[MAX_ST];
    int i = selected();
    if (i < 0) return;
    station_t s = list[i];
    int n = load_favs(favs, MAX_ST), k;
    for (k = 0; k < n && strcmp(favs[k].url, s.url); k++) {}
    if (k < n) {
        memmove(&favs[k], &favs[k + 1], sizeof(station_t) * (n - k - 1));
        n--;
        msg_update_text_fmt("%s: %s", tr("Removed from favorites"), s.name);
    } else if (n < MAX_ST) {
        favs[n++] = s;
        msg_update_text_fmt("%s: %s", tr("Added to favorites"), s.name);
    }
    save_favs(favs, n);
    if (kind == L_FAV) load_list();
    else show_list();
}

static void list_cb(button_data_t *b) {
    (void)b;
    kind = (kind + 1) % L_COUNT;
    if (kind == L_SEARCH && !query[0]) kind = L_FAV;
    load_list();
}

static bool search_ok_cb(void) {
    const char *t = textarea_window_get();
    char        clean[48];

    safe_text(t ? t : "", clean, sizeof(clean));
    /* "pais AR": las emisoras de otro país (código de dos letras) */
    if ((!strncasecmp(clean, "pais ", 5) || !strncasecmp(clean, "país ", 6) || !strncasecmp(clean, "country ", 8))) {
        const char *cc = strrchr(clean, ' ') + 1;
        if (strlen(cc) == 2 && isalpha((unsigned char)cc[0]) && isalpha((unsigned char)cc[1])) {
            country[0] = toupper((unsigned char)cc[0]);
            country[1] = toupper((unsigned char)cc[1]);
            country[2] = 0;
            save_country();
            kind = L_COUNTRY;
            textarea_window_close();
            lv_group_add_obj(keyboard_group, table);
            lv_group_set_editing(keyboard_group, true);
            load_list();
            return true;
        }
    }
    if (clean[0]) {
        snprintf(query, sizeof(query), "%s", clean);
        kind = L_SEARCH;
    }
    textarea_window_close();
    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);
    if (clean[0]) load_list();
    return true;
}

static bool search_cancel_cb(void) {
    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);
    return true;
}

static void search_cb(button_data_t *b) {
    (void)b;
    lv_group_remove_obj(table);
    textarea_window_open_w_label(search_ok_cb, search_cancel_cb, tr("Station or genre (\"pais AR\": another country)"));
    textarea_window_set(query);
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
        case LV_KEY_ESC:
            dialog_destruct();
            break;
        case LV_KEY_ENTER:
            play_cb(NULL);
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

static void tx_cb(lv_event_t *e) {
    (void)e;
    if (play_on) {      /* nunca al aire: se corta antes de transmitir */
        stop_play();
        show_list();
    }
}

/* ---------- abrir y cerrar ---------- */

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);
    lv_obj_add_event_cb(dialog.obj, tx_cb, EVENT_RADIO_TX, NULL);

    title_lbl = lv_label_create(dialog.obj);
    lv_label_set_recolor(title_lbl, true);
    lv_obj_set_style_text_font(title_lbl, &sony_22, 0);
    lv_obj_set_pos(title_lbl, 14, 6);

    now_lbl = lv_label_create(dialog.obj);
    lv_label_set_recolor(now_lbl, true);
    lv_label_set_long_mode(now_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(now_lbl, 770);
    lv_obj_set_style_text_font(now_lbl, &sony_24, 0);
    lv_obj_set_style_text_color(now_lbl, TC(text), 0);
    lv_obj_set_pos(now_lbl, 14, 36);

    info_lbl = lv_label_create(dialog.obj);
    lv_label_set_recolor(info_lbl, true);
    lv_label_set_long_mode(info_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(info_lbl, 770);
    lv_obj_set_style_text_font(info_lbl, &sony_20, 0);
    lv_obj_set_style_text_color(info_lbl, TC(text_value), 0);
    lv_obj_set_pos(info_lbl, 14, 66);

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_set_pos(table, 10, 96);
    lv_obj_set_size(table, 780, DIALOG_H - 104);
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

    load_country();
    /* Sin favoritas todavía: las emisoras del país */
    kind = load_favs(list, MAX_ST) ? L_FAV : L_COUNTRY;
    load_list();

    status_timer = lv_timer_create(status_cb, 500, NULL);
    status_cb(NULL);
}

static void destruct_cb(void) {
    stop_play();
    playing.name[0] = 0;
    textarea_window_close();
    if (status_timer) lv_timer_del(status_timer);
    status_timer = NULL;
    pthread_mutex_lock(&fetch_mux);
    list_gen++;             /* una lista que llega tarde ya no se muestra */
    pthread_mutex_unlock(&fetch_mux);
    fetching = false;
    title_lbl = now_lbl = info_lbl = table = NULL;
}
