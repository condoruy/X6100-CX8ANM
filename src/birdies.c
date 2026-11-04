/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: birdies. Ver birdies.h.
 *
 *  Archivo /mnt/birdies.txt: una línea por birdie, "frecuencia en kHz [nota]"; '#' comenta.
 *  La página web lo edita; la radio lo vuelve a leer sola cuando cambia.
 */
#include "birdies.h"

#include "cfg/cfg_api.h"
#include "cfg/subject_api.h"
#include "i18n.h"
#include "msg.h"
#include "params/params.h"
#include "radio.h"
#include "scheduler.h"

#include "lvgl/lvgl.h"
#include <aether_radio/x6100_control/control.h>
#include <liquid/liquid.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_BIRDIES   512
#define NOTE_LEN      40
#define NOTCH_WIDTH   30          /* Hz: el birdie es una portadora fija, alcanza un notch angosto */
#define SAME_HZ       25          /* dos marcas más cerca que esto son el mismo birdie */

#define DECIM         8           /* 44100 / 8 = 5512,5 Hz: alcanza para un filtro de hasta 2,7 kHz */
#define FFT_N         8192        /* 1,5 s de audio, 0,67 Hz por punto */
#define FS            (44100.0f / DECIM)
#define MAX_AUDIO_HZ  2700

typedef struct {
    int32_t hz;
    char    note[NOTE_LEN];
} birdie_t;

static birdie_t list[MAX_BIRDIES];
static int      count;
static time_t   file_mtime;

static bool     scanning;     /* buscando birdies en toda la banda */
static bool     notch_on;
static int32_t  notch_hz;     /* el birdie (RF) que tiene el notch */

/* Captura para "Marcar" (hilo de audio -> hilo de la interfaz) */
static volatile bool capturing;
static float         cap_buf[FFT_N];
static int           cap_n;
static int           cap_len = FFT_N;   /* marcar: 1,5 s; buscar: 0,75 s por paso */
static float         cap_acc;
static int           cap_dec;

/* ---------- archivo ---------- */

static int cmp_hz(const void *a, const void *b) {
    int32_t x = ((const birdie_t *)a)->hz, y = ((const birdie_t *)b)->hz;
    return x < y ? -1 : x > y;
}

static void load(void) {
    FILE       *f = fopen(BIRDIES_FILE, "r");
    char        line[160];
    struct stat st;

    count      = 0;
    file_mtime = stat(BIRDIES_FILE, &st) == 0 ? st.st_mtime : 0;
    if (!f) return;
    while (count < MAX_BIRDIES && fgets(line, sizeof(line), f)) {
        char  *p = line, *end;
        double khz;

        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == 0) continue;
        for (char *c = p; *c && *c != ' ' && *c != '\t'; c++) {
            if (*c == ',') *c = '.';
        }
        khz = strtod(p, &end);
        if (end == p || khz < 1.0 || khz > 1000000.0) continue;
        list[count].hz = (int32_t)lround(khz * 1000.0);
        while (*end == ' ' || *end == '\t') end++;
        end[strcspn(end, "\r\n")] = 0;
        snprintf(list[count].note, NOTE_LEN, "%s", end);
        count++;
    }
    fclose(f);
    qsort(list, count, sizeof(list[0]), cmp_hz);
}

static void save(void) {
    FILE       *f = fopen(BIRDIES_FILE ".tmp", "w");
    struct stat st;

    if (!f) {
        msg_update_text_fmt("Birdies: could not save the list");
        return;
    }
    fprintf(f, "# Birdies (CX8ANM): frecuencia en kHz y una nota opcional, uno por línea\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%.3f%s%s\n", list[i].hz / 1000.0, list[i].note[0] ? " " : "", list[i].note);
    }
    fclose(f);
    rename(BIRDIES_FILE ".tmp", BIRDIES_FILE);
    sync();
    file_mtime = stat(BIRDIES_FILE, &st) == 0 ? st.st_mtime : 0;
}

/* ---------- dónde cae cada birdie en el audio ---------- */

typedef struct {
    int32_t lo;        /* frecuencia que da 0 Hz de audio */
    int     sign;      /* +1 banda lateral superior, -1 inferior, 0 AM/FM (los dos lados) */
    int32_t low, high; /* filtro, Hz de audio */
} rx_view_t;

static rx_view_t rx_view(void) {
    rx_view_t    v;
    x6100_mode_t mode = (x6100_mode_t)subject_i_get((SubjectInt *)cfg_cur_mode);

    v.lo   = subject_i_get((SubjectInt *)cfg_fg_freq) + param_i_get(cfg_rit) +
             subject_i_get((SubjectInt *)cfg_mode_lo_offset);
    v.low  = subject_i_get((SubjectInt *)cfg_cur_filter_low);
    v.high = subject_i_get((SubjectInt *)cfg_cur_filter_high);
    switch (mode) {
        case x6100_mode_lsb:
        case x6100_mode_lsb_dig:
        case x6100_mode_cwr:
            v.sign = -1;
            break;
        case x6100_mode_am:
        case x6100_mode_nfm:
            v.sign = 0;
            v.low  = 0;
            break;
        default:
            v.sign = 1;
            break;
    }
    if (v.low < 100) v.low = 100;     /* el DNF va de 100 a 3000 Hz */
    if (v.high > 3000) v.high = 3000;
    return v;
}

static int32_t audio_of(const rx_view_t *v, int32_t hz) {
    int32_t rel = hz - v->lo;

    return v->sign > 0 ? rel : v->sign < 0 ? -rel : abs(rel);
}

/* El birdie dentro del filtro más cerca del centro, o -1 */
static int birdie_in_passband(const rx_view_t *v, int32_t *audio) {
    int     best = -1;
    int32_t mid  = (v->low + v->high) / 2, best_d = 0;

    for (int i = 0; i < count; i++) {
        int32_t a = audio_of(v, list[i].hz);

        if (a < v->low || a > v->high) continue;
        if (best < 0 || abs(a - mid) < best_d) {
            best   = i;
            best_d = abs(a - mid);
            *audio = a;
        }
    }
    return best;
}

static void release(void) {
    if (notch_on) {
        notch_on = false;
        radio_set_dnf_override(false, 0, 0);
    }
}

void birdies_refresh(void) {
    rx_view_t v;
    int32_t   audio = 0;
    int       i;

    /* El DNF que puso el operador manda */
    if (!params.birdies.x || capturing || scanning || count == 0 || param_i_get(cfg_dnf)) {
        release();
        return;
    }
    v = rx_view();
    i = birdie_in_passband(&v, &audio);
    if (i < 0) {
        release();
        return;
    }
    notch_on = true;
    notch_hz = list[i].hz;
    radio_set_dnf_override(true, (uint16_t)audio, NOTCH_WIDTH);
}

static void scan_stop(const char *why);

static void on_change(Subject *subj, void *user_data) {
    if (scanning && (subj == (Subject *)cfg_fg_freq || subj == (Subject *)cfg_cur_mode)) {
        scan_stop("Birdie search stopped: manual tuning");
        return;
    }
    birdies_refresh();
}

/* La página web cambió el archivo */
static void file_timer_cb(lv_timer_t *t) {
    struct stat st;
    time_t      m = stat(BIRDIES_FILE, &st) == 0 ? st.st_mtime : 0;

    if (m != file_mtime) {
        load();
        birdies_refresh();
    }
}

/* ---------- marcar ---------- */

/* 1 agregado, 0 ya estaba (queda la medida nueva), -1 lista llena */
static int add_birdie(int32_t hz, const char *note) {
    int i;

    for (i = 0; i < count && abs(list[i].hz - hz) > SAME_HZ; i++) {}
    if (i < count) {
        list[i].hz = hz;
        return 0;
    }
    if (count >= MAX_BIRDIES) return -1;
    list[count].hz = hz;
    snprintf(list[count].note, NOTE_LEN, "%s", note ? note : "");
    count++;
    qsort(list, count, sizeof(list[0]), cmp_hz);
    return 1;
}

bool birdies_capturing(void) {
    return capturing;
}

static void mark_done(void *arg);
static void (*cap_done)(void *) = mark_done;

void birdies_put_audio(size_t nsamples, const int16_t *samples) {
    if (!capturing) return;
    for (size_t i = 0; i < nsamples && cap_n < cap_len; i++) {
        cap_acc += samples[i];
        if (++cap_dec == DECIM) {
            cap_buf[cap_n++] = cap_acc / DECIM;
            cap_acc = 0;
            cap_dec = 0;
        }
    }
    if (cap_n >= cap_len) {
        capturing = false;
        scheduler_put_noargs(cap_done);
    }
}

void birdies_mark(void) {
    x6100_mode_t mode = (x6100_mode_t)subject_i_get((SubjectInt *)cfg_cur_mode);

    if (capturing || scanning) return;
    if (mode == x6100_mode_am || mode == x6100_mode_nfm) {
        msg_update_text_fmt("Birdies: mark them in USB, LSB or CW");
        return;
    }
    cap_done = mark_done;
    cap_len  = FFT_N;
    cap_n = cap_dec = 0;
    cap_acc   = 0;
    capturing = true;
    release();                /* que el notch no tape el tono que se quiere medir */
    msg_update_text_fmt("Listening to the birdie...");
}

static void mark_done(void *arg) {
    static float complex x[FFT_N], y[FFT_N];
    static float         p[FFT_N / 2];
    rx_view_t            v = rx_view();
    int                  lo_bin, hi_bin, peak = -1, n = 0;
    float                sorted_mid;

    for (int i = 0; i < FFT_N; i++) {
        x[i] = cap_buf[i] * liquid_hann(i, FFT_N);
    }
    fft_run(FFT_N, x, y, LIQUID_FFT_FORWARD, 0);
    for (int i = 0; i < FFT_N / 2; i++) {
        p[i] = crealf(y[i]) * crealf(y[i]) + cimagf(y[i]) * cimagf(y[i]);
    }

    lo_bin = (int)(v.low * FFT_N / FS) + 1;
    hi_bin = (int)((v.high < MAX_AUDIO_HZ ? v.high : MAX_AUDIO_HZ) * FFT_N / FS) - 1;
    if (hi_bin <= lo_bin + 8) {
        msg_update_text_fmt("Birdies: the filter is too narrow");
        birdies_refresh();
        return;
    }

    /* El pico más fuerte del filtro, contra la mediana del ruido del mismo filtro */
    static float tmp[FFT_N / 2];
    for (int i = lo_bin; i <= hi_bin; i++) {
        tmp[n++] = p[i];
        if (peak < 0 || p[i] > p[peak]) peak = i;
    }
    for (int i = 0; i < n / 2 + 1; i++) {     /* selección parcial: alcanza para la mediana */
        int m = i;
        for (int j = i + 1; j < n; j++) {
            if (tmp[j] < tmp[m]) m = j;
        }
        float t = tmp[i]; tmp[i] = tmp[m]; tmp[m] = t;
    }
    sorted_mid = tmp[n / 2];
    if (peak <= lo_bin || peak >= hi_bin || p[peak] < sorted_mid * 100.0f) {   /* 20 dB sobre el ruido (el ruido solo llega a ~13) */
        msg_update_text_fmt("Birdies: no steady tone heard in the filter");
        birdies_refresh();
        return;
    }

    /* Interpolación parabólica sobre los dB: la frecuencia con menos de 1 Hz de error */
    float a = 10.0f * log10f(p[peak - 1] + 1e-20f), b = 10.0f * log10f(p[peak] + 1e-20f),
          c = 10.0f * log10f(p[peak + 1] + 1e-20f);
    float d = (a - 2 * b + c) != 0.0f ? 0.5f * (a - c) / (a - 2 * b + c) : 0.0f;
    float audio = (peak + d) * FS / FFT_N;
    int32_t hz  = v.lo + (v.sign < 0 ? -1 : 1) * (int32_t)lroundf(audio);

    if (add_birdie(hz, NULL) < 0) {
        msg_update_text_fmt("Birdies: the list is full (%d)", MAX_BIRDIES);
        birdies_refresh();
        return;
    }
    save();
    if (!params.birdies.x) params_bool_set(&params.birdies, true);
    msg_update_text_fmt("Birdie marked at %.3f kHz", hz / 1000.0);
    birdies_refresh();
}

/* ---------- buscar en una banda o en todas ---------- */
/* La radio recorre la banda de a un filtro por vez (unos 2,4 kHz), escucha 0,75 s en cada paso y guarda
 * los tonos fijos: un birdie es una portadora quieta, igual en los 8 trozos de la escucha; la voz, los
 * modos digitales y las portadoras que se corren cambian. Mejor con la antena desconectada: solo quedan
 * los silbidos de la propia radio. "Todas las bandas" recorre las de radioaficionado una tras otra. */

#define SCAN_SETTLE_MS  200
#define SCAN_MAX_TONES  4
#define SCAN_N          (FFT_N / 2)        /* 0,75 s, 1,35 Hz por punto */
#define SCAN_SEGS       8                  /* la escucha en 8 trozos de 0,09 s */
#define SEG_N           (SCAN_N / SCAN_SEGS)
#define MAX_RANGES      32

typedef struct {
    int32_t first, last;    /* diales del primer y el último paso */
} scan_range_t;

static scan_range_t ranges[MAX_RANGES];
static int          n_ranges, cur_range;
static int64_t      steps_total, steps_done;
static int32_t      scan_dial, scan_restore;
static int32_t      scan_a_lo, scan_a_hi;       /* ventana de audio usada en cada paso */
static int          scan_found, scan_new;
static lv_timer_t  *scan_timer, *scan_guard;

static void scan_step(void);

static void scan_capture_cb(lv_timer_t *t) {
    scan_timer = NULL;
    if (!scanning) return;
    cap_len = SCAN_N;
    cap_n = cap_dec = 0;
    cap_acc   = 0;
    capturing = true;
}

/* Potencia por punto de una porción de la escucha */
static void power(const float *buf, int n, float *p) {
    static float complex x[FFT_N], y[FFT_N];

    for (int i = 0; i < n; i++) {
        x[i] = buf[i] * liquid_hann(i, n);
    }
    fft_run(n, x, y, LIQUID_FFT_FORWARD, 0);
    for (int i = 0; i < n / 2; i++) {
        p[i] = crealf(y[i]) * crealf(y[i]) + cimagf(y[i]) * cimagf(y[i]);
    }
}

static float median(const float *p, int lo, int hi) {
    static float tmp[FFT_N / 2];
    int          n = 0;

    for (int i = lo; i <= hi; i++) tmp[n++] = p[i];
    for (int i = 0; i < n / 2 + 1; i++) {
        int m = i;
        for (int j = i + 1; j < n; j++) {
            if (tmp[j] < tmp[m]) m = j;
        }
        float t = tmp[i]; tmp[i] = tmp[m]; tmp[m] = t;
    }
    return tmp[n / 2];
}

static void scan_done(void *arg) {
    static float p[SCAN_N / 2], seg[SCAN_SEGS][SEG_N / 2];
    float        seg_med[SCAN_SEGS];
    rx_view_t    v = rx_view();
    int          lo, hi, tones = 0;
    float        med;

    if (!scanning) return;
    lo  = (int)(scan_a_lo * SCAN_N / FS) + 1;
    hi  = (int)(scan_a_hi * SCAN_N / FS) - 1;
    power(cap_buf, SCAN_N, p);
    med = median(p, lo, hi);
    for (int k = 0; k < SCAN_SEGS; k++) {
        power(cap_buf + k * SEG_N, SEG_N, seg[k]);
        seg_med[k] = median(seg[k], lo / SCAN_SEGS, hi / SCAN_SEGS);
    }

    for (int i = lo + 1; i < hi && tones < SCAN_MAX_TONES; i++) {
        if (p[i] < med * 100.0f || p[i] < p[i - 1] || p[i] < p[i + 1]) continue;   /* pico, 20 dB */
        /* Un birdie está en los 8 trozos, en el mismo lugar y con la misma fuerza (±7 dB);
         * la voz, los modos digitales y las portadoras que se corren no */
        int   j = (i + SCAN_SEGS / 2) / SCAN_SEGS;
        float mn = 1e30f, mx = 0.0f;
        for (int k = 0; k < SCAN_SEGS; k++) {
            float r = 0.0f;
            for (int q = j - 1; q <= j + 1; q++) {
                if (q > 0 && q < SEG_N / 2 && seg[k][q] > r) r = seg[k][q];
            }
            r /= seg_med[k] + 1e-20f;
            if (r < mn) mn = r;
            if (r > mx) mx = r;
        }
        if (mn < 8.0f || mn < mx * 0.2f) continue;

        float a = 10.0f * log10f(p[i - 1] + 1e-20f), b = 10.0f * log10f(p[i] + 1e-20f),
              c = 10.0f * log10f(p[i + 1] + 1e-20f);
        float d     = (a - 2 * b + c) != 0.0f ? 0.5f * (a - c) / (a - 2 * b + c) : 0.0f;
        float audio = (i + d) * FS / SCAN_N;
        int32_t lo_hz = scan_dial + param_i_get(cfg_rit) + subject_i_get((SubjectInt *)cfg_mode_lo_offset);
        int32_t hz    = lo_hz + (v.sign < 0 ? -1 : 1) * (int32_t)lroundf(audio);

        int r = add_birdie(hz, "auto");
        if (r < 0) {
            save();
            scan_stop("Birdies: the list is full (%d)");
            return;
        }
        scan_new += r;
        scan_found++;
        tones++;
        i += 4;      /* los puntos de al lado del mismo tono */
    }
    if (tones) save();

    steps_done++;
    scan_dial += scan_a_hi - scan_a_lo;
    scan_step();
}

static void scan_step(void) {
    if (!scanning) return;
    while (cur_range < n_ranges && scan_dial > ranges[cur_range].last) {
        if (++cur_range < n_ranges) scan_dial = ranges[cur_range].first;
    }
    if (cur_range >= n_ranges) {
        char text[96];
        snprintf(text, sizeof(text), tr("Birdie search done: %d found, %d new"), scan_found, scan_new);
        scan_stop(text);
        return;
    }
    radio_set_freq(scan_dial);
    cap_done   = scan_done;
    scan_timer = lv_timer_create(scan_capture_cb, SCAN_SETTLE_MS, NULL);
    lv_timer_set_repeat_count(scan_timer, 1);
}

/* Si se aprieta el PTT, la búsqueda para y la radio vuelve a su frecuencia antes de salir al aire */
static void scan_guard_cb(lv_timer_t *t) {
    if (scanning && radio_get_state() != RADIO_RX) scan_stop("Birdie search stopped");
}

static void scan_stop(const char *why) {
    if (!scanning) return;
    scanning  = false;
    capturing = false;
    if (scan_timer) {
        lv_timer_del(scan_timer);
        scan_timer = NULL;
    }
    if (scan_guard) {
        lv_timer_del(scan_guard);
        scan_guard = NULL;
    }
    cap_done = mark_done;
    cap_len  = FFT_N;
    radio_set_freq(scan_restore);
    if (why) msg_update_text_fmt(why, MAX_BIRDIES);
    birdies_refresh();
}

bool birdies_scanning(void) {
    return scanning;
}

int birdies_scan_progress(void) {
    if (!scanning || steps_total <= 0) return 0;
    int p = (int)(100 * steps_done / steps_total);
    return p > 99 ? 99 : p;
}

int birdies_scan_found(void) {
    return scan_found;
}

/* Minutos que faltan, aproximados */
int birdies_scan_minutes_left(void) {
    if (!scanning) return 0;
    int64_t left = steps_total - steps_done;
    return (int)((left * (SCAN_SETTLE_MS + SCAN_N * 1000 / (int)FS + 60) / 1000 + 59) / 60);
}

/* Diales del primer y último paso para cubrir [start, stop] con la ventana de audio */
static void add_range(const rx_view_t *v, int32_t start, int32_t stop) {
    int32_t lo_off = param_i_get(cfg_rit) + subject_i_get((SubjectInt *)cfg_mode_lo_offset);
    int32_t step   = scan_a_hi - scan_a_lo;

    if (n_ranges >= MAX_RANGES || stop <= start) return;
    scan_range_t *r = &ranges[n_ranges++];
    if (v->sign < 0) {
        r->first = start - lo_off + scan_a_hi;
        r->last  = stop - lo_off + scan_a_lo;
    } else {
        r->first = start - lo_off - scan_a_lo;
        r->last  = stop - lo_off - scan_a_hi;
    }
    if (r->last < r->first) r->last = r->first;
    steps_total += (r->last - r->first) / step + 1;
}

void birdies_scan(bool all_bands) {
    x6100_mode_t mode = (x6100_mode_t)subject_i_get((SubjectInt *)cfg_cur_mode);
    int32_t      fg   = subject_i_get((SubjectInt *)cfg_fg_freq);
    int32_t      start, stop;

    if (scanning) {
        scan_stop("Birdie search stopped");
        return;
    }
    if (capturing) return;
    if (mode == x6100_mode_am || mode == x6100_mode_nfm) {
        msg_update_text_fmt("Birdies: search in USB, LSB or CW");
        return;
    }
    rx_view_t v = rx_view();
    scan_a_lo = v.low < 200 ? 200 : v.low;
    scan_a_hi = v.high > MAX_AUDIO_HZ ? MAX_AUDIO_HZ : v.high;
    if (scan_a_hi - scan_a_lo < 500) {
        msg_update_text_fmt("Birdies: the filter is too narrow");
        return;
    }

    n_ranges = cur_range = 0;
    steps_total = steps_done = 0;
    if (all_bands) {
        int32_t s[MAX_RANGES], e[MAX_RANGES];
        int     n = cfg_ham_bands(s, e, MAX_RANGES);
        for (int i = 0; i < n; i++) add_range(&v, s[i], e[i]);
    } else {
        if (!cfg_band_limits(fg, &start, &stop)) {
            msg_update_text_fmt("Birdies: tune inside a band first");
            return;
        }
        add_range(&v, start, stop);
    }
    if (n_ranges == 0) {
        msg_update_text_fmt("Birdies: tune inside a band first");
        return;
    }

    scan_dial    = ranges[0].first;
    scan_restore = fg;
    scan_found = scan_new = 0;
    scanning   = true;
    release();
    scan_guard = lv_timer_create(scan_guard_cb, 100, NULL);
    if (all_bands) {
        msg_update_text_fmt("Searching all bands for birdies, about %d min (antenna off is best)...",
                            birdies_scan_minutes_left());
    } else {
        msg_update_text_fmt("Searching the band for birdies (antenna off is best)...");
    }
    scan_step();
}

/* ---------- quitar ---------- */

void birdies_remove_here(void) {
    rx_view_t v = rx_view();
    int32_t   audio;
    int       i = birdie_in_passband(&v, &audio);

    if (i < 0) {
        msg_update_text_fmt("Birdies: none in the filter here");
        return;
    }
    int32_t hz = list[i].hz;
    memmove(&list[i], &list[i + 1], (count - i - 1) * sizeof(list[0]));
    count--;
    save();
    msg_update_text_fmt("Birdie at %.3f kHz removed", hz / 1000.0);
    birdies_refresh();
}

void birdies_clear(void) {
    count = 0;
    save();
    birdies_refresh();
    msg_update_text_fmt("Birdie list cleared");
}

int birdies_count(void) {
    return count;
}

bool birdies_active(void) {
    return notch_on;
}

void birdies_init(void) {
    load();
    subject_subscribe((Subject *)cfg_fg_freq, on_change, NULL);
    subject_subscribe((Subject *)cfg_cur_mode, on_change, NULL);
    subject_subscribe((Subject *)cfg_cur_filter_low, on_change, NULL);
    subject_subscribe((Subject *)cfg_cur_filter_high, on_change, NULL);
    subject_subscribe((Subject *)cfg_mode_lo_offset, on_change, NULL);
    subject_subscribe((Subject *)cfg_rit, on_change, NULL);
    subject_subscribe((Subject *)cfg_dnf, on_change, NULL);
    subject_subscribe((Subject *)cfg_dnf_auto, on_change, NULL);
    subject_subscribe((Subject *)cfg_dnf_center, on_change, NULL);
    subject_subscribe((Subject *)cfg_dnf_width, on_change, NULL);
    lv_timer_create(file_timer_cb, 2000, NULL);
    birdies_refresh();
}
