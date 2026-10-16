/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Idiomas de la interfaz: ver i18n.h. Las tablas están en i18n_<idioma>.inc,
 *  una línea por texto: { "texto original en inglés", "traducción" },
 */
#include "i18n.h"

#include <string.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>

namespace {

struct entry_t {
    const char *en;
    const char *tr;
};

const entry_t table_es[] = {
#include "i18n_es.inc"
};

typedef std::unordered_map<std::string_view, const char *> dict_t;

volatile uint8_t cur_lang = LANG_EN;
dict_t           dict_es;
std::once_flag   dict_once;

/* Resultados armados línea por línea: un anillo por hilo. LVGL copia el texto
 * al momento (lv_label_set_text y compañía), así que alcanza con que el
 * puntero viva un rato. */
constexpr int    RING_N   = 24;
constexpr size_t RING_LEN = 2048;
thread_local char ring[RING_N][RING_LEN];
thread_local int  ring_pos;

/* Para los *_static de LVGL, que guardan el puntero: copias permanentes */
std::mutex                                   perm_mutex;
std::unordered_map<std::string, std::string> perm;

void build_dicts() {
    for (const entry_t &e : table_es) {
        dict_es.emplace(std::string_view(e.en), e.tr);
    }
}

const dict_t *cur_dict() {
    switch (cur_lang) {
    case LANG_ES:
        std::call_once(dict_once, build_dicts);
        return &dict_es;
    default:
        return nullptr;
    }
}

const char *find(const dict_t *d, std::string_view key) {
    auto it = d->find(key);
    return it == d->end() ? nullptr : it->second;
}

/* Una línea: se respetan los espacios de los costados (" Off ") y, si no está
 * con dos puntos al final, se busca sin ellos ("Volume:" -> "Volume" + ":") */
bool line_tr(const dict_t *d, std::string_view line, std::string &out) {
    size_t a = 0, b = line.size();
    while (a < b && (line[a] == ' ' || line[a] == '\t')) a++;
    while (b > a && (line[b - 1] == ' ' || line[b - 1] == '\t')) b--;
    if (a == b) return false;

    std::string_view core  = line.substr(a, b - a);
    const char      *t     = find(d, core);
    bool             colon = false;

    if (!t && core.size() > 1 && core.back() == ':') {
        t     = find(d, core.substr(0, core.size() - 1));
        colon = t != nullptr;
    }
    if (!t) return false;
    out.append(line.substr(0, a));
    out.append(t);
    if (colon) out.push_back(':');
    out.append(line.substr(b));
    return true;
}

/* nullptr: sin cambios */
bool compose(const dict_t *d, const char *s, std::string &out) {
    std::string_view all(s);
    bool             changed = false;
    size_t           start   = 0;

    while (true) {
        size_t           nl   = all.find('\n', start);
        std::string_view line = all.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);

        if (!line_tr(d, line, out)) out.append(line);
        else changed = true;
        if (nl == std::string_view::npos) break;
        out.push_back('\n');
        start = nl + 1;
    }
    return changed;
}

} // namespace

extern "C" const char *tr(const char *s) {
    if (!s || !*s) return s;

    const dict_t *d = cur_dict();
    if (!d) return s;

    const char *t = find(d, std::string_view(s));
    if (t) return t;

    std::string out;
    if (!compose(d, s, out) || out.size() >= RING_LEN) return s;

    char *buf = ring[ring_pos];
    ring_pos  = (ring_pos + 1) % RING_N;
    memcpy(buf, out.c_str(), out.size() + 1);
    return buf;
}

extern "C" const char *tr_static(const char *s) {
    if (!s || !*s) return s;

    const dict_t *d = cur_dict();
    if (!d) return s;

    const char *t = find(d, std::string_view(s));
    if (t) return t;

    std::lock_guard<std::mutex> lock(perm_mutex);
    auto it = perm.find(s);
    if (it != perm.end()) return it->second.c_str();

    std::string out;
    if (!compose(d, s, out)) return s;
    return perm.emplace(s, std::move(out)).first->second.c_str();
}

extern "C" void i18n_set_lang(uint8_t lang) {
    cur_lang = lang < (uint8_t)LANG_COUNT ? lang : (uint8_t)LANG_EN;
}

extern "C" uint8_t i18n_get_lang(void) {
    return cur_lang;
}
