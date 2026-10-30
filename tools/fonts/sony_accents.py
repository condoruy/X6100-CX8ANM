#!/usr/bin/env python3
"""Letras con tilde para Sony Sketch, armadas con las letras de la misma fuente.

La fuente Sony del equipo solo trae ASCII; sin esto las tildes salían de otra
fuente (Roboto) y se notaba. Acá cada letra acentuada es la letra base de Sony
con el acento dibujado encima, del grosor del trazo de la fuente; ¡ y ¿ son !
y ? dados vuelta. Reescribe src/fonts/lat_sony_N.c (las letras de la plantilla
que arma tools/fonts/lat_gen.py, mismo orden y formato).

Uso: python3 tools/fonts/sony_accents.py      (necesita Pillow)
"""
import math, re, pathlib
from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[2]
FONTS = ROOT / 'src' / 'fonts'
SIZES = [14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 60]
# Las letras: las que trae la plantilla lat_sony_N.c (tools/fonts/lat_gen.py), en su orden
LETTERS = {'A': 'AÁÀÂÄ', 'E': 'EÉÈÊË', 'I': 'IÍÌÎÏ', 'O': 'OÓÒÔÖ', 'U': 'UÚÙÛÜ', 'N': 'NÑ', 'C': 'CÇ'}
KINDS = ['', 'acute', 'grave', 'circ', 'diaer']
COMPOSE = {}
for base, row in LETTERS.items():
    for i, ch in enumerate(row[1:], 1):
        kind = 'tilde' if base == 'N' else 'cedilla' if base == 'C' else KINDS[i]
        COMPOSE[ord(ch)] = (base, kind)
        COMPOSE[ord(ch.lower())] = (base.lower(), kind)


def targets(n):
    return [int(x, 16) for x in re.findall(r'/\* U\+([0-9A-F]{4}) ', (FONTS / f'lat_sony_{n}.c').read_text())]


class Glyph:
    def __init__(self, w, h, ox, oy, adv, px):
        self.w, self.h, self.ox, self.oy, self.adv, self.px = w, h, ox, oy, adv, px   # px[y][x] 0..15

    @property
    def top(self):          # sobre la línea de base
        return self.oy + self.h


def parse_ascii(path):
    src = path.read_text()
    bm = src[src.index('glyph_bitmap[]'):]
    bm = bm[bm.index('{') + 1:bm.index('};')]
    data = [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', re.sub(r'/\*.*?\*/', '', bm))]
    dsc = re.findall(r'\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), '
                     r'\.ofs_x = (-?\d+), \.ofs_y = (-?\d+)', src)
    glyphs = {}
    for i, ch in enumerate(range(32, 127)):
        bi, adv, w, h, ox, oy = map(int, dsc[i + 1])     # [0] reservado
        nib = []
        for b in data[bi:bi + (w * h + 1) // 2]:
            nib += [b >> 4, b & 15]
        px = [nib[y * w:(y + 1) * w] for y in range(h)]
        glyphs[chr(ch)] = Glyph(w, h, ox, oy, adv, px)
    m = re.search(r'\.line_height = (\d+)', src)
    return glyphs, int(m.group(1))


def stroke(gl):
    """Grosor del trazo: el ancho medio de la 'l' (o la 'I')"""
    g = gl['l'] if gl['l'].w else gl['I']
    runs = [sum(1 for v in row if v > 7) for row in g.px[g.h // 4: g.h * 3 // 4]]
    runs = [r for r in runs if r]
    return max(1.0, sum(runs) / len(runs)) if runs else 1.5


def mark(kind, width, t, scale=6):
    """El acento como mapa de 0..15, ancho 'width' px"""
    h = {'acute': max(2, round(t * 2.2)), 'grave': max(2, round(t * 2.2)), 'circ': max(2, round(t * 2.0)),
         'tilde': max(2, round(t * 1.6)), 'diaer': max(2, round(t * 1.3)), 'cedilla': max(2, round(t * 2.2))}[kind]
    W, H = width * scale, h * scale
    im = Image.new('L', (W, H), 0)
    d = ImageDraw.Draw(im)
    T = max(scale, t * scale)
    cx = W / 2
    if kind in ('acute', 'grave'):
        L = min(W * 0.45, H * 0.9)
        sgn = 1 if kind == 'acute' else -1
        d.line([(cx - sgn * L * 0.45, H - T / 2), (cx + sgn * L * 0.45, T / 2)], fill=255, width=int(T))
    elif kind == 'circ':
        L = min(W * 0.32, H * 1.0)
        d.line([(cx - L, H - T / 2), (cx, T / 2), (cx + L, H - T / 2)], fill=255, width=int(T), joint='curve')
    elif kind == 'cedilla':
        # gancho debajo de la letra: baja del centro y vuelve hacia la izquierda
        d.line([(cx, 0), (cx, H * 0.45), (cx + W * 0.12, H * 0.62), (cx, H - T / 2), (cx - W * 0.16, H - T / 2)],
               fill=255, width=int(T), joint='curve')
    elif kind == 'tilde':
        span = min(W * 0.75, H * 3.2)
        pts = [(cx - span / 2 + span * i / 40, H / 2 - math.sin(i / 40 * 2 * math.pi) * (H / 2 - T / 2) * 0.9)
               for i in range(41)]
        d.line(pts, fill=255, width=int(T))
    else:
        r = T * 0.62
        gap = max(W * 0.18, r * 1.3)
        for x in (cx - gap, cx + gap):
            d.ellipse([x - r, H / 2 - r, x + r, H / 2 + r], fill=255)
    im = im.resize((width, h), Image.LANCZOS)
    return [[min(15, round(im.getpixel((x, y)) / 17)) for x in range(width)] for y in range(h)]


def trim_top(g):
    """La 'i' sin punto: se sacan las filas de arriba hasta el primer hueco"""
    rows = g.px
    y = 0
    while y < len(rows) and any(rows[y]):
        y += 1
    while y < len(rows) and not any(rows[y]):
        y += 1
    if 0 < y < len(rows):
        return Glyph(g.w, g.h - y, g.ox, g.oy, g.adv, rows[y:])
    return g


def compose(base, kind, t, gap):
    # Letras angostas (la i): el acento más ancho que la letra, centrado
    w = max(base.w, math.ceil(t * 3.6))
    pl = (w - base.w) // 2
    pr = w - base.w - pl
    rows = [[0] * pl + row + [0] * pr for row in base.px]
    m = mark(kind, w, t)
    if kind == 'cedilla':      # debajo, pegada a la letra
        px = rows + m
        return Glyph(w, len(px), base.ox - pl, base.oy - len(m), base.adv, px)
    px = m + [[0] * w for _ in range(gap)] + rows
    return Glyph(w, len(px), base.ox - pl, base.oy, base.adv, px)


def rotated(g, top_at):
    px = [row[::-1] for row in g.px[::-1]]
    return Glyph(g.w, g.h, g.ox, top_at - g.h, g.adv, px)


def scaled(g, f, top_at):
    w, h = max(1, round(g.w * f)), max(1, round(g.h * f))
    im = Image.new('L', (g.w, g.h))
    im.putdata([v * 17 for row in g.px for v in row])
    im = im.resize((w, h), Image.LANCZOS)
    px = [[min(15, round(im.getpixel((x, y)) / 17)) for x in range(w)] for y in range(h)]
    return Glyph(w, h, g.ox, top_at - h, max(round(g.adv * 0.75), w * 16 + 32), px)


def underline(g, t):
    th = max(1, round(t * 0.7))
    px = g.px + [[0] * g.w] + [[12] * g.w for _ in range(th)]
    return Glyph(g.w, len(px), g.ox, g.oy - 1 - th, g.adv, px)


def build(n):
    gl, line_h = parse_ascii(FONTS / f'sony_{n}.c')
    t = stroke(gl)
    gap = max(1, round(t * 0.6))
    cap_top, x_top = gl['H'].top, gl['x'].top
    out = {}
    for cp in targets(n):
        if cp in COMPOSE:
            ch, kind = COMPOSE[cp]
            base = trim_top(gl['i']) if ch == 'i' else gl[ch]
            out[cp] = compose(base, kind, t, gap)
        elif cp == 0xA1:
            out[cp] = rotated(gl['!'], x_top)
        elif cp == 0xBF:
            out[cp] = rotated(gl['?'], x_top)
        elif cp == 0xB0:
            o = scaled(gl['o'], 0.6, cap_top)
            out[cp] = o
        elif cp == 0xBA:
            out[cp] = underline(scaled(gl['o'], 0.55, cap_top), t)
        elif cp == 0xAA:
            out[cp] = underline(scaled(gl['a'], 0.55, cap_top), t)
    return out


def emit(n, glyphs):
    path = FONTS / f'lat_sony_{n}.c'
    src = path.read_text()
    data, dsc, idx = [], [], 0
    for cp in targets(n):
        g = glyphs[cp]
        nib = [v for row in g.px for v in row]
        if len(nib) % 2:
            nib.append(0)
        b = [(nib[i] << 4) | nib[i + 1] for i in range(0, len(nib), 2)]
        lines = [', '.join(f'0x{x:x}' for x in b[i:i + 12]) for i in range(0, len(b), 12)]
        data.append(f'    /* U+{cp:04X} "{chr(cp)}" */\n' + ''.join(f'    {l},\n' for l in lines))
        dsc.append(f'    {{.bitmap_index = {idx}, .adv_w = {g.adv}, .box_w = {g.w}, .box_h = {g.h}, '
                   f'.ofs_x = {g.ox}, .ofs_y = {g.oy}}}')
        idx += len(b)
    bm = 'glyph_bitmap[] = {\n' + ''.join(data).rstrip(',\n') + '\n};'
    src = re.sub(r'glyph_bitmap\[\] = \{.*?\n\};', lambda m: bm, src, count=1, flags=re.S)
    dsc_txt = ('glyph_dsc[] = {\n    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} '
               '/* id = 0 reserved */,\n' + ',\n'.join(dsc) + '\n};')
    src = re.sub(r'glyph_dsc\[\] = \{.*?\n\};', lambda m: dsc_txt, src, count=1, flags=re.S)
    src = re.sub(r' \* Opts:.*', ' * Opts: tools/fonts/sony_accents.py (letras de Sony con acento dibujado)', src, count=1)
    path.write_text(src)


for n in SIZES:
    emit(n, build(n))
print('lat_sony_*.c: listo')
