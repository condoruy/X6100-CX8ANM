#!/usr/bin/env python3
"""Genera src/i18n_<idioma>.inc desde tools/i18n/<idioma>.txt y revisa la tabla."""
import re, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z)?[diouxXeEfgGcsp%]')

def unesc(s):
    return s.replace('\\n', '\n').replace('\\t', '\t')

def cesc(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t') + '"'

def build(lang):
    src = ROOT / 'tools' / 'i18n' / f'{lang}.txt'
    rows, errors, seen = [], [], {}
    for no, line in enumerate(src.read_text(encoding='utf-8').splitlines(), 1):
        if not line.strip() or line.startswith('#'):
            continue
        parts = line.split('\t')
        if len(parts) != 2:
            errors.append(f'{src.name}:{no}: falta el TAB o hay más de uno'); continue
        en, tr = unesc(parts[0]), unesc(parts[1])
        if en in seen:
            errors.append(f'{src.name}:{no}: repetido (ya en la línea {seen[en]}): {en!r}'); continue
        seen[en] = no
        if SPEC.findall(en) != SPEC.findall(tr):
            errors.append(f'{src.name}:{no}: los % no coinciden: {en!r} -> {tr!r}')
        if en.count('\n') != tr.count('\n'):
            errors.append(f'{src.name}:{no}: distinta cantidad de líneas: {en!r}')
        rows.append((en, tr))
    keys = dict(rows)
    for en, tr in rows:   # una traducción que es a su vez otra clave se traduciría dos veces
        if tr != en and tr in keys and keys[tr] != tr:
            errors.append(f'la traducción {tr!r} (de {en!r}) es también un texto original')
    if errors:
        print('\n'.join(errors)); sys.exit(1)
    out = ROOT / 'src' / f'i18n_{lang}.inc'
    with out.open('w', encoding='utf-8') as f:
        f.write(f'/* Generado por tools/i18n/build.py desde tools/i18n/{lang}.txt: no editar a mano */\n')
        for en, tr in rows:
            f.write(f'{{ {cesc(en)}, {cesc(tr)} }},\n')
    print(f'{out.relative_to(ROOT)}: {len(rows)} textos')

for lang in sys.argv[1:] or ['es', 'de', 'fr', 'it']:
    build(lang)
