/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2024 Georgy Dyuldin aka R2RFE
 */

#define _GNU_SOURCE

#include "migrations.h"

#include "db.h"
#include "../cfg/digital_modes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZEOF_ARRAY(arr) (sizeof(arr) > 0 ? sizeof(arr) / sizeof(arr[0]) : 0)

static sqlite3_stmt *update_ver_stmt;

static int get_current_version(int * ver);
static int set_current_version(int ver);

/* Migrations functions */
static int _0_init_migrations() {
    int rc;
    rc = sqlite3_exec(db, "INSERT INTO version(id) VALUES(0)", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot insert 0 version: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

static int _1_create_ftx_table() {
    int rc;
    char *query;
    rc = asprintf(&query,
        "BEGIN;"
        "CREATE TABLE IF NOT EXISTS digital_modes("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "label varchar(64) NOT NULL, "
            "freq INTEGER NOT NULL CHECK(freq > 0), "
            "mode INTEGER NOT NULL DEFAULT 3 CHECK(mode >= 0 AND mode <= 7), "
            "type INTEGER NOT NULL, "
            "CONSTRAINT freq_type_uniq UNIQUE(freq, type) "
        ");"
        "CREATE INDEX IF NOT EXISTS digital_modes_type_idx ON digital_modes (type);"
        "CREATE INDEX IF NOT EXISTS digital_modes_freq_idx ON digital_modes (freq);"
        "INSERT INTO digital_modes(label, freq, mode, type) "
        "SELECT m1.val, m2.val, m3.val, %u  "
        "FROM memory AS m1  "
        "INNER JOIN memory AS m2 ON m1.id == m2.id  "
        "INNER JOIN memory AS m3 ON m1.id == m3.id  "
        "WHERE m1.name='label' AND m2.name='vfoa_freq' AND m3.name = 'vfoa_mode' AND m1.id >= 100 AND m1.id < 200; "
        "INSERT INTO digital_modes(label, freq, mode, type) "
        "SELECT m1.val, m2.val, m3.val, %u  "
        "FROM memory AS m1  "
        "INNER JOIN memory AS m2 ON m1.id == m2.id  "
        "INNER JOIN memory AS m3 ON m1.id == m3.id  "
        "WHERE m1.name='label' AND m2.name='vfoa_freq' AND m3.name = 'vfoa_mode' AND m1.id >= 200; "
        "COMMIT",
        CFG_DIG_TYPE_FT8, CFG_DIG_TYPE_FT4
    );
    if (rc == -1) {
        printf("Cannot allocate SQL query\n");
        return 1;
    }
    rc = sqlite3_exec(db, query, NULL, NULL, NULL);
    free(query);
    if (rc != SQLITE_OK) {
        printf("Cannot migrate: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

static int _2_update_atu_freq() {
    int rc;
    rc = sqlite3_exec(db, "UPDATE atu SET freq=freq * 50000 + 25000 WHERE freq < 500000", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot update ATU frequencies: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

static int _3_update_spectrum_peak_hold() {
    int rc;
    // spectrum peak hold from 1ms to 1s
    rc = sqlite3_exec(db, "UPDATE params SET val = val / 1000 WHERE name = 'spectrum_peak_hold'", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot update spectrum peak hold: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* Migrations array */
static int _4_add_js8_presets() {
    int rc;
    char *query;
    /* JS8Call's default dial frequencies (its FrequencyList.cpp), USB-D like
     * the FT8/FT4 rows. OR IGNORE keeps rows a fresh params.db already has. */
    rc = asprintf(&query,
        "INSERT OR IGNORE INTO digital_modes(label, freq, mode, type) "
        "SELECT column1, column2, 3, %u FROM (VALUES "
            "('JS8 160m', 1842000),"
            "('JS8 80m', 3578000),"
            "('JS8 40m', 7078000),"
            "('JS8 30m', 10130000),"
            "('JS8 20m', 14078000),"
            "('JS8 17m', 18104000),"
            "('JS8 15m', 21078000),"
            "('JS8 12m', 24922000),"
            "('JS8 10m', 28078000),"
            "('JS8 6m', 50318000)"
        ")",
        CFG_DIG_TYPE_JS8
    );
    if (rc == -1) {
        printf("Cannot allocate SQL query\n");
        return 1;
    }
    rc = sqlite3_exec(db, query, NULL, NULL, NULL);
    free(query);
    if (rc != SQLITE_OK) {
        printf("Cannot add JS8 presets: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* GhostNet's JS8 nets (s2underground/GhostNet, version 1.5): 7.107 MHz is
 * the main one, 14.107 and 3.575 MHz the others. */
static int _5_add_ghostnet_presets() {
    int rc;
    char *query;
    rc = asprintf(&query,
        "INSERT OR IGNORE INTO digital_modes(label, freq, mode, type) "
        "SELECT column1, column2, 3, %u FROM (VALUES "
            "('GhostNet 80m', 3575000),"
            "('GhostNet 40m', 7107000),"
            "('GhostNet 20m', 14107000)"
        ")",
        CFG_DIG_TYPE_JS8_GHOSTNET
    );
    if (rc == -1) {
        printf("Cannot allocate SQL query\n");
        return 1;
    }
    rc = sqlite3_exec(db, query, NULL, NULL, NULL);
    free(query);
    if (rc != SQLITE_OK) {
        printf("Cannot add GhostNet presets: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* WSPR dial frequencies (WSJT-X's defaults) */
static int _6_add_wspr_presets() {
    int rc;
    char *query;
    rc = asprintf(&query,
        "INSERT OR IGNORE INTO digital_modes(label, freq, mode, type) "
        "SELECT column1, column2, 3, %u FROM (VALUES "
            "('WSPR 160m', 1836600),"
            "('WSPR 80m', 3568600),"
            "('WSPR 60m', 5287200),"
            "('WSPR 40m', 7038600),"
            "('WSPR 30m', 10138700),"
            "('WSPR 20m', 14095600),"
            "('WSPR 17m', 18104600),"
            "('WSPR 15m', 21094600),"
            "('WSPR 12m', 24924600),"
            "('WSPR 10m', 28124600),"
            "('WSPR 6m', 50293000)"
        ")",
        CFG_DIG_TYPE_WSPR
    );
    if (rc == -1) {
        printf("Cannot allocate SQL query\n");
        return 1;
    }
    rc = sqlite3_exec(db, query, NULL, NULL, NULL);
    free(query);
    if (rc != SQLITE_OK) {
        printf("Cannot add WSPR presets: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* FT2 dial frequencies (IU8LMC's Decodium list, from SP2KMO's upstream 76d98ce2) */
static int _7_add_ft2_presets() {
    int rc;
    char *query;
    rc = asprintf(&query,
        "INSERT OR IGNORE INTO digital_modes(label, freq, mode, type) "
        "SELECT column1, column2, 3, %u FROM (VALUES "
            "('FT2 160m', 1843000),"
            "('FT2 80m', 3568000),"
            "('FT2 60m', 5360000),"
            "('FT2 40m', 7062000),"
            "('FT2 30m', 10144000),"
            "('FT2 20m', 14084000),"
            "('FT2 17m', 18108000),"
            "('FT2 15m', 21144000),"
            "('FT2 12m', 24923000),"
            "('FT2 10m', 28184000),"
            "('FT2 6m', 50316000)"
        ")",
        CFG_DIG_TYPE_FT2
    );
    if (rc == -1) {
        printf("Cannot allocate SQL query\n");
        return 1;
    }
    rc = sqlite3_exec(db, query, NULL, NULL, NULL);
    free(query);
    if (rc != SQLITE_OK) {
        printf("Cannot add FT2 presets: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    return 0;
}

/* CX8ANM: bandas completas para la Región 2 (Uruguay): 160m desde 1.800, 80m hasta 4.000,
 * 40m hasta 7.300, 6m hasta 54; nuevas 2200m, 630m, 60m y AM (onda media 530-1710 kHz);
 * radiodifusión 120m y 90m. Sin tabla de bandas no hace nada: bands_check() carga las de fábrica. */
static int _8_bands_region2() {
    sqlite3_stmt *st;
    int           rc;

    if (sqlite3_prepare_v2(db, "SELECT 1 FROM bands LIMIT 1", -1, &st, 0) != SQLITE_OK)
        return 0;
    sqlite3_finalize(st);
    rc = sqlite3_exec(db,
        "BEGIN;"
        "UPDATE bands SET start_freq = 1800000 WHERE id = 1 AND start_freq = 1810000;"
        "UPDATE bands SET stop_freq = 4000000 WHERE id = 2 AND stop_freq = 3800000;"
        "UPDATE bands SET stop_freq = 7300000 WHERE id = 4 AND stop_freq = 7200000;"
        "UPDATE bands SET stop_freq = 54000000 WHERE id = 12 AND stop_freq = 53900000;"
        "UPDATE bands SET start_freq = 7300000 WHERE id = 103 AND start_freq = 7200000;"
        "INSERT OR IGNORE INTO bands(id, name, start_freq, stop_freq, type) VALUES "
            "(13, '2200m', 135700, 137800, 1),"
            "(14, '630m', 472000, 479000, 1),"
            "(15, '60m', 5351500, 5366500, 1),"
            "(114, 'AM 530-1710', 530000, 1710000, 1),"
            "(112, 'Broadcast 120m', 2300000, 2495000, 0),"
            "(113, 'Broadcast 90m', 3200000, 3400000, 0);"
        "INSERT OR IGNORE INTO band_params(bands_id, name, val) VALUES "
            "(13, 'vfoa_freq', 136000), (13, 'vfoa_mode', 2),"
            "(14, 'vfoa_freq', 475000), (14, 'vfoa_mode', 2),"
            "(15, 'vfoa_freq', 5357000), (15, 'vfoa_mode', 2),"
            "(114, 'vfoa_freq', 1050000), (114, 'vfoa_mode', 6);"
        "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot update the bands: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    }
    return 0;   /* nunca impide arrancar */
}

/* CX8ANM: las bandas CB con sus 40 canales enteros (A 26.065 ... D 27.855) y la E hasta 27.995
 * (de ahí para arriba es 10 m); al entrar, AM en el canal 19. Solo si siguen como venían. */
static int _9_cb_bands() {
    int rc = sqlite3_exec(db,
        "BEGIN;"
        "UPDATE bands SET start_freq = 26065000, stop_freq = 26505000 WHERE id = 201 AND start_freq = 26055000 AND stop_freq = 26295000;"
        "UPDATE bands SET stop_freq = 26955000 WHERE id = 202 AND start_freq = 26515000 AND stop_freq = 26745000;"
        "UPDATE bands SET stop_freq = 27405000 WHERE id = 203 AND start_freq = 26965000 AND stop_freq = 27195000;"
        "UPDATE bands SET stop_freq = 27855000 WHERE id = 204 AND start_freq = 27415000 AND stop_freq = 27645000;"
        "INSERT OR IGNORE INTO bands(id, name, start_freq, stop_freq, type) VALUES (205, 'CB E', 27865000, 27995000, 1);"
        "UPDATE band_params SET val = 26285000 WHERE bands_id = 201 AND name = 'vfoa_freq' AND val = 26055000;"
        "INSERT OR IGNORE INTO band_params(bands_id, name, val) VALUES "
            "(201, 'vfoa_freq', 26285000), (201, 'vfoa_mode', 6),"
            "(202, 'vfoa_freq', 26735000), (202, 'vfoa_mode', 6),"
            "(203, 'vfoa_freq', 27185000), (203, 'vfoa_mode', 6),"
            "(204, 'vfoa_freq', 27635000), (204, 'vfoa_mode', 6),"
            "(205, 'vfoa_freq', 27935000), (205, 'vfoa_mode', 6);"
        "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot update the CB bands: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    }
    return 0;   /* nunca impide arrancar */
}

static int (*migrations[])() = {
    _0_init_migrations,
    _1_create_ftx_table,
    _2_update_atu_freq,
    _3_update_spectrum_peak_hold,
    _4_add_js8_presets,
    _5_add_ghostnet_presets,
    _6_add_wspr_presets,
    _7_add_ft2_presets,
    _8_bands_region2,
    _9_cb_bands,
};

int migrations_apply(void) {
    int rc = 0;
    int ver;
    if (db == NULL) {
        printf("Database is not opened\n");
        return 1;
    }

    rc = get_current_version(&ver);
    if (rc != 0) {
        printf("Cannot get current version\n");
        return 1;
    }
    for (size_t i = ver+1; i < SIZEOF_ARRAY(migrations); i++){
        printf("Apply migration: %i ...\n", i);
        rc = (*migrations[i])();
        if (rc != 0) {
            printf("Can't apply %i migration\n", i);
            break;
        }
        set_current_version(i);
    }
    return rc;
}


static int get_current_version(int * ver) {
    int rc;
    sqlite3_stmt *stmt;
    rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS version(id INT NOT NULL DEFAULT 0)", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Cannot create versions table: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    rc = sqlite3_prepare_v2(db, "SELECT id from version", -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        printf("Failed prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        *ver = sqlite3_column_int(stmt, 0);
    } else {
        *ver = -1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

static int set_current_version(int ver) {
    int rc;
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, "UPDATE version SET id=?", -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        printf("Failed prepare statement: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    sqlite3_bind_int(stmt, 1, ver);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}
