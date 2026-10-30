/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  CX8ANM: voz desde el navegador (remote_voice.h).
 */
#include "remote_voice.h"

#include "audio.h"
#include "cfg/cfg_api.h"
#include "dialog.h"
#include "msg.h"
#include "radio.h"

#include <aether_radio/x6100_control/control.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define PREBUF_SAMPLES  (VOICE_RATE * 3 / 10)      /* 0,3 s antes de salir al aire */
#define LEAD_IN_US      100000                      /* el relé y el modem, como los mensajes de voz */
#define IN_SAMPLES      2048
#define OUT_MAX         (IN_SAMPLES * AUDIO_PLAY_RATE / VOICE_RATE + 8)

static volatile bool on_air;
static volatile bool run;

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool remote_voice_on_air(void) {
    return on_air;
}

static bool voice_mode(void) {
    int32_t mode = cparam_i_get(cfg_cur_mode);
    return mode == x6100_mode_lsb || mode == x6100_mode_usb || mode == x6100_mode_am || mode == x6100_mode_nfm;
}

/* Por qué no se puede transmitir ahora (NULL: se puede) */
static const char *refuse(void) {
    if (radio_get_state() != RADIO_RX) return "the radio is already transmitting";
    if (dialog_is_run()) return "close the app open on the radio first";
    if (!voice_mode()) return "voice needs LSB, USB, AM or FM";
    return NULL;
}

/* 16 kHz -> la frecuencia de reproducción, lineal; pos y last siguen de un bloque al otro */
static size_t resample(const int16_t *in, size_t n, int16_t *out, double *pos, int16_t *last) {
    const double step = (double)VOICE_RATE / AUDIO_PLAY_RATE;
    size_t       k    = 0;

    while (*pos < n && k < OUT_MAX) {
        size_t  i = (size_t)*pos;
        double  f = *pos - i;
        int16_t a = i == 0 ? *last : in[i - 1];
        int16_t b = in[i];
        out[k++] = (int16_t)(a + (b - a) * f);
        *pos += step;
    }
    *pos -= n;
    if (n) *last = in[n - 1];
    return k;
}

static void key(bool tx) {
    radio_set_modem(tx);
    on_air = tx;
}

static void serve(int fd) {
    static int16_t in[IN_SAMPLES], pre[PREBUF_SAMPLES], out[OUT_MAX];
    size_t         pre_n = 0, have_bytes = 0;
    double         pos   = 0;
    int16_t        last  = 0;
    uint64_t       start = 0, last_data = now_ms();
    bool           keyed = false, clean_end = false;
    const char    *why   = refuse();
    char           line[96];

    snprintf(line, sizeof(line), why ? "ERR %s\n" : "OK\n", why);
    if (write(fd, line, strlen(line)) < 0 || why) return;

    run = true;
    while (run) {
        struct pollfd p = {.fd = fd, .events = POLLIN};
        int           r = poll(&p, 1, 100);
        uint64_t      t = now_ms();

        if (r < 0 && errno != EINTR) break;
        if (r > 0) {
            ssize_t got = read(fd, (char *)in + have_bytes, sizeof(in) - have_bytes);
            if (got <= 0) {     /* el navegador soltó el PTT */
                clean_end = true;
                break;
            }
            have_bytes += got;
            last_data = t;
        }
        if (t - last_data > VOICE_GAP_MS) {
            msg_schedule_text_fmt("Web voice: no audio, TX stopped");
            break;
        }
        if (keyed && t - start > VOICE_MAX_S * 1000) {
            msg_schedule_text_fmt("Web voice: 3 minutes, TX stopped");
            break;
        }
        if (keyed && (dialog_is_run() || !voice_mode())) break;

        size_t n = have_bytes / 2;
        if (!n) continue;

        if (!keyed) {
            /* juntar 0,3 s, así la red puede atrasarse un poco sin cortar la voz */
            size_t take = n < PREBUF_SAMPLES - pre_n ? n : PREBUF_SAMPLES - pre_n;
            memcpy(pre + pre_n, in, take * 2);
            pre_n += take;
            memmove(in, in + take, have_bytes - take * 2);
            have_bytes -= take * 2;
            if (pre_n < PREBUF_SAMPLES) continue;
            if ((why = refuse())) {
                msg_schedule_text_fmt("Web voice: %s", why);
                break;
            }
            key(true);
            keyed = true;
            start = now_ms();
            usleep(LEAD_IN_US);
            size_t k = resample(pre, pre_n, out, &pos, &last);
            if (audio_play_until(out, k, &run, 1500) < 0) break;
            n = have_bytes / 2;
            if (!n) continue;
        }
        size_t k = resample(in, n, out, &pos, &last);
        memmove(in, in + n, have_bytes - n * 2);
        have_bytes -= n * 2;
        if (audio_play_until(out, k, &run, 1500) < 0) break;
    }
    if (keyed) {
        if (clean_end) audio_play_wait();
        else audio_play_flush();
        key(false);
    }
    run = false;
}

static void *voice_thread(void *arg) {
    int lfd = (int)(intptr_t)arg;

    while (true) {
        int fd = accept(lfd, NULL, NULL);
        if (fd < 0) {
            if (errno != EINTR) usleep(200000);
            continue;
        }
        serve(fd);
        close(fd);
    }
    return NULL;
}

void remote_voice_init(void) {
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    pthread_t          th;
    int                fd;

    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", REMOTE_VOICE_SOCK);
    unlink(REMOTE_VOICE_SOCK);
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, 2) < 0) {
        if (fd >= 0) close(fd);
        return;
    }
    if (pthread_create(&th, NULL, voice_thread, (void *)(intptr_t)fd) == 0) pthread_detach(th);
}
