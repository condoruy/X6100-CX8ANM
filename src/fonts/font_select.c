/* Runtime font choice: the code keeps using &sony_N; for Roboto/DejaVu/Arial the contents of each
 * sony_N are replaced by the alt font before the UI is built. The alt fonts are ASCII only
 * (lv_font_conv --bpp 4 --no-compress --range 32-126 --format lvgl, size two steps below
 * Sony's; Arial: tools/font_gen.py, about 0.8 x Sony's size so two-line button labels fit);
 * icon glyphs come from the untouched Sony copy through the fallback.
 * Letras con tilde, ñ, ¡ ¿ (para el español): lat_<familia>_N, solo esos
 * caracteres, entre la fuente y la copia de Sony: alt -> lat -> Sony. */
#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "font_select.h"

#define SIZES(X) X(14) X(16) X(18) X(20) X(22) X(24) X(26) X(28) X(30) \
                 X(32) X(34) X(36) X(38) X(40) X(42) X(44) X(60)

#define DECL(n) extern lv_font_t sony_##n; extern const lv_font_t roboto_##n, dejavu_##n, arial_##n, rcond_##n;
SIZES(DECL)
#define DECL_LAT(n) extern const lv_font_t lat_sony_##n, lat_roboto_##n, lat_dejavu_##n, lat_arial_##n, lat_rcond_##n;
SIZES(DECL_LAT)

#define SONY(n) &sony_##n,
#define ROBOTO(n) &roboto_##n,
#define DEJAVU(n) &dejavu_##n,
#define ARIAL(n) &arial_##n,
#define RCOND(n) &rcond_##n,
static lv_font_t *const sony[] = { SIZES(SONY) };
static const lv_font_t *const roboto[] = { SIZES(ROBOTO) };
static const lv_font_t *const dejavu[] = { SIZES(DEJAVU) };
static const lv_font_t *const arial[] = { SIZES(ARIAL) };
static const lv_font_t *const rcond[] = { SIZES(RCOND) };
#define LAT_SONY(n) &lat_sony_##n,
#define LAT_ROBOTO(n) &lat_roboto_##n,
#define LAT_DEJAVU(n) &lat_dejavu_##n,
#define LAT_ARIAL(n) &lat_arial_##n,
#define LAT_RCOND(n) &lat_rcond_##n,
static const lv_font_t *const lat_sony[] = { SIZES(LAT_SONY) };
static const lv_font_t *const lat_roboto[] = { SIZES(LAT_ROBOTO) };
static const lv_font_t *const lat_dejavu[] = { SIZES(LAT_DEJAVU) };
static const lv_font_t *const lat_arial[] = { SIZES(LAT_ARIAL) };
static const lv_font_t *const lat_rcond[] = { SIZES(LAT_RCOND) };
#define N_FONTS (sizeof(sony) / sizeof(sony[0]))

static lv_font_t orig[N_FONTS];   /* Sony's, as built */
static lv_font_t lat[N_FONTS];    /* las letras con tilde de la fuente elegida */
static bool saved;

int buttons_h = 54;  /* = BUTTONS_H_MIN */

/* Again on a theme change: Sony's back first, then the choice */
void fonts_apply(uint8_t choice) {
    for (unsigned i = 0; i < N_FONTS; i++) {
        if (!saved) orig[i] = *sony[i];
        *sony[i] = orig[i];
    }
    saved = true;
    buttons_h = 54;
    if (choice != FONT_ROBOTO && choice != FONT_DEJAVU && choice != FONT_ARIAL && choice != FONT_RCOND) {
        for (unsigned i = 0; i < N_FONTS; i++) {
            lat[i]           = *lat_sony[i];
            lat[i].fallback  = NULL;
            sony[i]->fallback = &lat[i];
        }
        return;
    }
    if (choice == FONT_DEJAVU) buttons_h = 62;
    const lv_font_t *const *alt = choice == FONT_ROBOTO ? roboto : choice == FONT_DEJAVU ? dejavu :
                                   choice == FONT_ARIAL ? arial : rcond;
    const lv_font_t *const *alt_lat = choice == FONT_ROBOTO ? lat_roboto : choice == FONT_DEJAVU ? lat_dejavu :
                                       choice == FONT_ARIAL ? lat_arial : lat_rcond;

    for (unsigned i = 0; i < N_FONTS; i++) {
        *sony[i] = *alt[i];
        lat[i]          = *alt_lat[i];
        lat[i].fallback = &orig[i];
        sony[i]->fallback = &lat[i];
    }
}

/* The sony_N `steps` sizes smaller (sony_60 stays): for screens that VISION's scale makes too big */
const lv_font_t *font_down(const lv_font_t *f, int steps) {
    for (int i = 0; i + 1 < (int)N_FONTS; i++)
        if (sony[i] == f) return sony[i - steps > 0 ? i - steps : 0];
    return f;
}

/* VISION: every sony_N drawn `steps` sizes bigger (sony_20 as sony_26 with 3), up to sony_44; sony_60 stays.
 * Call after fonts_apply(), before the UI is built */
void fonts_scale(int steps) {
    static lv_font_t was[N_FONTS];

    for (unsigned i = 0; i < N_FONTS; i++) was[i] = *sony[i];
    for (unsigned i = 0; i + 1 < N_FONTS; i++) {
        unsigned to = i + steps < N_FONTS - 1 ? i + steps : N_FONTS - 2;
        *sony[i]    = was[to];
    }
}
