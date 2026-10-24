"""Vista previa aproximada de la pantalla principal del X6100 con un tema (800 x 480).
Dibuja la disposición normal (barra de arriba, frecuencia, escala, cascada y botones) con los colores
del tema; las letras son parecidas a las de la radio, no las mismas."""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFont

FONTS = {
    'arial': '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
    'arial_b': '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf',
    'dejavu': '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
    'dejavu_b': '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
}


def font(t, size, bold=False):
    fam = 'arial' if t.get('font') == 'arial' else 'dejavu'
    path = FONTS[fam + ('_b' if bold else '')]
    if not os.path.exists(path):
        path = FONTS[fam]
    return ImageFont.truetype(path, size)


def rgb(v):
    return (v >> 16 & 255, v >> 8 & 255, v & 255)


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def ramp(stops, n=256):
    s = [rgb(int(x.lstrip('#'), 16)) for x in stops]
    out = []
    for i in range(n):
        f = i / (n - 1) * (len(s) - 1)
        k = min(int(f), len(s) - 2)
        out.append(lerp(s[k], s[k + 1], f - k))
    return out


def box(im, xy, c, t, fill, grad, border, radius=None):
    x0, y0, x1, y1 = (int(v) for v in xy)
    r = t.get('radius', 4) if radius is None else radius
    w, h = x1 - x0, y1 - y0
    layer = Image.new('RGB', (w, h), rgb(c[fill]))
    if grad and c.get(grad) is not None:
        d = ImageDraw.Draw(layer)
        a, b = rgb(c[fill]), rgb(c[grad])
        for y in range(h):
            d.line([(0, y), (w, y)], fill=lerp(a, b, y / max(h - 1, 1)))
    mask = Image.new('L', (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], r, fill=255)
    im.paste(layer, (x0, y0), mask)
    bw = t.get('border', 1)
    if bw and border:
        ImageDraw.Draw(im).rounded_rectangle([x0, y0, x1 - 1, y1 - 1], r, outline=rgb(c[border]), width=bw)


def text(d, xy, s, f, col, anchor='la'):
    d.text(xy, s, font=f, fill=rgb(col) if isinstance(col, int) else col, anchor=anchor)


def render(name, j, c):
    t = dict(j)
    im = Image.new('RGB', (800, 480), rgb(c['bg']))
    d = ImageDraw.Draw(im)
    bg = t.get('box_gradient')

    # ---- barra de arriba ----
    for xy in [(2, 2, 362, 74), (366, 2, 626, 74), (630, 2, 798, 74)]:
        box(im, xy, c, t, 'bar', 'bar_grad' if bg else None, 'bar_border')
    f18, f18b = font(t, 17), font(t, 17, True)
    items = [('VFO-A', 'text'), ('USB-D', 'mode_dig'), ('AUTO', 'text'), ('PRE', 'on'), ('ATT', 'pill_off_text'), ('ATU', 'pill_off_text')]
    x = 14
    for s, role in items:
        w = d.textlength(s, font=f18)
        if role == 'on':
            box(im, (x - 6, 9, int(x + w + 6), 31), c, t, 'sel_bg', None, 'sel_border' if t.get('pill_border') else None, 4)
            text(d, (x, 11), s, f18, c['sel_text'])
        else:
            if t.get('pill_border'):
                d.rounded_rectangle([x - 5, 9, x + w + 5, 31], 4, outline=rgb(c['pill_border']), width=t['pill_border'])
            text(d, (x, 11), s, f18, c[role])
        x += w + 20
    icons = [('ok', 'wifi'), ('info', 'bt'), ('info', 'audio'), ('accent', 'kbd'), ('ok', 'gps'), ('pill_off_text', 'DX')]
    for i, (role, kind) in enumerate(icons):
        cx = 36 + i * 58
        col = rgb(c[role])
        if kind == 'wifi':
            for k, rr in enumerate([14, 9, 4]):
                d.arc([cx - rr, 54 - rr, cx + rr, 54 + rr], 225, 315, fill=col, width=3)
            d.ellipse([cx - 2, 56, cx + 2, 60], fill=col)
        elif kind == 'DX':
            text(d, (cx, 53), 'DX', f18b, col, 'mm')
        elif kind == 'kbd':
            d.rounded_rectangle([cx - 12, 45, cx + 12, 62], 3, outline=col, width=2)
            for k in range(4):
                d.rectangle([cx - 8 + k * 5, 50, cx - 6 + k * 5, 52], fill=col)
        elif kind == 'gps':
            d.polygon([(cx - 10, 52), (cx + 11, 43), (cx + 2, 64), (cx, 54)], fill=col)
        elif kind == 'bt':
            d.line([(cx - 6, 47), (cx + 6, 59), (cx, 64), (cx, 42), (cx + 6, 47), (cx - 6, 59)], fill=col, width=2)
        else:
            d.line([(cx - 6, 61), (cx - 6, 45), (cx + 8, 42), (cx + 8, 58)], fill=col, width=3)
            d.ellipse([cx - 11, 57, cx - 3, 64], fill=col)
            d.ellipse([cx + 3, 54, cx + 11, 61], fill=col)

    # S-meter
    nb = 30
    for i in range(nb):
        x0 = 378 + i * 8
        lit = i < 21
        if t.get('meter_gradient'):
            col = lerp(rgb(c['meter_weak']), rgb(c['meter_high']), i / (nb - 1))
        elif i < 17:
            col = rgb(c['meter_bar'])
        else:
            col = lerp(rgb(c['meter_low']), rgb(c['meter_high']), (i - 17) / (nb - 18))
        if not lit:
            col = rgb(c['off'])
        d.rectangle([x0, 10, x0 + 4, 40], fill=col)
    fs = font(t, 18, True)
    for i, s in enumerate(['S1', '3', '5', '7', '9']):
        text(d, (384 + i * 32, 14), s, fs, c['text'])
    text(d, (546, 14), '+20', fs, c['meter_over']); text(d, (586, 14), '+40', fs, c['meter_over'])
    text(d, (496, 60), '-82.2', font(t, 16), c['text_value'], 'mm')
    # reloj
    text(d, (714, 22), '18:45', font(t, 38, True), c['text'], 'mm')
    text(d, (768, 28), ':06', font(t, 18), c['text'], 'mm')
    text(d, (714, 58), '7.8v   12.0v', font(t, 18), c['text'], 'mm')

    # ---- franja de la frecuencia ----
    if t.get('rule'):
        d.rectangle([0, 75, 800, 77], fill=rgb(c['accent']))
    box(im, (2, 78, 798, 126), c, t, 'strip', 'strip_grad' if bg else None, 'strip_border')
    text(d, (18, 102), '40m', font(t, 22), c['text_head'], 'lm')
    fbig = font(t, 34, True)
    text(d, (440, 102), '7.127.000', fbig, c['text_value'], 'rm')
    fm = font(t, 24, True)
    if t.get('mode_box'):
        w = d.textlength('USB-D', font=fm)
        box(im, (452, 88, int(462 + w + 10), 117), c, t, 'mode_bg', None, 'mode_border', 4)
        text(d, (462, 102), 'USB-D', fm, c['mode_text'], 'lm')
    else:
        text(d, (452, 102), 'USB-D', fm, c['mode_text'], 'lm')
    d.ellipse([696, 96, 708, 108], fill=rgb(c['ok']))
    text(d, (716, 102), 'GF15', font(t, 22), c['text'], 'lm')

    # ---- escala ----
    fsc = font(t, 18, True)
    for i, s in enumerate(['7.077', '7.100', '7.120', '7.140', '7.160', '7.177']):
        x = [8, 184, 344, 504, 664, 792][i]
        text(d, (x, 142), s, fsc, c['text'], 'lm' if i == 0 else 'rm' if i == 5 else 'mm')

    # ---- cascada ----
    pal = ramp(j['waterfall'])
    y0, y1 = 158, 425
    rnd = random.Random(7)
    sigs = [(118, 3, .9), (212, 2, .7), (330, 4, 1.0), (392, 2, .6), (470, 18, .75), (585, 2, .5), (640, 3, .85), (745, 2, .55)]
    wf = Image.new('RGB', (800, y1 - y0))
    px = wf.load()
    for y in range(y1 - y0):
        for x in range(800):
            v = .06 + rnd.random() * .14
            for sx, sw, a in sigs:
                if (y // 9 + sx) % 7 == 0 and sw < 5:
                    continue        # CW: puntos y rayas
                dd = abs(x - sx)
                if dd < sw * 3:
                    v = max(v, a * math.exp(-(dd / sw) ** 2) + rnd.random() * .05)
            px[x, y] = pal[min(255, int(v * 255))]
    im.paste(wf, (0, y0))
    # plan de banda y spots
    d.rectangle([0, y0, 800, y0 + 20], fill=rgb(c['bp_ssb']))
    text(d, (400, y0 + 10), 'ALL MODES / SSB', font(t, 14), c['bp_text'], 'mm')
    fsp = font(t, 18, True)
    for x, call in [(8, 'LU1ABC'), (116, 'PY2XYZ'), (212, 'CX2AQ'), (330, 'LU5FF'), (585, 'CE3RR'), (640, 'CX8ANM')]:
        d.rectangle([x, y0, x + 3, y0 + 30], fill=rgb(c['spot']))
        text(d, (x + 2, y0 + 34), call, fsp, c['spot'])
    # filtro y VFO
    ov = Image.new('RGBA', (800, 480), (0, 0, 0, 0))
    ImageDraw.Draw(ov).rectangle([470, y0, 520, y1], fill=rgb(c['vfo_line']) + (40,))
    im.paste(Image.alpha_composite(im.convert('RGBA'), ov).convert('RGB'))
    d = ImageDraw.Draw(im)
    d.line([(470, y0), (470, y1)], fill=rgb(c['vfo_line']), width=2)

    # ---- botones ----
    gap = t.get('btn_gap', 0)
    labels = [('VOL 1:2', None), ('Volume', '30'), ('Squelch', '0'), ('RF gain', '63'), ('TX power', '5.0 W')]
    for i, (a, b) in enumerate(labels):
        x0 = 2 + i * 160 + gap
        xy = (x0, 428 + gap, x0 + 156 - 2 * gap, 478 - gap)
        if i == 0:
            box(im, xy, c, t, 'btn_page', None, 'btn_border', t.get('radius_btn', 4))
            text(d, ((xy[0] + xy[2]) // 2, 453), a, font(t, 22), c['btn_page_text'], 'mm')
            continue
        active = i == 1
        if active:
            box(im, xy, c, t, 'btn_active', 'btn_active_grad' if t.get('btn_gradient') else None, 'btn_active_border', t.get('radius_btn', 4))
        else:
            box(im, xy, c, t, 'btn', 'btn_grad' if t.get('btn_gradient') else None, 'btn_border', t.get('radius_btn', 4))
        tc = c['btn_active_text'] if active else c['btn_text']
        head = c['btn_active_text'] if active else c['text_head']
        text(d, ((xy[0] + xy[2]) // 2, 441), a, font(t, 18, True), head, 'mm')
        text(d, ((xy[0] + xy[2]) // 2, 464), b, font(t, 24, True), tc, 'mm')
        mark = c['btn_mark_active'] if active else c['btn_mark']
        d.rectangle([xy[0] + 3, 436, xy[0] + 6, 470], fill=rgb(mark))
    if t.get('rule'):
        d.rectangle([162 + gap, 426, 316 - gap, 428], fill=rgb(c['accent']))
    return im


def render_all(built, outdir, sheet_name='todos'):
    os.makedirs(outdir, exist_ok=True)
    thumbs = []
    for name, (j, c) in built.items():
        im = render(name, j, c)
        fn = os.path.join(outdir, name.lower().replace(' ', '_') + '.png')
        im.save(fn)
        thumbs.append((name, im))
        print('vista', fn)
    # hoja con todos
    cols, tw, th, pad = 2, 560, 336, 14
    rows = (len(thumbs) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * (tw + pad) + pad, rows * (th + 44 + pad) + pad), (24, 26, 30))
    d = ImageDraw.Draw(sheet)
    f = ImageFont.truetype(FONTS['dejavu_b'], 24)
    for i, (name, im) in enumerate(thumbs):
        x = pad + (i % cols) * (tw + pad)
        y = pad + (i // cols) * (th + 44 + pad)
        d.text((x, y + 6), name, font=f, fill=(240, 240, 240))
        sheet.paste(im.resize((tw, th), Image.LANCZOS), (x, y + 40))
    fn = os.path.join(outdir, sheet_name + '.png')
    sheet.save(fn)
    print('hoja', fn)
