/*
 * NAVTEX / SITOR-B receiver, and DSC (ITU-R M.493, receive only) on the same demodulator
 */

#include "dialog_navtex.h"
#include "navtex/decoder.h"
#include "navtex/dsc.h"
#include "ft8/table_view.h"
#include "flags.h"
#include "cJSON.h"
#include "dialog.h"
#include "styles.h"
#include "theme.h"
#include "buttons.h"
#include "keyboard.h"
#include "radio.h"
#include "mfk.h"
#include "events.h"
#include "freq_list.h"
#include "freq_strip.h"
#include "app_frame.h"
#include "cfg/cfg_api.h"
#include "dsp.h"
#include "remote_server.h"
#include "msg.h"
#include "main_screen.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define NAVTEX_TEST_WAV "/mnt/navtex_test.wav"
#define NAVTEX_SAVE_DIR "/mnt/navtex"   /* navtex-YYYYMMDD-HHMMSS.txt, on the web Files page (idea from 1KO125) */
#define NAVTEX_TEST_CHUNK 256

#define NAVTEX_TEXT_MAX 65536 //32768
#define NAVTEX_PENDING_MAX 512
#define NAVTEX_UI_TIMER_MS 50

#define NAVTEX_SCROLL_STEP 36

#define DSC_ROWS_MAX 100
#define DSC_PENDING_MAX 8
#define DSC_REPEAT_S 120      /* a distress alert is repeated: the same call again within this is skipped */

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void audio_cb(unsigned int n, float *samples);
static void close_cb(button_data_t *data);
static void clear_cb(button_data_t *data);
static void save_cb(button_data_t *data);
static void test_cb(button_data_t *data);
static void freq_cb(button_data_t *data);
static void *test_thread_cb(void *arg);
static bool start_test(void);
static void stop_test(void);
static bool reset_decoder(void);
static void ui_timer_cb(lv_timer_t *timer);
static void char_cb(int c, void *userdata);
static void key_cb(lv_event_t *e);
static void mode_cb(button_data_t *data);
static void page_cb(button_data_t *data);
static const char *mode_label(void);
static const char *page1_label(void);
static const char *page2_label(void);
static void mark_cb(button_data_t *data);
static void scan_cb(button_data_t *data);
static const char *mark_label(void);
static const char *scan_label(void);
static void scan_stop(const char *why);
static void scan_tick(void);

static navtex_decoder_t *decoder = NULL;

/* DSC: the mode, and the calls from the audio thread (under text_mutex) to the UI */
static volatile bool dsc_mode = false;
static volatile bool dsc_reset = true;
static dsc_rx_t dsc_rx;
static dsc_call_t dsc_pending[DSC_PENDING_MAX];
static int dsc_npending = 0;
static lv_obj_t *dsc_list = NULL;
static lv_font_t dsc_font;    /* sony_18 with Montserrat behind it for the degree sign and the arrow */
static lv_obj_t *scroll = NULL;
static lv_obj_t *label = NULL;
static lv_timer_t *ui_timer = NULL;
static app_frame_t frame;

static pthread_mutex_t text_mutex = PTHREAD_MUTEX_INITIALIZER;
static char pending[NAVTEX_PENDING_MAX];
static size_t pending_len = 0;
static char text_buf[NAVTEX_TEXT_MAX];
static size_t text_len = 0;
static bool follow_tail = true;

static pthread_t test_thread;
static bool test_thread_valid = false;
static volatile bool test_stop_requested = false;
static volatile bool test_finished = false;
static volatile bool test_mode = false;

static button_data_t btn_page = {
    .type = BTN_TEXT_FN,
    .label_fn = page1_label,
    .press = page_cb
};

static button_data_t btn_page_2 = {
    .type = BTN_TEXT_FN,
    .label_fn = page2_label,
    .press = page_cb
};

static button_data_t btn_mode = {
    .type = BTN_TEXT_FN,
    .label_fn = mode_label,
    .press = mode_cb
};

static button_data_t btn_close = {
    .type = BTN_TEXT,
    .label = "Close",
    .press = close_cb
};

static button_data_t btn_clear = {
    .type = BTN_TEXT,
    .label = "Clear",
    .press = clear_cb
};

static button_data_t btn_save = {
    .type = BTN_TEXT,
    .label = "Save",
    .press = save_cb
};

static button_data_t btn_freq = {
    .type = BTN_TEXT,
    .label = "Freq.",
    .press = freq_cb
};

static button_data_t btn_test = {
    .type = BTN_TEXT,
    .label = "Test",
    .press = test_cb
};

static buttons_page_t btn_page_1 = {
    .items = { &btn_page, &btn_freq, &btn_mode, &btn_clear, &btn_close }
};

static button_data_t btn_mark = {
    .type = BTN_TEXT_FN,
    .label_fn = mark_label,
    .press = mark_cb
};

static button_data_t btn_scan = {
    .type = BTN_TEXT_FN,
    .label_fn = scan_label,
    .press = scan_cb
};

static buttons_page_t btn_page_2_items = {
    .items = { &btn_page_2, &btn_save, &btn_mark, &btn_scan, NULL }
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

dialog_t *dialog_navtex = &dialog;

/* Official NAVTEX frequencies, plus the HF MSI ones (same SITOR-B mode), then the
 * MF/HF DSC distress and safety channels (DSC mode, NAVTEX_N on).
 * The decoder is centred on 1000 Hz: B (0) is the higher tone, Y (1) the lower. */
#define NAVTEX_N 11
#define DSC_N    7
static const freq_list_item_t navtex_freqs[NAVTEX_N + DSC_N] = {
    { .group = "NAVTEX" },
    { .freq_hz = 518000,   .name = "NAVTEX",    .note = "International, English" },
    { .freq_hz = 490000,   .name = "NAVTEX",    .note = "National, local language" },
    { .freq_hz = 4209500,  .name = "NAVTEX HF", .note = "Tropical areas" },
    { .group = "HF MSI (NBDP)" },
    { .freq_hz = 8416500,  .name = "HF MSI" },
    { .freq_hz = 12579000, .name = "HF MSI" },
    { .freq_hz = 16806500, .name = "HF MSI" },
    { .freq_hz = 19680500, .name = "HF MSI" },
    { .freq_hz = 22376000, .name = "HF MSI" },
    { .freq_hz = 26100500, .name = "HF MSI" },
    { .group = "DSC distress / safety" },
    { .freq_hz = 2187500,  .name = "DSC", .note = "MF distress / safety" },
    { .freq_hz = 4207500,  .name = "DSC", .note = "HF distress / safety" },
    { .freq_hz = 6312000,  .name = "DSC", .note = "HF distress / safety" },
    { .freq_hz = 8414500,  .name = "DSC", .note = "HF distress / safety" },
    { .freq_hz = 12577000, .name = "DSC", .note = "HF distress / safety" },
    { .freq_hz = 16804500, .name = "DSC", .note = "HF distress / safety" },
};

#define DSC_DEFAULT (NAVTEX_N + 4)   /* 8414.5 kHz */
#define ALL_N       (NAVTEX_N + DSC_N)

/* CX8ANM: frecuencias marcadas para el escaneo (NAVTEX y DSC juntas), en /mnt/navtex/scan.conf */
#define SCAN_CONF       NAVTEX_SAVE_DIR "/scan.conf"
#define SCAN_SETTLE_MS  700     /* después de sintonizar: AGC y filtros */
#define SCAN_DWELL_NAV  6000    /* NAVTEX: SITOR-B necesita unos segundos para sincronizar */
#define SCAN_DWELL_DSC  2000    /* DSC: el patrón de puntos dura 2 s en HF */
#define SCAN_HOLD_NAV   30000   /* se queda mientras llegan caracteres, y 30 s más */
#define SCAN_HOLD_DSC   15000
#define SCAN_NAV_CHARS  6       /* caracteres en el período para decir "hay NAVTEX" */

static bool                scan_mark[ALL_N];
static freq_list_item_t    disp_freqs[ALL_N];
static char                disp_note[ALL_N][48];
static volatile unsigned   act_nav, act_dsc;    /* hilo de audio: caracteres NAVTEX, señales DSC */
static int                 dot_run, dot_last = -1;
static enum { SCAN_OFF, SCAN_DWELL, SCAN_HOLD } scan_state;
static int                 scan_idx = -1;
static uint32_t            scan_t0, scan_last_act;
static unsigned            scan_nav0, scan_dsc0;

static void disp_build(void)
{
    for (int i = 0; i < ALL_N; i++) {
        disp_freqs[i] = navtex_freqs[i];
        if (navtex_freqs[i].group || !scan_mark[i])
            continue;
        snprintf(disp_note[i], sizeof(disp_note[i]), "%s%s[ESCANEO]", navtex_freqs[i].note ? navtex_freqs[i].note : "",
                 navtex_freqs[i].note ? "  " : "");
        disp_freqs[i].note = disp_note[i];
    }
}

static void scan_load(void)
{
    FILE *f = fopen(SCAN_CONF, "r");
    long  hz;

    memset(scan_mark, 0, sizeof(scan_mark));
    if (f) {
        while (fscanf(f, "%ld", &hz) == 1)
            for (int i = 0; i < ALL_N; i++)
                if (!navtex_freqs[i].group && navtex_freqs[i].freq_hz == hz)
                    scan_mark[i] = true;
        fclose(f);
    }
    disp_build();
}

static void scan_save(void)
{
    FILE *f;

    mkdir(NAVTEX_SAVE_DIR, 0755);
    f = fopen(SCAN_CONF, "w");
    if (!f)
        return;
    for (int i = 0; i < ALL_N; i++)
        if (scan_mark[i])
            fprintf(f, "%ld\n", (long)navtex_freqs[i].freq_hz);
    fclose(f);
}

/* The mode's part of navtex_freqs (with the scan marks in the notes) */
static const freq_list_item_t *mode_freqs(int *n)
{
    *n = dsc_mode ? DSC_N : NAVTEX_N;
    return dsc_mode ? disp_freqs + NAVTEX_N : disp_freqs;
}

/* The info bar keeps the station's short name ("DSC", "HF MSI"): with the note it ran
 * under the frequency */
static void short_label(void)
{
    int n;
    int32_t dial = 0, vfo = cparam_i_get(cfg_fg_freq);
    const freq_list_item_t *list = mode_freqs(&n);
    const freq_list_item_t *it = freq_list_base(list, n, 1000, vfo, &dial);

    if (it && dial == vfo)
        freq_strip_set_label(it->name, dial);
}

static void tune(const freq_list_item_t *it)
{
    freq_list_tune(it, 1000);
    short_label();
}

bool dialog_navtex_is_active(void)
{
    return dialog.run;
}

static void append_text(const char *src, size_t n)
{
    if (n == 0)
        return;

    /*
     * Keep text_buf as a normal NUL-terminated string for LVGL.
     * When it fills up, discard complete oldest lines until there is
     * enough room for the incoming text.
     */
    if (n >= NAVTEX_TEXT_MAX) {
        src += n - (NAVTEX_TEXT_MAX - 1);
        n = NAVTEX_TEXT_MAX - 1;
        text_len = 0;
    }

    while (text_len + n >= NAVTEX_TEXT_MAX && text_len > 0) {
        char *newline = memchr(text_buf, '\n', text_len);
        size_t drop;

        if (newline)
            drop = (size_t)(newline - text_buf) + 1;
        else
            drop = text_len;

        memmove(text_buf, text_buf + drop, text_len - drop);
        text_len -= drop;
        text_buf[text_len] = '\0';
    }

    memcpy(text_buf + text_len, src, n);
    text_len += n;
    text_buf[text_len] = '\0';
}

static void char_cb(int c, void *userdata)
{
    (void)userdata;

    if (c == '\r' || dsc_mode)
        return;
    if (c > ' ' && c < 127)
        act_nav++;

    pthread_mutex_lock(&text_mutex);

    if (pending_len < NAVTEX_PENDING_MAX - 1)
        pending[pending_len++] = (char)c;

    pthread_mutex_unlock(&text_mutex);
}

/* Audio thread: a DSC call to the UI */
static void dsc_call_cb(const dsc_call_t *call, void *userdata)
{
    (void)userdata;

    act_dsc++;
    pthread_mutex_lock(&text_mutex);
    if (dsc_npending < DSC_PENDING_MAX)
        dsc_pending[dsc_npending++] = *call;
    pthread_mutex_unlock(&text_mutex);
}

/* Audio thread: the demodulated bits, in DSC mode; the higher tone is B (0) */
static void bit_cb(int mark, void *userdata)
{
    (void)userdata;

    if (!dsc_mode)
        return;
    /* El patrón de puntos de DSC (bits alternados) o una llamada en curso: actividad */
    if (mark != dot_last) {
        if (++dot_run >= 40) {
            act_dsc++;
            dot_run = 0;
        }
    } else {
        dot_run = 0;
    }
    dot_last = mark;
    if (dsc_rx.locked)
        act_dsc++;
    if (dsc_reset) {
        dsc_rx_init(&dsc_rx, dsc_call_cb, NULL);
        dsc_reset = false;
    }
    dsc_rx_bit(&dsc_rx, !mark);
}


static uint16_t read_le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool reset_decoder(void)
{
    if (decoder) {
        navtex_decoder_destroy(decoder);
        decoder = NULL;
    }

    decoder = navtex_decoder_create(0);
    if (!decoder) {
        fprintf(stderr, "NAVTEX: unable to create decoder\n");
        return false;
    }

    navtex_decoder_set_char_callback(decoder, char_cb, NULL);
    navtex_decoder_set_bit_callback(decoder, bit_cb, NULL);
    dsc_reset = true;
    return true;
}

/* Locate the PCM data in a RIFF/WAVE file.  Do not assume a 44-byte
 * header: afconvert may place extra chunks before the audio data.
 */
static bool open_test_wav(FILE **out, uint32_t *data_bytes)
{
    unsigned char hdr[12];
    unsigned char chunk[8];
    bool fmt_ok = false;
    FILE *f = fopen(NAVTEX_TEST_WAV, "rb");

    if (!f) {
        fprintf(stderr, "NAVTEX TEST: cannot open %s\n", NAVTEX_TEST_WAV);
        return false;
    }

    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
        memcmp(hdr, "RIFF", 4) != 0 ||
        memcmp(hdr + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "NAVTEX TEST: invalid WAV file\n");
        fclose(f);
        return false;
    }

    while (fread(chunk, 1, sizeof(chunk), f) == sizeof(chunk)) {
        uint32_t size = read_le32(chunk + 4);

        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char fmt[16];

            if (size < sizeof(fmt) ||
                fread(fmt, 1, sizeof(fmt), f) != sizeof(fmt)) {
                fprintf(stderr, "NAVTEX TEST: invalid fmt chunk\n");
                fclose(f);
                return false;
            }

            uint16_t format = read_le16(fmt);
            uint16_t channels = read_le16(fmt + 2);
            uint32_t rate = read_le32(fmt + 4);
            uint16_t bits = read_le16(fmt + 14);

            fmt_ok = (format == 1 && channels == 1 &&
                      rate == 11025 && bits == 16);

            if (size > sizeof(fmt))
                fseek(f, (long)(size - sizeof(fmt)), SEEK_CUR);
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!fmt_ok) {
                fprintf(stderr,
                        "NAVTEX TEST: WAV must be mono 11025 Hz PCM Int16\n");
                fclose(f);
                return false;
            }

            *out = f;
            *data_bytes = size;
            return true;
        } else {
            fseek(f, (long)size, SEEK_CUR);
        }

        if (size & 1)
            fseek(f, 1, SEEK_CUR);
    }

    fprintf(stderr, "NAVTEX TEST: data chunk not found\n");
    fclose(f);
    return false;
}

static void *test_thread_cb(void *arg)
{
    (void)arg;

    FILE *f = NULL;
    uint32_t bytes_left = 0;
    int16_t pcm[NAVTEX_TEST_CHUNK];
    float samples[NAVTEX_TEST_CHUNK];

    if (!open_test_wav(&f, &bytes_left)) {
        test_finished = true;
        return NULL;
    }

    fprintf(stderr, "NAVTEX TEST: playing %s\n", NAVTEX_TEST_WAV);

    while (!test_stop_requested && bytes_left >= 2) {
        size_t want = NAVTEX_TEST_CHUNK;
        size_t available = bytes_left / 2;

        if (want > available)
            want = available;

        size_t n = fread(pcm, sizeof(int16_t), want, f);
        if (n == 0)
            break;

        bytes_left -= (uint32_t)(n * sizeof(int16_t));

        for (size_t i = 0; i < n; ++i)
            samples[i] = (float)pcm[i] / 32768.0f;

        if (decoder)
            navtex_decoder_process(decoder, samples, (unsigned int)n);

        /* Feed the decoder at the same speed as live 11025 Hz audio. */
        usleep((useconds_t)((1000000ULL * n) / 11025ULL));
    }

    fclose(f);

    if (test_stop_requested)
        fprintf(stderr, "NAVTEX TEST: stopped\n");
    else
        fprintf(stderr, "NAVTEX TEST: finished\n");

    test_finished = true;
    return NULL;
}

static bool start_test(void)
{
    if (test_mode)
        return false;

    /* From this point audio_cb stops feeding live RX audio. */
    test_mode = true;
    test_stop_requested = false;
    test_finished = false;

    if (!reset_decoder()) {
        test_mode = false;
        return false;
    }

    int rc = pthread_create(&test_thread, NULL, test_thread_cb, NULL);
    if (rc != 0) {
        fprintf(stderr, "NAVTEX TEST: pthread_create failed: %d\n", rc);
        test_mode = false;
        reset_decoder();
        return false;
    }

    test_thread_valid = true;
    return true;
}

static void stop_test(void)
{
    if (!test_mode && !test_thread_valid)
        return;

    test_stop_requested = true;

    if (test_thread_valid) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    test_finished = false;
    test_mode = false;
    reset_decoder();
}


/* Station and frequency on the left, receiver state on the right */
static void header_update(void)
{
    char         l[80], r[32];
    int32_t      f = cparam_i_get(cfg_fg_freq) + 1000;     /* USB, decoder centred on 1000 Hz */
    const char  *name = dsc_mode ? "DSC" : "NAVTEX", *note = NULL;

    for (size_t i = 0; i < sizeof(navtex_freqs) / sizeof(navtex_freqs[0]); i++) {
        if (!navtex_freqs[i].group && navtex_freqs[i].freq_hz == f && (i >= NAVTEX_N) == dsc_mode) {
            name = navtex_freqs[i].name;
            note = navtex_freqs[i].note;
        }
    }
    if (dsc_mode && note)
        snprintf(l, sizeof(l), "%s   %.1f kHz   %s", name, f / 1000.0, note);
    else
        snprintf(l, sizeof(l), "%s   %.1f kHz", name, f / 1000.0);
    if (test_mode) snprintf(r, sizeof(r), "#%06x Test WAV#", theme_hex(TC_warn));
    else snprintf(r, sizeof(r), "RX");
    if (strcmp(l, lv_label_get_text(frame.head_left)))
        lv_label_set_text(frame.head_left, l);
    if (strcmp(r, lv_label_get_text(frame.head_right)))
        lv_label_set_text(frame.head_right, r);
}

/* DSC rows: the format's name for the remote */
static const char *fmt_name(int fmt)
{
    switch (fmt) {
        case 102: return "area";
        case 112: return "distress";
        case 114: return "group";
        case 116: return "all ships";
        case 120: return "individual";
        default: return "automatic";
    }
}

/* What the call is about, after the country; arrow before an addressed station */
static void dsc_detail(const dsc_call_t *c, const char *arrow, char *out, size_t size)
{
    const char *cat = dsc_cat_name(c), *nature = dsc_nature_name(c->nature);
    const char *tc = dsc_tc_name(c->tc1);
    char        target[48];

    if (c->fmt == 112) {
        snprintf(out, size, "%s%s", c->ecc_ok ? "" : "? ", nature[0] ? nature : tr("DISTRESS ALERT"));
        return;
    }
    if (!strcmp(cat, "ack")) {
        snprintf(out, size, tr("%s%s %s  distress ack"), c->ecc_ok ? "" : "? ", arrow, c->dmmsi);
        return;
    }
    if (c->fmt == 116)
        snprintf(target, sizeof(target), "ALL SHIPS");
    else if (c->fmt == 102)
        snprintf(target, sizeof(target), "AREA %s", c->to);
    else if (c->fmt == 114)
        snprintf(target, sizeof(target), tr("%s group %s"), arrow, c->to);
    else
        snprintf(target, sizeof(target), "%s %s", arrow, c->to);
    if (c->dmmsi[0])   /* distress relay */
        snprintf(out, size, "%s%s  RELAY %s %s", c->ecc_ok ? "" : "? ", target, c->dmmsi, nature);
    else
        snprintf(out, size, "%s%s  %s%s%s", c->ecc_ok ? "" : "? ", target, tc, tc[0] && c->freq[0] ? " " : "",
                 c->freq);
}

/* Distress alerts and relays: position and time on a second line */
static void dsc_where(const dsc_call_t *c, char *out, size_t size)
{
    out[0] = 0;
    if (!c->dmmsi[0] || !strcmp(dsc_cat_name(c), "ack"))
        return;
    snprintf(out, size, "%s%s%s%s", c->pos[0] ? c->pos : tr("position unknown"), c->utc[0] ? tr("  at ") : "", c->utc,
             c->utc[0] ? " UTC" : "");
}

/* "Italy", "Coast, Spain", "" */
static const char *dsc_from_country(const char *mmsi, char *buf, size_t size, int *flag)
{
    const char *prefix = NULL, *name = dsc_country(mmsi, &prefix);

    *flag = -1;
    if (!name)
        return "";
    for (size_t i = 0; i < sizeof(flag_of) / sizeof(flag_of[0]); i++)
        if (!strcmp(flag_of[i].prefix, prefix)) {
            *flag = flag_of[i].flag;
            break;
        }
    if (mmsi[0] == '0' && mmsi[1] == '0') {
        snprintf(buf, size, tr("Coast, %s"), name);
        return buf;
    }
    return name;
}

static void row_delete_cb(lv_event_t *e)
{
    free(lv_obj_get_user_data(lv_event_get_target(e)));
}

static lv_obj_t *row_label(lv_obj_t *row, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *text, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(row);

    lv_obj_set_style_text_font(l, &dsc_font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, w);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, text);
    return l;
}

/* A row at the top of the list: time, category, MMSI, flag and country, detail */
/* Todo lo recibido queda en /mnt/navtex, un archivo por día (UTC): navtex-AAAAMMDD.txt
 * y dsc-AAAAMMDD.txt. Se descargan desde la web (Galería > NAVTEX, o Archivos). */
static void autosave_append(const char *prefix, const char *text, size_t n)
{
    static int day_navtex = -1;
    char       name[96];
    time_t     now = time(NULL);
    struct tm  tm;

    if (!n)
        return;
    gmtime_r(&now, &tm);
    mkdir(NAVTEX_SAVE_DIR, 0755);
    snprintf(name, sizeof(name), NAVTEX_SAVE_DIR "/%s-%04d%02d%02d.txt", prefix, tm.tm_year + 1900, tm.tm_mon + 1,
             tm.tm_mday);
    FILE *f = fopen(name, "a");
    if (!f)
        return;
    /* NAVTEX: una marca de hora al empezar cada día */
    if (prefix[0] == 'n' && day_navtex != tm.tm_yday) {
        fprintf(f, "\n--- %04d-%02d-%02d %02d:%02d UTC ---\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min);
        day_navtex = tm.tm_yday;
    }
    fwrite(text, 1, n, f);
    fclose(f);
}

static void dsc_add_row(const dsc_call_t *c, const char *hms, int32_t freq)
{
    static dsc_call_t last;
    static time_t     last_t;
    time_t            now = time(NULL);
    char              detail[96], where[64], cty_buf[48], line[256];
    const char       *cat = dsc_cat_name(c), *pill;
    int               flag;
    bool              distress = !strcmp(cat, "distress");
    lv_color_t        pill_bg, pill_fg = TC(text);

    if (!memcmp(&last, c, sizeof(last)) && now - last_t < DSC_REPEAT_S)
        return;
    last   = *c;
    last_t = now;

    const char *country = dsc_from_country(c->from, cty_buf, sizeof(cty_buf), &flag);
    dsc_detail(c, LV_SYMBOL_RIGHT, detail, sizeof(detail));
    dsc_where(c, where, sizeof(where));

    /* For X6100 Link, and the text Save writes */
    {
        char plain[96];
        dsc_detail(c, "->", plain, sizeof(plain));
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "t", "dsc_rx");
        cJSON_AddStringToObject(o, "time", hms);
        cJSON_AddStringToObject(o, "cat", cat);
        cJSON_AddStringToObject(o, "fmt", fmt_name(c->fmt));
        cJSON_AddStringToObject(o, "from", c->from);
        cJSON_AddStringToObject(o, "to", c->fmt == 116 || c->fmt == 112 ? "" : c->to);
        cJSON_AddStringToObject(o, "country", country);
        cJSON_AddStringToObject(o, "nature", dsc_nature_name(c->nature));
        cJSON_AddStringToObject(o, "pos", c->pos);
        cJSON_AddStringToObject(o, "utc", c->utc);
        cJSON_AddStringToObject(o, "distress", c->dmmsi);
        cJSON_AddStringToObject(o, "detail", plain);
        cJSON_AddBoolToObject(o, "ecc_ok", c->ecc_ok);
        cJSON_AddNumberToObject(o, "freq", freq / 1000.0);
        remote_event(o);
        snprintf(line, sizeof(line), "%s %-8s %s %s %s%s%s\n", hms, cat, c->from, country, plain, where[0] ? "  " : "",
                 where);
        autosave_append("dsc", line, strlen(line));
    }
    if (!dsc_list)
        return;

    if (distress) {
        pill = "DISTRESS"; pill_bg = TC(tx_bg);
    } else if (!strcmp(cat, "ack")) {
        pill = "ACK"; pill_bg = TC(toggle_on);
    } else if (!strcmp(cat, "urgency")) {
        pill = "URGENCY"; pill_bg = TC(warn); pill_fg = TC(panel);
    } else if (!strcmp(cat, "safety")) {
        pill = "SAFETY"; pill_bg = TC(accent); pill_fg = TC(panel);
    } else {
        pill = "ROUTINE"; pill_bg = TC(off);
    }

    lv_obj_t *row = lv_obj_create(dsc_list);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(row, lv_pct(100), where[0] ? 54 : 30);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, TC(row), 0);
    lv_obj_set_user_data(row, strdup(line));
    lv_obj_add_event_cb(row, row_delete_cb, LV_EVENT_DELETE, NULL);
    if (distress) {
        lv_obj_set_style_bg_color(row, TC(err_bg), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_t *edge = lv_obj_create(row);
        lv_obj_remove_style_all(edge);
        lv_obj_set_size(edge, 3, lv_pct(100));
        lv_obj_set_style_bg_color(edge, TC(err_strong), 0);
        lv_obj_set_style_bg_opa(edge, LV_OPA_COVER, 0);
    }

    row_label(row, 8, 4, 76, hms, c->ecc_ok ? TC(text_dim) : TC(warn));

    lv_obj_t *p = row_label(row, 90, 4, 88, pill, pill_fg);
    lv_obj_set_style_text_font(p, &sony_16, 0);
    lv_obj_set_style_text_align(p, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(p, pill_bg, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p, 4, 0);
    lv_obj_set_style_pad_ver(p, 1, 0);

    row_label(row, 190, 4, 104, c->from, TC(text));
    const lv_img_dsc_t *img = table_view_flag(flag);
    if (img) {
        lv_obj_t *i = lv_img_create(row);
        lv_img_set_src(i, img);
        lv_obj_set_pos(i, 300, 9);
    }
    row_label(row, 322, 4, 150, country, TC(text_dim));
    row_label(row, 480, 4, 270, detail, TC(text));
    if (where[0])
        row_label(row, 190, 28, 560, where, TC(text));

    lv_obj_move_to_index(row, 0);   /* the newest on top */
    if (lv_obj_get_child_cnt(dsc_list) > DSC_ROWS_MAX)
        lv_obj_del(lv_obj_get_child(dsc_list, -1));
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (scroll)
        header_update();
    scan_tick();

    /* The worker never touches LVGL or destroys the decoder.  When a test
     * reaches EOF, finish the thread here in the UI context and return to
     * a fresh decoder for normal live reception.
     */
    if (test_mode && test_finished) {
        if (test_thread_valid) {
            pthread_join(test_thread, NULL);
            test_thread_valid = false;
        }
        test_finished = false;
        test_mode = false;
        reset_decoder();
    }
    char local[NAVTEX_PENDING_MAX];
    size_t n = 0;
    dsc_call_t calls[DSC_PENDING_MAX];
    int ncalls;

    pthread_mutex_lock(&text_mutex);
    n = pending_len;
    if (n > 0) {
        memcpy(local, pending, n);
        pending_len = 0;
    }
    ncalls = dsc_npending;
    memcpy(calls, dsc_pending, ncalls * sizeof(calls[0]));
    dsc_npending = 0;
    pthread_mutex_unlock(&text_mutex);

    if (ncalls) {
        time_t now = time(NULL);
        char   hms[16];
        strftime(hms, sizeof(hms), "%H:%M:%S", gmtime(&now));
        for (int i = 0; i < ncalls; i++)
            dsc_add_row(&calls[i], hms, cparam_i_get(cfg_fg_freq) + 1000);
    }

    if (n > 0) {
        remote_text("navtex_text", local, n);
        autosave_append("navtex", local, n);
    }
    if (n == 0 || !label)
        return;

    append_text(local, n);
    lv_label_set_text(label, text_buf);

    if (scroll && follow_tail)
        lv_obj_scroll_to_y(scroll, LV_COORD_MAX, LV_ANIM_OFF);
}

/* The mode's content, status line and buttons */
static void mode_show(void)
{
    if (!scroll)
        return;
    if (dsc_mode) {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(dsc_list, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(frame.status,
                          "DSC 100 Bd / 170 Hz   MFK: scroll   Freq.: channels");
    } else {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(dsc_list, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(frame.status, "SITOR-B 100 Bd / 170 Hz   MFK: scroll   Freq.: stations");
    }
    lv_obj_scroll_to_y(scroll, dsc_mode ? 0 : LV_COORD_MAX, LV_ANIM_OFF);
    if (btn_mode.disp_btn) buttons_refresh(&btn_mode);
    if (btn_page.disp_btn) buttons_refresh(&btn_page);
    if (btn_page_2.disp_btn) buttons_refresh(&btn_page_2);
    header_update();
}

static void construct_cb(lv_obj_t *parent)
{
    pthread_mutex_lock(&text_mutex);
    pending_len = 0;
    dsc_npending = 0;
    pthread_mutex_unlock(&text_mutex);
    text_len = 0;
    text_buf[0] = '\0';
    follow_tail = true;

    test_mode = false;
    test_thread_valid = false;
    test_stop_requested = false;
    test_finished = false;

    reset_decoder();
    scan_state = SCAN_OFF;
    scan_load();

    /* Full screen in the common app look; the main waterfall is hidden */
    dialog.obj = dialog_init(parent);
    dsp_set_waterfall_enabled(false);
    dsp_set_spectrum_enabled(false);
    app_frame_create(&frame, dialog.obj);

    scroll = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(scroll);
    app_frame_body(scroll);
    lv_obj_set_style_pad_all(scroll, 8, 0);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);

    label = lv_label_create(scroll);
    /* panel_style has just been attached and LVGL may not have run a
     * layout pass yet.  lv_obj_get_content_width(scroll) can therefore
     * still be 0 here, making the label effectively invisible.
     * 100% means the panel's content width (same padding as RTTY).
     */
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, &sony_22, 0);
    lv_obj_set_style_text_color(label, TC(text), 0);
    lv_label_set_text(label, "NAVTEX ready - 100 baud / 170 Hz shift\n");

    /* DSC calls, the newest on top */
    dsc_font = sony_18;
    dsc_font.fallback = &lv_font_montserrat_18;
    dsc_list = lv_obj_create(scroll);
    lv_obj_remove_style_all(dsc_list);
    lv_obj_set_size(dsc_list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dsc_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(dsc_list, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* MFK target.  LEFT scrolls towards older text, RIGHT towards
     * the newest text.  The volume encoder is handled in key_cb too.
     */
    lv_group_add_obj(keyboard_group, scroll);
    lv_obj_add_event_cb(scroll, key_cb, LV_EVENT_KEY, NULL);
    lv_group_focus_obj(scroll);
    lv_group_set_editing(keyboard_group, true);

    ui_timer = lv_timer_create(ui_timer_cb, NAVTEX_UI_TIMER_MS, NULL);
    mode_show();
}

static void destruct_cb(void)
{
    freq_list_close();
    scan_state = SCAN_OFF;

    /* Stop the WAV worker before destroying decoder/UI objects. */
    if (test_mode || test_thread_valid)
        stop_test();
    if (ui_timer) {
        lv_timer_del(ui_timer);
        ui_timer = NULL;
    }

    if (decoder) {
        navtex_decoder_destroy(decoder);
        decoder = NULL;
    }

    pthread_mutex_lock(&text_mutex);
    pending_len = 0;
    pthread_mutex_unlock(&text_mutex);

    label = NULL;
    scroll = NULL;
    dsc_list = NULL;
    dsp_set_waterfall_enabled(true);
    dsp_set_spectrum_enabled(true);
}

static void audio_cb(unsigned int n, float *samples)
{
    /* Never mix live receiver audio with the WAV test stream. */
    if (test_mode)
        return;

    if (decoder)
        navtex_decoder_process(decoder, samples, n);
}

static void key_cb(lv_event_t *e)
{
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    /* While a dialog is open the main-screen handler is disabled,
     * therefore NAVTEX must handle the volume encoder itself.
     */
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

    if (!scroll)
        return;

    switch (key) {
        case LV_KEY_LEFT: {
            /* MFK left: browse older text and stop following the tail. */
            lv_coord_t y = lv_obj_get_scroll_y(scroll);
            follow_tail = false;
            lv_obj_scroll_to_y(scroll, y - NAVTEX_SCROLL_STEP, LV_ANIM_OFF);
            break;
        }

        case LV_KEY_RIGHT: {
            /* MFK right: browse newer text.  Once another step cannot
             * move farther, we are at the tail and automatic following
             * is enabled again.
             */
            lv_coord_t old_y = lv_obj_get_scroll_y(scroll);
            lv_obj_scroll_to_y(scroll, old_y + NAVTEX_SCROLL_STEP, LV_ANIM_OFF);
            lv_coord_t new_y = lv_obj_get_scroll_y(scroll);

            if (new_y == old_y) {
                follow_tail = true;
                lv_obj_scroll_to_y(scroll, LV_COORD_MAX, LV_ANIM_OFF);
            }
            break;
        }

        default:
            break;
    }
}

static void close_cb(button_data_t *data)
{
    (void)data;
    dialog_destruct();
}

static void clear_cb(button_data_t *data)
{
    (void)data;

    if (dsc_mode) {
        if (dsc_list)
            lv_obj_clean(dsc_list);
        return;
    }

    pthread_mutex_lock(&text_mutex);
    pending_len = 0;
    pthread_mutex_unlock(&text_mutex);

    text_len = 0;
    text_buf[0] = '\0';
    follow_tail = true;

    if (label)
        lv_label_set_text(label, "");

    if (scroll)
        lv_obj_scroll_to_y(scroll, 0, LV_ANIM_OFF);
}

/* The text on screen to a file, then cleared; DSC: the calls, oldest first */
static void save_cb(button_data_t *data)
{
    char   name[64];
    time_t now = time(NULL);
    FILE  *f;
    bool   ok = true;
    int    rows = dsc_list ? (int)lv_obj_get_child_cnt(dsc_list) : 0;

    if (dsc_mode ? !rows : !text_len) {
        msg_update_text_fmt("Nothing to save");
        return;
    }
    mkdir(NAVTEX_SAVE_DIR, 0755);
    strftime(name, sizeof(name), dsc_mode ? NAVTEX_SAVE_DIR "/dsc-%Y%m%d-%H%M%S.txt"
                                          : NAVTEX_SAVE_DIR "/navtex-%Y%m%d-%H%M%S.txt", gmtime(&now));
    f = fopen(name, "w");
    if (f && dsc_mode) {
        for (int i = rows - 1; i >= 0 && ok; i--)
            ok = fputs(lv_obj_get_user_data(lv_obj_get_child(dsc_list, i)), f) >= 0;
    } else if (f) {
        ok = fwrite(text_buf, 1, text_len, f) == text_len;
    }
    if (!f || !ok) {
        if (f) fclose(f);
        msg_update_text_fmt("NAVTEX: save failed");
        return;
    }
    fclose(f);
    clear_cb(data);
    msg_update_text_fmt("Saved %s", name + sizeof(NAVTEX_SAVE_DIR));
}

static void test_cb(button_data_t *data)
{
    (void)data;

    if (test_mode) {
        stop_test();
        return;
    }

    if (!start_test())
        fprintf(stderr, "NAVTEX TEST: unable to start\n");
}

/* The mode's list of stations / channels */
static void freq_cb(button_data_t *data)
{
    (void)data;
    int n;
    const freq_list_item_t *list = mode_freqs(&n);

    if (scan_state != SCAN_OFF)
        scan_stop("Scan stopped: manual tuning");
    if (freq_list_is_open())
        freq_list_close();
    else if (scroll)
        freq_list_open(lv_obj_get_parent(scroll), dsc_mode ? "DSC - channels" : "NAVTEX - frequencies", list, n,
                       1000, short_label);
}

/* NAVTEX <-> DSC; the tuning follows when it was on the other mode's channels */
static void mode_set(bool dsc)
{
    int n;
    const freq_list_item_t *list;
    int32_t dial = 0;

    if (dsc == dsc_mode)
        return;
    freq_list_close();
    list = mode_freqs(&n);
    freq_list_base(list, n, 1000, cparam_i_get(cfg_fg_freq), &dial);
    bool on_channel = dial == cparam_i_get(cfg_fg_freq);

    dsc_mode = dsc;
    dsc_reset = true;
    if (on_channel)
        tune(dsc ? &navtex_freqs[DSC_DEFAULT] : &navtex_freqs[1]);
    mode_show();
}

static void mode_cb(button_data_t *data)
{
    (void)data;
    if (scan_state != SCAN_OFF)
        scan_stop("Scan stopped");
    mode_set(!dsc_mode);
}

static void page_cb(button_data_t *data)
{
    buttons_load_page(data == &btn_page ? &btn_page_2_items : &btn_page_1);
}

static const char *mode_label(void)
{
    return dsc_mode ? "Mode:\nDSC" : "Mode:\nNAVTEX";
}

static const char *page1_label(void)
{
    return dsc_mode ? "(DSC 1:2)" : "(NAVTEX 1:2)";
}

static const char *page2_label(void)
{
    return dsc_mode ? "(DSC 2:2)" : "(NAVTEX 2:2)";
}

/* Web remote (X6100 Link) */
void dialog_navtex_remote(const char *cmd, int32_t val, const char *text)
{
    (void)text;
    if (!strcmp(cmd, "app_base")) {
        int n;
        const freq_list_item_t *list = mode_freqs(&n);
        const freq_list_item_t *it = freq_list_base(list, n, 1000, cparam_i_get(cfg_fg_freq), NULL);
        if (it)
            tune(it);
    } else if (!strcmp(cmd, "navtex_mode")) {
        mode_set(val == 1);
    }
}

int dialog_navtex_remote_state(char *buf, size_t size)
{
    int32_t dial = 0;
    int n;
    const freq_list_item_t *list = mode_freqs(&n);

    freq_list_base(list, n, 1000, cparam_i_get(cfg_fg_freq), &dial);
    return snprintf(buf, size, "{\"base\":%i,\"mode\":%i}", (int)dial, dsc_mode ? 1 : 0);
}

/* ---- CX8ANM: escaneo de las frecuencias marcadas ---- */

/* The list entry the VFO is on (any mode), -1 none */
static int current_entry(void)
{
    int32_t f = cparam_i_get(cfg_fg_freq) + 1000;

    for (int i = 0; i < ALL_N; i++)
        if (!navtex_freqs[i].group && navtex_freqs[i].freq_hz == f && (i >= NAVTEX_N) == dsc_mode)
            return i;
    for (int i = 0; i < ALL_N; i++)
        if (!navtex_freqs[i].group && navtex_freqs[i].freq_hz == f)
            return i;
    return -1;
}

static int marked_count(void)
{
    int n = 0;

    for (int i = 0; i < ALL_N; i++)
        n += scan_mark[i];
    return n;
}

static const char *mark_label(void)
{
    int i = current_entry();

    return i >= 0 && scan_mark[i] ? "Unmark\nfreq." : "Mark\nfreq.";
}

static const char *scan_label(void)
{
    return scan_state == SCAN_OFF ? "Scan:\nOFF" : "Scan:\nON";
}

static void scan_buttons(void)
{
    if (btn_mark.disp_btn) buttons_refresh(&btn_mark);
    if (btn_scan.disp_btn) buttons_refresh(&btn_scan);
}

static void mark_cb(button_data_t *data)
{
    (void)data;
    int i = current_entry();

    if (i < 0) {
        msg_update_text_fmt("Tune a station from the list first (Freq.)");
        return;
    }
    scan_mark[i] = !scan_mark[i];
    scan_save();
    disp_build();
    msg_update_text_fmt(scan_mark[i] ? "%.1f kHz marked for scanning (%d)" : "%.1f kHz unmarked (%d)",
                        navtex_freqs[i].freq_hz / 1000.0, marked_count());
    scan_buttons();
}

/* Tunes to entry i without the list's message, and the mode follows (NAVTEX / DSC) */
static void scan_tune(int i)
{
    const freq_list_item_t *it = &navtex_freqs[i];
    int32_t                 dial = it->freq_hz - 1000;
    bool                    dsc = i >= NAVTEX_N;

    if (dsc != dsc_mode) {
        dsc_mode  = dsc;
        dsc_reset = true;
        mode_show();
    }
    main_screen_set_freq(dial);
    cparam_i_set(cfg_cur_mode, x6100_mode_usb);
    freq_strip_set_label(it->name, dial);
    dot_run      = 0;
    scan_idx     = i;
    scan_state   = SCAN_DWELL;
    scan_t0      = lv_tick_get();
    scan_nav0    = act_nav;
    scan_dsc0    = act_dsc;
}

static void scan_next(void)
{
    for (int k = 1; k <= ALL_N; k++) {
        int i = (scan_idx + k + ALL_N) % ALL_N;
        if (scan_mark[i]) {
            scan_tune(i);
            return;
        }
    }
    scan_stop("Scan stopped: no marked frequencies");
}

static void scan_status(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void scan_status(const char *fmt, ...)
{
    char    buf[120];
    va_list ap;

    if (!scroll)
        return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), tr(fmt), ap);
    va_end(ap);
    if (strcmp(buf, lv_label_get_text(frame.status)))
        lv_label_set_text(frame.status, buf);
}

static void scan_stop(const char *why)
{
    scan_state = SCAN_OFF;
    if (why)
        msg_update_text_fmt("%s", tr(why));
    mode_show();
    scan_buttons();
}

static void scan_cb(button_data_t *data)
{
    (void)data;

    if (scan_state != SCAN_OFF) {
        scan_stop("Scan stopped");
        return;
    }
    if (test_mode) {
        msg_update_text_fmt("Stop the test first");
        return;
    }
    if (marked_count() == 0) {
        msg_update_text_fmt("Mark frequencies first: Freq., choose one, Mark freq.");
        return;
    }
    freq_list_close();
    scan_idx = current_entry() >= 0 ? current_entry() - 1 : -1;
    scan_next();
    msg_update_text_fmt("Scanning %d frequencies", marked_count());
    scan_buttons();
}

/* UI timer: dwell on each marked frequency; with NAVTEX or DSC activity it stays and decodes */
static void scan_tick(void)
{
    if (scan_state == SCAN_OFF || scan_idx < 0)
        return;

    bool     dsc   = scan_idx >= NAVTEX_N;
    uint32_t el    = lv_tick_elaps(scan_t0);
    double   khz   = navtex_freqs[scan_idx].freq_hz / 1000.0;
    unsigned nav   = act_nav - scan_nav0, dscn = act_dsc - scan_dsc0;
    bool     activity;

    if (el < SCAN_SETTLE_MS) {      /* lo de antes de asentarse no cuenta */
        scan_nav0 = act_nav;
        scan_dsc0 = act_dsc;
        return;
    }
    activity = dsc ? dscn > 0 : nav >= SCAN_NAV_CHARS;

    if (scan_state == SCAN_DWELL) {
        if (activity) {
            scan_state    = SCAN_HOLD;
            scan_last_act = lv_tick_get();
            scan_nav0     = act_nav;
            scan_dsc0     = act_dsc;
            msg_update_text_fmt(dsc ? "DSC activity on %.1f kHz: listening" : "NAVTEX activity on %.1f kHz: listening",
                                khz);
            return;
        }
        if (el >= (uint32_t)(SCAN_SETTLE_MS + (dsc ? SCAN_DWELL_DSC : SCAN_DWELL_NAV))) {
            scan_next();
            return;
        }
        scan_status("SCAN  %.1f kHz  %s   (Scan: stop)", khz, dsc ? "DSC" : "NAVTEX");
        return;
    }

    /* HOLD: cualquier actividad nueva renueva la espera */
    if (act_nav != scan_nav0 || act_dsc != scan_dsc0) {
        scan_last_act = lv_tick_get();
        scan_nav0     = act_nav;
        scan_dsc0     = act_dsc;
    }
    uint32_t quiet = lv_tick_elaps(scan_last_act), hold = dsc ? SCAN_HOLD_DSC : SCAN_HOLD_NAV;
    if (quiet >= hold) {
        scan_next();
        return;
    }
    scan_status("RECEIVING  %.1f kHz  %s   - back to scan in %u s", khz, dsc ? "DSC" : "NAVTEX",
                (unsigned)((hold - quiet) / 1000));
}
