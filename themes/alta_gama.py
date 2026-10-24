#!/usr/bin/env python3
"""Temas "alta gama" para el X6100 (CX8ANM).

Cada tema se describe con una paleta corta (fondo, superficies, texto, acento, botón activo, cascada...)
y de ahí salen los ~140 colores que usa la interfaz (src/theme.h). Escribe themes/alta-gama/<nombre>.json,
que la imagen copia a /mnt/themes al arrancar.

    python3 themes/alta_gama.py            # escribe los JSON
    python3 themes/alta_gama.py --preview  # además, vistas previas en docs/screenshots/themes_alta_gama/
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'themes', 'alta-gama')


def base_colors(base='Classic'):
    """Los colores del tema base (columna de THEME_COLORS): lo que el tema no cambia.
    Classic para los temas oscuros, Outdoor para los claros"""
    col = {'CX8ANM': 2, 'Legacy': 3, 'Outdoor': 4, 'Classic': 5}[base]
    src = open(os.path.join(ROOT, 'src', 'theme.h')).read()
    out = {}
    for m in re.finditer(r'X\((\w+),\s*0x(\w+),\s*0x(\w+),\s*0x(\w+),\s*0x(\w+)\)', src):
        out[m.group(1)] = int(m.group(col), 16)
    return out


def h(c):
    return int(c.lstrip('#'), 16) if isinstance(c, str) else c


def mix(a, b, t):
    """t = 0: a; t = 1: b"""
    a, b = h(a), h(b)
    r = [round(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t) for s in (16, 8, 0)]
    return r[0] << 16 | r[1] << 8 | r[2]


def lum(c):
    c = h(c)
    return 0.299 * (c >> 16 & 255) + 0.587 * (c >> 8 & 255) + 0.114 * (c & 255)


def tint(c, hue):
    """La luminosidad de c con el tono de hue (noche roja: todo en rojo)"""
    l = lum(c) / 255
    hue = h(hue)
    hl = max(lum(hue) / 255, 1e-3)
    r = [min(255, round(((hue >> s) & 255) * l / hl)) for s in (16, 8, 0)]
    return r[0] << 16 | r[1] << 8 | r[2]


def roles(p, base='Classic'):
    """Paleta corta -> todos los roles"""
    bg, text = h(p['bg']), h(p['text'])
    c = dict(base_colors(base))
    surf, surf2, border = h(p['surface']), h(p['surface2']), h(p['border'])
    acc, act, act_text = h(p['accent']), h(p['active']), h(p['active_text'])
    c.update({
        'bg': bg,
        'bar': h(p.get('bar', surf)), 'bar_grad': h(p.get('bar_grad', surf2)), 'bar_border': border,
        'strip': h(p.get('strip', surf)), 'strip_grad': h(p.get('strip_grad', surf2)), 'strip_border': border,
        'dialog': surf, 'dialog_grad': surf2, 'dialog_border': border,
        'btn': h(p['btn']), 'btn_grad': h(p.get('btn_grad', p['btn'])), 'btn_border': h(p.get('btn_border', border)),
        'btn_text': h(p.get('btn_text', text)),
        'btn_active': act, 'btn_active_grad': h(p.get('active_grad', act)), 'btn_active_border': h(p.get('active_border', act)),
        'btn_active_text': act_text,
        'btn_page': h(p.get('page', p['btn'])), 'btn_page_text': h(p.get('page_text', p.get('head', acc))),
        'btn_off_text': mix(text, bg, .65), 'btn_off_border': mix(border, bg, .5),
        'btn_mark': h(p.get('mark', acc)), 'btn_mark_assigned': mix(p.get('mark', acc), bg, .35),
        'btn_mark_active': h(p.get('mark_active', p.get('tx', 0xff3030))),
        'card': border, 'focus': act, 'focus_text': act_text, 'edited': acc,
        'panel': bg, 'panel_border': border, 'list': mix(surf2, bg, .5), 'row': mix(surf, text, .08),
        'popup': surf2, 'popup_border': border, 'side': mix(surf2, bg, .3), 'chip': mix(surf, text, .1),
        'sel_bg': h(p.get('sel_bg', mix(act, bg, .55))), 'sel_text': h(p.get('sel_text', text)),
        'sel_border': h(p.get('sel_border', act)), 'focus_bg': h(p.get('sel_bg', mix(act, bg, .4))),
        'near_bg': mix(acc, bg, .8), 'vfo': h(p.get('vfo', acc)), 'vfo_bg': mix(p.get('vfo', acc), bg, .82),
        'text': text, 'text_value': h(p.get('value', text)), 'text_dim': h(p['dim']),
        'text_soft': mix(text, bg, .25), 'text_mute': mix(text, bg, .48), 'text_faint': mix(text, bg, .62),
        'text_head': h(p.get('head', acc)), 'line': h(p.get('line', p['dim'])), 'action': acc,
        'accent': acc, 'accent_soft': mix(acc, text, .4),
        'veil': mix(bg, surf, .5), 'track': mix(surf, text, .15), 'frame': mix(surf2, bg, .4),
        'off': mix(text, bg, .7), 'icon_off': mix(surf, bg, .3), 'switch_off': mix(text, bg, .15),
        'lock': acc, 'control_border': mix(text, bg, .45), 'toggle_on': act, 'toggle_focus': mix(act, text, .4),
        'vfo_line': h(p.get('vfo_line', 0xff2020)), 'spot': h(p.get('spot', p.get('head', acc))),
        'mode_text': h(p.get('mode', acc)), 'mode_dig': h(p.get('mode_dig', acc)),
        'mode_bg': h(p.get('mode_bg', act)), 'mode_border': h(p.get('mode_border', p.get('mode_bg', act))),
        'pill_off_text': h(p.get('pill_off', mix(text, bg, .35))), 'pill_border': border,
        'meter_weak': h(p.get('meter_weak', p['meter_bar'])), 'meter_bar': h(p['meter_bar']),
        'meter_over': h(p.get('meter_over', p['meter_high'])),
        'meter_low': h(p['meter_low']), 'meter_high': h(p['meter_high']),
        'title_text': h(p.get('head', acc)),
    })
    if 'bp' in p:   # plan de banda: CW, digitales, SSB, balizas, texto
        for k, v in zip(('bp_cw', 'bp_dig', 'bp_ssb', 'bp_bcn', 'bp_text'), p['bp']):
            c[k] = h(v)
    for k in ('ok', 'warn', 'err', 'info', 'tx', 'digi'):
        if k in p:
            c[k] = h(p[k])
    c['ok_bg'] = mix(c['ok'], bg, .78)
    c['ok_bar'] = mix(c['ok'], bg, .15)
    c['warn_bg'] = mix(c['warn'], bg, .78)
    c['err_bg'] = mix(c['err'], bg, .75)
    c['err_strong'] = c['err']
    c['tx_bg'] = mix(c['tx'], bg, .25)
    c['tx_box'] = mix(c['tx'], bg, .6)
    if 'hue' in p:   # todo en un solo tono (Noche Roja)
        for k in c:
            c[k] = tint(c[k], p['hue'])
    return c


# ---------- los temas ----------
# "inspirado en": la estética de la pantalla de cada familia de equipos, no su logo ni su marca
BASE_THEMES = {
    # Negro con teclas grises en degradé, azul para lo activo, recuadro del modo, cascada azul-amarillo-rojo
    'Pro Azul': dict(
        base='Classic', font='arial', radius=3, radius_btn=4, radius_popup=4, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#000000', surface='#16263e', surface2='#060b14', border='#3a5a88',
                     strip='#0b1626', strip_grad='#000000', btn='#5a6170', btn_grad='#1e2129', btn_border='#7d8696',
                     text='#ffffff', value='#ffffff', dim='#9aa6b4', head='#6cc6ff', accent='#ffa628',
                     active='#2f72d6', active_grad='#123e86', active_border='#9fd2ff', active_text='#ffffff',
                     mode='#ffffff', mode_bg='#1f5fbf', mode_border='#9fd2ff',
                     meter_bar='#e6e6e6', meter_low='#40e040', meter_high='#ff3030', meter_over='#ff4040',
                     mark='#ffa628', vfo='#40c8ff', spot='#40e8ff'),
        waterfall=['#000000', '#00003c', '#00008c', '#0040ff', '#00c0ff', '#00ff80', '#ffff00', '#ff8000', '#ff0000']),
    # Ámbar sobre negro, como los visores de cristal líquido ámbar: todo cálido, cascada en fuego
    'Pro Ambar': dict(
        base='Classic', font='dejavu', radius=2, radius_btn=3, radius_popup=3, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1, meter_gradient=True,
        palette=dict(bg='#050302', surface='#24180a', surface2='#0b0703', border='#7a5418',
                     strip='#1a1106', strip_grad='#050302', btn='#3a2810', btn_grad='#140d05', btn_border='#8a6020',
                     text='#ffb84a', value='#ffd27a', dim='#b07e30', head='#ffcf70', accent='#ffd000',
                     active='#ffb000', active_grad='#d88a00', active_border='#ffe0a0', active_text='#1a0f00',
                     mode='#1a0f00', mode_bg='#ffb000', mode_border='#ffe0a0',
                     meter_bar='#ffb000', meter_weak='#6a4a10', meter_low='#ffb000', meter_high='#ff3000',
                     sel_text='#ffe0a0', vfo='#ffd000', spot='#ffe9a0', ok='#d8e040', info='#ffd27a',
                     digi='#ff9a50', warn='#ff8a00', mode_dig='#ffd000',
                     bp=('#3a2404', '#7a5008', '#5a3a08', '#6a1800', '#ffd27a')),
        waterfall=['#050302', '#1e0c00', '#4a1e00', '#8a3c00', '#d06a00', '#ffa000', '#ffd060', '#fff0c0']),
    # Verde fósforo (visores fluorescentes de los equipos clásicos): verde sobre negro, sin degradés
    'Pro Verde': dict(
        base='Classic', font='roboto', radius=0, radius_btn=0, radius_popup=0, border=1,
        btn_gradient=False, box_gradient=False, focus_solid=True, mode_box=False, pill_border=0, meter_gradient=True,
        palette=dict(bg='#000000', surface='#06150c', surface2='#020a05', border='#1f6b40',
                     btn='#071a0e', btn_border='#2a8a52', text='#5dff9a', value='#a8ffcc', dim='#2fa864',
                     head='#9dffc4', accent='#c8ff60', active='#2bd46a', active_text='#001a0a',
                     mode='#c8ff60', meter_bar='#2bd46a', meter_weak='#0f4a26', meter_low='#2bd46a',
                     meter_high='#e8ff70', meter_over='#e8ff70', vfo='#c8ff60', spot='#c8ff60',
                     ok='#5dff9a', info='#7dffe0', warn='#e8ff70', digi='#7dffe0',
                     bp=('#0a3a20', '#2a6a10', '#0f5a2c', '#5a5a00', '#c8ffd8')),
        waterfall=['#000000', '#001a0c', '#003c1c', '#006b34', '#00a850', '#3ce070', '#a0ff90', '#f0ffd0']),
    # Grafito de los equipos SDR modernos: gris oscuro plano, amarillo para lo activo, línea de acento
    'Pro Grafito': dict(
        base='Classic', font='roboto', radius=6, radius_btn=6, radius_popup=6, border=1, btn_gap=2,
        btn_gradient=False, box_gradient=False, focus_solid=True, mode_box=False, pill_border=0, rule=True,
        palette=dict(bg='#15171b', surface='#24272d', surface2='#1a1c20', border='#3a3f47',
                     strip='#101215', strip_grad='#101215', btn='#2b2f36', btn_border='#3a3f47',
                     text='#f2f4f7', value='#ffffff', dim='#9aa1ab', head='#4fd1ff', accent='#ffd400',
                     active='#ffd400', active_border='#ffd400', active_text='#141414', page_text='#ffd400',
                     mode='#ffd400', meter_bar='#4fd1ff', meter_low='#4fd1ff', meter_high='#ff4060',
                     vfo='#4fd1ff', spot='#4fd1ff', mark='#4fd1ff', sel_bg='#3a3416', sel_text='#ffd400'),
        waterfall=['#000010', '#000a50', '#0050c8', '#00c8ff', '#00ff90', '#ffe000', '#ff4000', '#ff0060']),
    # Azul profundo con números blancos, como las pantallas azules de los equipos de base
    'Pro Marino': dict(
        base='Classic', font='arial', radius=4, radius_btn=5, radius_popup=5, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#00122b', surface='#0a2b5c', surface2='#04183a', border='#4b7cc4',
                     strip='#001f47', strip_grad='#000f26', btn='#23466f', btn_grad='#0c2240', btn_border='#5c8ad0',
                     text='#ffffff', value='#e8f2ff', dim='#8fb0d8', head='#8fd0ff', accent='#ffd040',
                     active='#d8ecff', active_grad='#9cc8f4', active_border='#ffffff', active_text='#002050',
                     mode='#002050', mode_bg='#ffd040', mode_border='#ffe890',
                     meter_bar='#ffffff', meter_low='#ffd040', meter_high='#ff5040',
                     vfo='#ffd040', spot='#a8e0ff', mark='#ffd040', sel_bg='#1f5fbf', mode_dig='#ffd040'),
        waterfall=['#000a1e', '#00285a', '#0050a8', '#0090e8', '#40c8ff', '#c8f0ff', '#ffffff']),
    # Noche: todo en rojo sobre negro para no perder la visión nocturna (con el brillo bajo)
    'Noche Roja': dict(
        base='Classic', font='roboto', radius=3, radius_btn=4, radius_popup=4, border=1,
        btn_gradient=False, box_gradient=False, focus_solid=True, mode_box=False, pill_border=0,
        meter_gradient=True, mono_cluster=True,
        palette=dict(bg='#000000', surface='#1a0403', surface2='#0a0101', border='#5a120c',
                     btn='#160302', btn_border='#6a1810', text='#ff4a3a', value='#ff6a58', dim='#a02418',
                     head='#ff6a58', accent='#ff8a70', active='#d01a0c', active_text='#000000',
                     mode='#ff8a70', meter_bar='#c02010', meter_low='#c02010', meter_high='#ff6050',
                     bp=('#2a0200', '#4a0800', '#3a0400', '#600a00', '#ff6a58'), hue='#ff2a14'),
        waterfall=['#000000', '#140000', '#380000', '#6a0000', '#a00800', '#d81800', '#ff5030']),
    # Icom IC-7300: pantalla táctil en negro azabache, teclas biseladas, azul Icom en modos y cascada multinivel
    'Icom IC-7300': dict(
        base='Classic', font='arial', radius=3, radius_btn=4, radius_popup=4, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#040608', surface='#121824', surface2='#070b10', border='#2b394d',
                     strip='#0c121c', strip_grad='#020406', btn='#2b3545', btn_grad='#141a24', btn_border='#44546d',
                     text='#f2f5f9', value='#ffffff', dim='#8a98ab', head='#40b0ff', accent='#ffaa1e',
                     active='#1858be', active_grad='#0e387c', active_border='#70b5ff', active_text='#ffffff',
                     mode='#ffffff', mode_bg='#124ba0', mode_border='#60a8ff',
                     meter_bar='#e8ecf2', meter_low='#00d050', meter_high='#ff2626', meter_over='#ff3838',
                     vfo='#38b4ff', spot='#50c4ff', mark='#ffaa1e', sel_bg='#15427d', sel_text='#80ccff'),
        waterfall=['#000000', '#000030', '#001880', '#0040e0', '#00b0f0', '#00d860', '#ffe000', '#ff6800', '#ee0000']),
    # Yaesu FTDX: gris metálico satinado, teclas ámbar incandescente y cascada estilo 3DSS
    'Yaesu FTDX': dict(
        base='Classic', font='roboto', radius=2, radius_btn=3, radius_popup=3, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#08090b', surface='#191c22', surface2='#0b0d10', border='#3e4450',
                     strip='#121418', strip_grad='#060708', btn='#282c35', btn_grad='#14161b', btn_border='#485060',
                     text='#f0f3f6', value='#ffffff', dim='#949ca8', head='#00c0f0', accent='#ff9600',
                     active='#ff8800', active_grad='#b85800', active_border='#ffbe60', active_text='#000000',
                     mode='#000000', mode_bg='#ff9600', mode_border='#ffbe60',
                     meter_bar='#00c8f0', meter_weak='#004860', meter_low='#00c8f0', meter_high='#ff3010', meter_over='#ff3010',
                     vfo='#00c8f0', spot='#4de0ff', mark='#ff9600', sel_bg='#402400', sel_text='#ffb440'),
        waterfall=['#000004', '#08082c', '#14208c', '#0088d8', '#00d8f0', '#c80088', '#ff3000', '#ffb000']),
    # Kenwood TS-890: grafito profundo, verde esmeralda y acentos en oro suave
    'Kenwood TS-890': dict(
        base='Classic', font='dejavu', radius=3, radius_btn=4, radius_popup=4, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#060709', surface='#16191f', surface2='#0a0c0f', border='#343b47',
                     strip='#0e1014', strip_grad='#040507', btn='#252a33', btn_grad='#12151a', btn_border='#404856',
                     text='#eaf0f6', value='#f8fbff', dim='#8a94a2', head='#44d472', accent='#f2a828',
                     active='#2ea854', active_grad='#165c2c', active_border='#76f09e', active_text='#ffffff',
                     mode='#ffffff', mode_bg='#184e2a', mode_border='#5ce088',
                     meter_bar='#44d472', meter_weak='#124824', meter_low='#44d472', meter_high='#f03828', meter_over='#f03828',
                     vfo='#f2a828', spot='#72e89a', mark='#f2a828', sel_bg='#10361c', sel_text='#64e68e'),
        waterfall=['#000000', '#000c2e', '#002888', '#0078d0', '#00c8e0', '#24c060', '#f4b820', '#d82020']),
    # Elecraft K4: estética táctica modular, negro anodizado, acentos amarillo oro y máxima legibilidad
    'Elecraft K4': dict(
        base='Classic', font='roboto', radius=1, radius_btn=2, radius_popup=2, border=1,
        btn_gradient=False, box_gradient=False, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#0d0f12', surface='#1b1f26', surface2='#111418', border='#323a48',
                     strip='#14171d', strip_grad='#14171d', btn='#222832', btn_border='#384252',
                     text='#f4f6fa', value='#ffffff', dim='#949eae', head='#ffc820', accent='#ffc820',
                     active='#ffc820', active_border='#ffe070', active_text='#12151a',
                     mode='#12151a', mode_bg='#ffc820', mode_border='#ffe070',
                     meter_bar='#48c8ff', meter_low='#48c8ff', meter_high='#ff3848', meter_over='#ff3848',
                     vfo='#ffc820', spot='#ffdb58', mark='#ffc820', sel_bg='#3a2f0a', sel_text='#ffc820'),
        waterfall=['#000000', '#060c24', '#0a2468', '#0070c8', '#00c0d8', '#20c040', '#ffe020', '#ff3010', '#ffffff']),
}


MORE_RIGS = {
    # Icom, pantalla "tipo B": fondo azul en degradé, teclas grises, letra blanca
    'Icom IC-7300 B': dict(
        base='Classic', font='arial', radius=3, radius_btn=4, radius_popup=4, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=False, pill_border=0,
        palette=dict(bg='#0b2f7a', surface='#1d4fae', surface2='#0a2462', border='#6f95d8',
                     strip='#123d92', strip_grad='#08205a', btn='#9aa0aa', btn_grad='#3c4049', btn_border='#c8ccd4',
                     text='#ffffff', value='#ffffff', dim='#b8c8e8', head='#ffffff', accent='#ffa000',
                     active='#ffa000', active_grad='#c06a00', active_border='#ffe0a0', active_text='#000000',
                     mode='#ffffff', meter_bar='#ffffff', meter_low='#40e040', meter_high='#ff3030',
                     vfo='#ffd040', spot='#ffd040', sel_bg='#ffa000', sel_text='#000000', mode_dig='#ffd040'),
        waterfall=['#00062a', '#000a60', '#0030c0', '#00a0ff', '#40ffff', '#ffff00', '#ff8000', '#ff0000']),
    # Icom de base: negro, teclas grises en degradé, azul para lo activo, modo en recuadro
    'Icom IC-7610': dict(
        base='Classic', font='arial', radius=3, radius_btn=4, radius_popup=4, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#000000', surface='#262626', surface2='#050505', border='#5a5a5a',
                     strip='#141414', strip_grad='#000000', btn='#6c6c6c', btn_grad='#262626', btn_border='#9a9a9a',
                     text='#ffffff', value='#ffffff', dim='#a8a8a8', head='#40c8ff', accent='#ffa000',
                     active='#2f72d6', active_grad='#123e86', active_border='#9fd2ff', active_text='#ffffff',
                     mode='#ffffff', mode_bg='#1f5fbf', mode_border='#9fd2ff',
                     meter_bar='#e6e6e6', meter_low='#40e040', meter_high='#ff3030', vfo='#40c8ff', spot='#40e8ff'),
        waterfall=['#000000', '#000048', '#0020c0', '#0090ff', '#00ffff', '#ffff00', '#ff8000', '#ff0000', '#ffffff']),
    # Yaesu de uso general: paneles gris azulado, teclas azules, letra blanca
    'Yaesu FT-991A': dict(
        base='Classic', font='arial', radius=4, radius_btn=5, radius_popup=5, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=False, pill_border=0,
        palette=dict(bg='#0e1620', surface='#22303e', surface2='#0e1620', border='#4c6a88',
                     btn='#2f63b8', btn_grad='#163a78', btn_border='#7aa4e0', text='#ffffff', value='#ffffff',
                     dim='#9ab0c8', head='#ffe040', accent='#ffe040', active='#ffe040', active_grad='#c8a800',
                     active_border='#fff4a0', active_text='#101010', mode='#ffe040',
                     meter_bar='#ffffff', meter_low='#40e060', meter_high='#ff3030', vfo='#ffe040', spot='#ffe040'),
        waterfall=['#000010', '#00185a', '#0050c0', '#00c060', '#e0e000', '#ff6000', '#ffffff']),
    # Kenwood de tope de gama: frecuencia ámbar sobre negro con acentos azules
    'Kenwood TS-990': dict(
        base='Classic', font='dejavu', radius=3, radius_btn=3, radius_popup=3, border=1,
        btn_gradient=True, box_gradient=True, focus_solid=True, mode_box=True, pill_border=1,
        palette=dict(bg='#000000', surface='#1a1208', surface2='#000000', border='#5a4220',
                     btn='#2c2418', btn_grad='#0e0a06', btn_border='#6a5030', text='#ffc860', value='#ffb030',
                     dim='#a88a50', head='#60b8ff', accent='#60b8ff', active='#2a6ad0', active_grad='#103a80',
                     active_border='#9ad0ff', active_text='#ffffff', mode='#ffffff', mode_bg='#2a6ad0',
                     mode_border='#9ad0ff', meter_bar='#ffb030', meter_low='#ffb030', meter_high='#ff3020',
                     vfo='#60b8ff', spot='#60b8ff', mode_dig='#60b8ff'),
        waterfall=['#000000', '#00103a', '#00308a', '#0080d0', '#40c0ff', '#ffd060', '#ffffff']),
    # Elecraft portátil: visor de cristal líquido claro, números negros (bueno al sol)
    'Elecraft KX3': dict(
        base='Outdoor', font='roboto', radius=3, radius_btn=3, radius_popup=3, border=2,
        btn_gradient=False, box_gradient=False, focus_solid=True, mode_box=False, pill_border=0, meter_gradient=True,
        palette=dict(bg='#c9d3a4', surface='#d8e0b4', surface2='#d8e0b4', border='#3a4228',
                     btn='#b8c290', btn_border='#3a4228', text='#101408', value='#000000', dim='#4a5232',
                     head='#000000', accent='#2a3010', active='#1c2208', active_text='#d8e0b4', mode='#000000',
                     meter_bar='#1c2208', meter_weak='#7a845a', meter_low='#1c2208', meter_high='#000000',
                     meter_over='#000000', vfo='#000000', spot='#000000', vfo_line='#000000', sel_bg='#1c2208',
                     sel_text='#d8e0b4', ok='#1c4a10', warn='#5a3a00', err='#7a0000', info='#102a50', tx='#7a0000',
                     digi='#3a1060', mode_dig='#000000', page_text='#000000',
                     bp=('#8a946a', '#a0a070', '#98a878', '#a08868', '#101408')),
        waterfall=['#c9d3a4', '#aab48a', '#8a946c', '#6a7450', '#4a5236', '#2c321e', '#101408']),
    # SDR de PC (SmartSDR): pizarra oscura, amarillo para lo activo, panorama azul y cascada a rojo
    'Flex SmartSDR': dict(
        base='Classic', font='roboto', radius=2, radius_btn=2, radius_popup=2, border=1, rule=True,
        btn_gradient=False, box_gradient=True, focus_solid=True, mode_box=False, pill_border=0,
        palette=dict(bg='#0a0e14', surface='#1c2430', surface2='#10161e', border='#34404e',
                     btn='#1c2430', btn_border='#34404e', text='#e8eef6', value='#ffffff', dim='#8a98a8',
                     head='#40b0ff', accent='#ffc400', active='#ffc400', active_text='#101010', mode='#ffc400',
                     meter_bar='#40b0ff', meter_low='#40e070', meter_high='#ff3a3a', vfo='#ffc400', spot='#40b0ff',
                     sel_bg='#3a3000', sel_text='#ffc400', page_text='#ffc400'),
        waterfall=['#000000', '#000a3c', '#002a8c', '#0070d0', '#00c0a0', '#a0e000', '#ffc000', '#ff2000']),
    # SDR de PC (Thetis): negro, frecuencia amarilla, rótulos verdes
    'ANAN Thetis': dict(
        base='Classic', font='arial', radius=0, radius_btn=2, radius_popup=2, border=1,
        btn_gradient=True, box_gradient=False, focus_solid=True, mode_box=False, pill_border=0,
        palette=dict(bg='#000000', surface='#0a0a0a', surface2='#000000', border='#404040',
                     btn='#4a4a4a', btn_grad='#1e1e1e', btn_border='#707070', text='#e0e0e0', value='#ffff40',
                     dim='#909090', head='#40ff60', accent='#ffff40', active='#3a8a3a', active_grad='#1a4a1a',
                     active_border='#80ff80', active_text='#ffffff', mode='#40ff60',
                     meter_bar='#40ff60', meter_low='#ffff40', meter_high='#ff3030', vfo='#ffff40', spot='#40ff60',
                     mode_dig='#40ff60'),
        waterfall=['#000000', '#00002a', '#000080', '#0000ff', '#00ffff', '#00ff00', '#ffff00', '#ff0000']),
    # SDR de PC (ExpertSDR): negro y naranja
    'Expert SunSDR': dict(
        base='Classic', font='roboto', radius=4, radius_btn=4, radius_popup=4, border=1,
        btn_gradient=False, box_gradient=True, focus_solid=True, mode_box=True, pill_border=0,
        palette=dict(bg='#0c0c0e', surface='#1e1e22', surface2='#121214', border='#3a3a40',
                     btn='#1e1e22', btn_border='#3a3a40', text='#f0f0f0', value='#ffffff', dim='#9a9aa0',
                     head='#ff8a20', accent='#ff8a20', active='#ff8a20', active_text='#140800', mode='#140800',
                     mode_bg='#ff8a20', mode_border='#ffb870', meter_bar='#ff8a20', meter_low='#ff8a20',
                     meter_high='#ff2a2a', vfo='#ff8a20', spot='#ffb870', sel_bg='#4a2808', sel_text='#ffb870',
                     mode_dig='#ff8a20'),
        waterfall=['#000000', '#100020', '#300060', '#8000a0', '#e02060', '#ff8020', '#ffe080', '#ffffff']),
}
BASE_THEMES.update(MORE_RIGS)


def make_deck_variant(src):
    """Crea una variante con disposición deck (frecuencia gigante, medidores al costado, teclas planas)."""
    t = dict(src)
    t['deck'] = True
    t['font'] = 'rcond'
    t['radius_btn'] = 0
    return t


# Construimos el diccionario final THEMES con cada tema y su variante Deck correspondiente
THEMES = {}
for name, spec in BASE_THEMES.items():
    THEMES[name] = spec
    THEMES[name + ' Deck'] = make_deck_variant(spec)


def build(name, t):
    c = roles(t['palette'], t['base'])
    j = {'base': t['base'], 'colors': {k: '#%06x' % v for k, v in c.items()}}
    for k in ('radius', 'radius_btn', 'radius_popup', 'border', 'btn_gradient', 'box_gradient', 'focus_solid',
              'pill_border', 'mode_box', 'btn_gap', 'rule', 'meter_gradient', 'mono_cluster', 'deck', 'font'):
        if k in t:
            j[k] = t[k]
    j['waterfall'] = t['waterfall']
    j['waterfall_steps'] = 0
    return j, c


def main():
    os.makedirs(OUT, exist_ok=True)
    built = {}
    for f in os.listdir(OUT):          # los que ya no existen
        if f.endswith('.json'):
            os.remove(os.path.join(OUT, f))
    for name, t in THEMES.items():
        assert len(name) < 32, name
        j, c = build(name, t)
        with open(os.path.join(OUT, name + '.json'), 'w') as f:
            json.dump(j, f, indent=1)
            f.write('\n')
        if not t.get('deck'):
            built[name] = (j, c)
        print('ok', name, len(j['colors']), 'colores')
    if '--preview' in sys.argv:
        import alta_gama_preview
        out = os.path.join(ROOT, 'docs', 'screenshots', 'themes_alta_gama')
        pro = {k: v for k, v in built.items() if k.startswith(('Pro ', 'Noche'))}
        alta_gama_preview.render_all(pro, out, 'pro')
        alta_gama_preview.render_all({k: v for k, v in built.items() if k not in pro}, out, 'estilos')


if __name__ == '__main__':
    main()
