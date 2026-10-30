#!/usr/bin/env python3
"""Vuelve a generar src/fonts/lat_<familia>_N.c (letras con acento de la interfaz) con
lv_font_conv, mismo tamaño y fuente que dice cada archivo, y el juego de letras de SYMBOLS
(español, alemán, francés e italiano). Para Sony arma la plantilla y después hay que correr
tools/fonts/sony_accents.py, que dibuja las letras.

Uso: python3 tools/fonts/lat_gen.py /ruta/a/lv_font_conv.js
"""
import re, subprocess, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
FONTS = ROOT / 'src' / 'fonts'
SYMBOLS = '¡¿ÁÉÍÓÚÜÑáéíóúüñºª°ÀÂÄÇÈÊËÌÎÏÒÔÖÙÛàâäçèêëìîïòôöùû'
CONV = sys.argv[1]
SONY_TEMPLATE = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def conv(font, size, out):
    subprocess.run(['node', CONV, '--bpp', '4', '--size', str(size), '--no-compress', '--no-kerning',
                    '--font', font, '--symbols', SYMBOLS, '--format', 'lvgl', '-o', str(out)], check=True,
                   capture_output=True)


for path in sorted(FONTS.glob('lat_*_*.c')):
    old = path.read_text()
    size = int(re.search(r' \* Size: (\d+) px', old).group(1))
    m = re.search(r'--font (\S+)', old)
    name = path.stem
    if name.startswith('lat_sony_'):
        conv(SONY_TEMPLATE, size, path)
        new = path.read_text()
        # las medidas de la línea, como estaban (las de Sony)
        for key in ('line_height', 'base_line'):
            v = re.search(rf'\.{key} = (-?\d+)', old).group(1)
            new = re.sub(rf'\.{key} = -?\d+', f'.{key} = {v}', new, count=1)
        path.write_text(new)
    else:
        conv(m.group(1), size, path)
    print(path.name, size)
