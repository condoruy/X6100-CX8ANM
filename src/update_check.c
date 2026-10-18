/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - new release alert: once after start, as soon as there is a network,
 *  x6100-link.it/api/latest.json (the release IU1WXY marked to notify, as X6100 Link reads it);
 *  a newer one than this firmware shows a message. Settings > Info > Update alert turns it off.
 *  Its "min_fw" (set on the site's admin page) is the oldest firmware still fine: an older one
 *  asks to update at every start, whatever Update alert says.
 *  Python fetches it: BusyBox wget has no HTTPS.
 *
 *  Check of the version order on the host:
 *    gcc -DUPDATE_CHECK_TEST -Isrc -o /tmp/uc src/update_check.c && /tmp/uc
 */

#include "update_check.h"

#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* "v0.112" > "v0.99": number by number */
bool update_newer(const char *a, const char *b) {
    a += *a == 'v';
    b += *b == 'v';
    while (*a || *b) {
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);

        if (x != y) return x > y;
        a += *a == '.';
        b += *b == '.';
        if ((*a && (*a < '0' || *a > '9')) || (*b && (*b < '0' || *b > '9'))) break;
    }
    return false;
}

/* AA00, AA00aa, AA00aa00: else left out (the site places the radio from its IP) */
static bool grid_ok(const char *g) {
    size_t n = strlen(g);

    if (n != 4 && n != 6 && n != 8) return false;
    for (size_t i = 0; i < n; i++) {
        char c   = (char)toupper((unsigned char)g[i]);
        char top = i < 2 ? 'R' : 'X';
        if (i % 4 < 2 ? (c < 'A' || c > top) : (c < '0' || c > '9')) return false;
    }
    return true;
}

#ifdef UPDATE_CHECK_TEST
#include <assert.h>
int main(void) {
    assert(update_newer("v0.112", "v0.111"));
    assert(update_newer("v0.100", "v0.99"));
    assert(!update_newer("v0.112", "v0.112"));
    assert(!update_newer("v0.99", "v0.112"));
    assert(update_newer("v1.0", "v0.200"));
    assert(grid_ok("JN35") && grid_ok("jn35sc") && grid_ok("JN35SC12"));
    assert(!grid_ok("") && !grid_ok("JN3") && !grid_ok("ZZ35") && !grid_ok("JN35SZ") && !grid_ok("35JN"));
    puts("ok");
    return 0;
}
#else

#include "cJSON.h"
#include "main.h"
#include "msg.h"
#include "params/params.h"
#include "station.h"

/* CX8ANM: solo las compilaciones propias (github.com/condoruy/X6100-CX8ANM, Actions > Build image):
 * la última que salió bien tiene el número de ejecución, el último de la versión (1.0.N). El aviso
 * de x6100-link.it era de la edición de IU1WXY y pedía actualizar sin cambios propios. */
#define FETCH "python3 -c \"import urllib.request as u,json;" \
              "r=u.Request('https://api.github.com/repos/condoruy/X6100-CX8ANM/actions/workflows/main.yml/runs" \
              "?status=success&branch=main&per_page=1',headers={'User-Agent':'x6100-cx8anm'});" \
              "print(json.load(u.urlopen(r,timeout=15))['workflow_runs'][0]['run_number'])\" 2>/dev/null"

static void *check_thread(void *arg) {
    (void)arg;
    const char *own = strrchr(CX8ANM_VERSION, '.');
    int         own_n;

    if (!own || (own_n = atoi(own + 1)) <= 0) return NULL;   /* "dev": compilada a mano */
    /* up to ~20 minutes for the WiFi to come up */
    for (int i = 0; i < 20; i++) {
        char  buf[64] = "";
        FILE *p;

        sleep(i ? 60 : 30);
        if ((p = popen(FETCH, "r"))) {
            if (!fgets(buf, sizeof(buf), p)) buf[0] = 0;
            pclose(p);
        }
        if (!buf[0]) continue;      /* no network yet */

        int latest = atoi(buf);
        if (latest > own_n && params.update_alert.x) {
            char ver[32];
            snprintf(ver, sizeof(ver), "%.*s%d", (int)(own - CX8ANM_VERSION + 1), CX8ANM_VERSION, latest);
            msg_schedule_long_text_fmt("New CX8ANM version %s: download it from GitHub (Actions)", ver);
        }
        break;
    }
    return NULL;
}

/* Device ping for the site's map and count of radios on (privacy notice): every 5 minutes and
 * at once when the network comes up, only with a default route. An anonymous random id kept
 * in /mnt, the firmware version, the QTH grid when valid. A failure waits for the next round. */
#define DEVICE_ID "/mnt/device_id"

static bool net_up(void) {
    FILE *f = fopen("/proc/net/route", "r");
    char  line[256], ifc[32];
    bool  up = false;

    if (!f) return false;
    while (!up && fgets(line, sizeof(line), f)) {
        unsigned long dest;
        up = sscanf(line, "%31s %lx", ifc, &dest) == 2 && dest == 0 && strcmp(ifc, "lo");
    }
    fclose(f);
    return up;
}

static void device_id(char *id, size_t size) {
    FILE *f = fopen(DEVICE_ID, "r");

    id[0] = 0;
    if (f) {
        if (fgets(id, (int)size, f)) id[strcspn(id, "\r\n")] = 0;
        fclose(f);
    }
    if (strlen(id) >= 8 && strspn(id, "0123456789abcdef") == strlen(id)) return;

    unsigned char r[16] = {0};
    if ((f = fopen("/dev/urandom", "r"))) {
        if (fread(r, 1, sizeof(r), f) != sizeof(r)) r[0] ^= 1;
        fclose(f);
    }
    for (int i = 0; i < 16 && 2 * i + 2 < (int)size; i++) sprintf(id + 2 * i, "%02x", r[i]);
    if ((f = fopen(DEVICE_ID, "w"))) {
        fprintf(f, "%s\n", id);
        fclose(f);
    }
}

/* The station callsign for the site (it finds the user by it, IU1WXY/P as IU1WXY): upper case,
 * [A-Z0-9/] of 3-15 characters, else "" (not sent) */
static void ping_call(char *call, size_t size) {
    size_t n = 0;

    for (const char *c = params.callsign.x; *c && n + 1 < size; c++) {
        char u = (char)toupper((unsigned char)*c);
        if (!isalnum((unsigned char)u) && u != '/') { n = 0; break; }
        call[n++] = u;
    }
    call[n] = '\0';
    if (n < 3 || n > 15) call[0] = '\0';
}

/* The CID of the microSD the radio runs from (mmcblk0, the root): maker, OEM, name, revision,
 * serial and date, 32 hex digits; the serial alone repeats on no-brand cards. Else "" */
static void ping_sd(char *sd, size_t size) {
    FILE *f = fopen("/sys/block/mmcblk0/device/cid", "r");

    sd[0] = '\0';
    if (!f) return;
    if (fscanf(f, "%32[0-9a-fA-F]", sd) != 1 || strlen(sd) != 32) sd[0] = '\0';
    fclose(f);
    for (char *c = sd; *c; c++) *c = (char)tolower((unsigned char)*c);
}

/* The SoC serial (Allwinner SID, fixed in the chip): the radio itself, whatever SD it runs from.
 * 16 hex digits from /proc/cpuinfo, else "" */
static void ping_hw(char *hw, size_t size) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    char  line[128];

    hw[0] = '\0';
    if (!f) return;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "Serial : %16[0-9a-fA-F]", hw) == 1) break;
    fclose(f);
    if (strlen(hw) != 16) hw[0] = '\0';
    for (char *c = hw; *c; c++) *c = (char)tolower((unsigned char)*c);
    (void)size;
}

static void *ping_thread(void *arg) {
    char id[40], qth[16], call[20], sd[36], hw[20], cmd[460];
    bool was_up = false;
    int  wait = 0;

    (void)arg;
    device_id(id, sizeof(id));
    ping_sd(sd, sizeof(sd));
    ping_hw(hw, sizeof(hw));
    while (1) {
        bool up = net_up();

        if (up && (!was_up || wait <= 0)) {
            snprintf(qth, sizeof(qth), "%s", station_qth());
            ping_call(call, sizeof(call));
            snprintf(cmd, sizeof(cmd),
                     "python3 -c \"import urllib.request as u;u.urlopen('https://x6100-link.it/api/ping.php"
                     "?id=%s&kind=radio&ver=%s%s%s%s%s%s%s%s%s',timeout=15)\" >/dev/null 2>&1",
                     id, strrchr(VERSION, ' ') + 1, grid_ok(qth) ? "&qth=" : "", grid_ok(qth) ? qth : "",
                     call[0] ? "&call=" : "", call, sd[0] ? "&sd=" : "", sd, hw[0] ? "&hw=" : "", hw);
            if (system(cmd) == -1) { /* nothing to do: the next round tries again */ }
            wait = 300;
        }
        was_up = up;
        sleep(30);
        wait -= 30;
    }
    return NULL;
}

void update_check_start(void) {
    pthread_t t;

    if (pthread_create(&t, NULL, check_thread, NULL) == 0)
        pthread_detach(t);
    if (pthread_create(&t, NULL, ping_thread, NULL) == 0)
        pthread_detach(t);
}
#endif
