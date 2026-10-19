/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - Modo ahorro y aviso de la banda base (CX8ANM)
 *
 *  El sistema arranca con el procesador siempre al máximo ("performance"). Con Modo ahorro
 *  se usa "schedutil" (o "ondemand"): sube la velocidad en cuanto hay trabajo (DSP, FT8,
 *  cascada) y la baja en reposo. La pantalla se atenúa antes (como mucho a los 15 s).
 *  El consumo real se ve en /mnt/battery_log.csv (cpu_mhz, vbat, batcap cada 30 s).
 */

#include "power_save.h"

#include "params/params.h"
#include "update_check.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define POWER_SAVE_DIM_S    15
#define CPU_MAX             4

/* Última banda base oficial de Xiegu conocida: BASE V1.1.8 (15/09/2024, publicada el 23/09/2024).
 * Cuando Xiegu publique otra, se cambia acá. */
#define BASEBAND_LATEST     "1.1.8"

static bool write_governor(int cpu, const char *gov) {
    char  path[80];
    FILE *f;

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cpu);
    if (!(f = fopen(path, "w"))) return false;
    bool ok = fputs(gov, f) >= 0;
    ok = (fclose(f) == 0) && ok;
    return ok;
}

void power_save_apply(void) {
    for (int cpu = 0; cpu < CPU_MAX; cpu++) {
        if (!params.power_save.x) {
            write_governor(cpu, "performance");
        } else if (!write_governor(cpu, "schedutil")) {
            write_governor(cpu, "ondemand");
        }
    }
}

int power_save_dim_timeout(int timeout_s) {
    if (params.power_save.x && timeout_s > POWER_SAVE_DIM_S) return POWER_SAVE_DIM_S;
    return timeout_s;
}

/* "V1.1.8 Sep 15 2024,15:22:37" -> "1.1.8" */
static bool baseband_number(const char *s, char *out, size_t size) {
    for (; s && *s; s++) {
        if (isdigit((unsigned char)*s)) {
            size_t n = 0;
            while ((isdigit((unsigned char)s[n]) || s[n] == '.') && n + 1 < size) {
                out[n] = s[n];
                n++;
            }
            out[n] = 0;
            return strchr(out, '.') != NULL;
        }
    }
    return false;
}

const char *power_save_baseband_text(const char *version) {
    static char buf[160];
    char        num[24];

    if (!version || !*version) return "?";
    if (!baseband_number(version, num, sizeof(num))) {
        snprintf(buf, sizeof(buf), "%s", version);
    } else if (update_newer(BASEBAND_LATEST, num)) {
        snprintf(buf, sizeof(buf), "%s  -  Xiegu %s", version, BASEBAND_LATEST);
    } else {
        snprintf(buf, sizeof(buf), "%s  (OK)", version);
    }
    return buf;
}
