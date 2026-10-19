/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Espejo de la pantalla para la página web "Equipo" (CX8ANM / Cóndor Tecnológico).
 *
 *  El servidor web toca MIRROR_FLAG mientras alguien mira la página. Solo
 *  entonces, MIRROR_FPS veces por segundo y si la pantalla cambió, el hilo de
 *  LVGL copia la pantalla (drm_capture) y un hilo aparte la comprime en JPEG y
 *  la deja en MIRROR_JPEG (se escribe aparte y se renombra: nunca a medias).
 *  Sin nadie mirando no se hace nada. Las teclas y perillas de la página
 *  llegan por otro lado: el servidor web las escribe en /dev/input/eventN,
 *  como si fueran las del frente.
 */

#include "lv_drivers/display/drm.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <jpeglib.h>

#include "lvgl/lvgl.h"
#include "mirror.h"

#if USE_DRM

#define MIRROR_FLAG     "/tmp/x6100-mirror"         /* tocado por el servidor web */
#define MIRROR_JPEG     "/tmp/x6100-screen.jpg"
#define MIRROR_FPS      6
#define MIRROR_QUALITY  65
#define MIRROR_IDLE_S   4                           /* sin pedidos: se deja de capturar */
#define MIRROR_KEEP_MS  2000                        /* aun sin cambios, una cada 2 s */
#define MAX_PX          (800 * 480)

static pthread_mutex_t  mut  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   cond = PTHREAD_COND_INITIALIZER;
static uint32_t         *px;            /* la última captura */
static int32_t          px_w, px_h;
static bool             pending;        /* captura lista para comprimir */
static bool             busy;           /* el hilo está comprimiendo */

static bool             have_drm = true;
static uint32_t         last_seq;
static int64_t          last_ms;

static int64_t now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

static void jpeg_fail(j_common_ptr c) {
    longjmp(*(jmp_buf *) c->client_data, 1);
}

static bool write_jpeg(const uint32_t *src, int32_t w, int32_t h) {
    struct jpeg_compress_struct c;
    struct jpeg_error_mgr       jerr;
    jmp_buf                     jb;
    FILE                        *f = fopen(MIRROR_JPEG ".tmp", "wb");

    if (!f) {
        return false;
    }
    c.err = jpeg_std_error(&jerr);
    jerr.error_exit = jpeg_fail;
    jpeg_create_compress(&c);
    c.client_data = &jb;
    if (setjmp(jb)) {
        jpeg_destroy_compress(&c);
        fclose(f);
        unlink(MIRROR_JPEG ".tmp");
        return false;
    }
    jpeg_stdio_dest(&c, f);
    c.image_width = w;
    c.image_height = h;
    c.input_components = 4;
    c.in_color_space = JCS_EXT_BGRX;
    jpeg_set_defaults(&c);
    jpeg_set_quality(&c, MIRROR_QUALITY, TRUE);
    c.dct_method = JDCT_IFAST;
    jpeg_start_compress(&c, TRUE);
    while (c.next_scanline < (JDIMENSION) h) {
        JSAMPROW row = (JSAMPROW) (src + c.next_scanline * w);

        jpeg_write_scanlines(&c, &row, 1);
    }
    jpeg_finish_compress(&c);
    jpeg_destroy_compress(&c);
    if (fclose(f) != 0) {
        unlink(MIRROR_JPEG ".tmp");
        return false;
    }
    return rename(MIRROR_JPEG ".tmp", MIRROR_JPEG) == 0;
}

static void * encoder_thread(void *arg) {
    static uint32_t work[MAX_PX];

    setpriority(PRIO_PROCESS, 0, 10);       /* después de la radio y la pantalla */
    while (true) {
        int32_t w, h;

        pthread_mutex_lock(&mut);
        while (!pending) {
            pthread_cond_wait(&cond, &mut);
        }
        memcpy(work, px, px_w * px_h * 4);
        w = px_w;
        h = px_h;
        pending = false;
        busy = true;
        pthread_mutex_unlock(&mut);

        if (!write_jpeg(work, w, h)) {
            LV_LOG_WARN("mirror: can't write " MIRROR_JPEG);
        }

        pthread_mutex_lock(&mut);
        busy = false;
        pthread_mutex_unlock(&mut);
    }
    return NULL;
}

static int want_fps = 2;    /* lo que pide la página: menos cuadros, menos WiFi y menos ruido en la radio */

/* ¿Alguien mira la página? (el servidor toca el archivo en cada pedido y escribe ahí los
 * cuadros por segundo que eligió la página: 1 a MIRROR_FPS) */
static bool watched(void) {
    struct stat st;
    static time_t read_at;

    if (stat(MIRROR_FLAG, &st) != 0 || time(NULL) - st.st_mtime > MIRROR_IDLE_S) {
        return false;
    }
    if (st.st_mtime != read_at) {
        FILE *f = fopen(MIRROR_FLAG, "r");
        int   n = 0;

        read_at = st.st_mtime;
        if (f) {
            if (fscanf(f, "%d", &n) == 1 && n >= 1) {
                want_fps = n > MIRROR_FPS ? MIRROR_FPS : n;
            }
            fclose(f);
        }
    }
    return true;
}

static void timer_cb(lv_timer_t *t) {
    static int skip;
    uint32_t   seq;
    int64_t    ms;
    bool       free_slot;

    if (!have_drm) {
        return;
    }
    /* stat() una vez por segundo mientras nadie mira */
    if (!last_ms && ++skip % MIRROR_FPS) {
        return;
    }
    if (!watched()) {
        last_ms = 0;
        return;
    }
    seq = drm_frame_seq();
    ms  = now_ms();
    if (last_ms && seq == last_seq && ms - last_ms < MIRROR_KEEP_MS) {
        return;
    }
    if (last_ms && ms - last_ms < 1000 / want_fps - 20) {
        return;                             /* más cuadros que los pedidos */
    }

    pthread_mutex_lock(&mut);
    free_slot = !pending && !busy;
    pthread_mutex_unlock(&mut);
    if (!free_slot) {
        return;                             /* el JPEG anterior todavía se comprime */
    }

    /* Copia en el hilo de LVGL (mismo hilo que dibuja): sin cuadros a medias */
    pthread_mutex_lock(&mut);
    have_drm = drm_capture(px, &px_w, &px_h);
    pending  = have_drm;
    pthread_mutex_unlock(&mut);
    if (have_drm) {
        pthread_cond_signal(&cond);
        last_seq = seq;
        last_ms  = ms;
    } else {
        LV_LOG_WARN("mirror: no DRM screen to copy, mirror off");
    }
}

void mirror_init(void) {
    pthread_t thread;

    px = malloc(MAX_PX * 4);
    if (!px) {
        return;
    }
    unlink(MIRROR_JPEG);
    if (pthread_create(&thread, NULL, encoder_thread, NULL) != 0) {
        return;
    }
    pthread_detach(thread);
    lv_timer_create(timer_cb, 1000 / MIRROR_FPS, NULL);
}

#else

void mirror_init(void) {
}

#endif
