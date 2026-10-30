#!/usr/bin/env python3
"""Genera src/countries_data.inc: los países (ISO 3166), su región IARU, el nombre en los
idiomas de la interfaz, la zona horaria POSIX de su ciudad principal y su locator.

Necesita Babel (nombres de países) y los datos de zonas horarias del sistema (tzdata).
Uso: python3 tools/countries/gen.py
"""
import pathlib, unicodedata
from babel import Locale

ROOT = pathlib.Path(__file__).resolve().parents[2]
ZI = pathlib.Path('/usr/share/zoneinfo')
LANGS = ['en', 'es', 'de', 'fr', 'it']          # el orden de lang_t (i18n.h)
FONT_OK = set(chr(c) for c in range(32, 127)) | set('¡¿ÁÉÍÓÚÜÑáéíóúüñºª°ÀÂÄÇÈÊËÌÎÏÒÔÖÙÛàâäçèêëìîïòôöùû')

# Región IARU (la del país miembro; Mongolia y Asia Central van con la 1)
R2 = set('AG AI AR AW BB BL BM BO BQ BR BS BZ CA CL CO CR CU CW DM DO EC FK GD GF GL GP GS GT GY HN HT '
         'JM KN KY LC MF MQ MS MX NI PA PE PM PR PY SR SV SX TC TT US UY VC VE VG VI'.split())
R3 = set('AF AS AU BD BN BT CC CK CN CX FJ FM GU HK ID IN IO IR JP KH KI KP KR LA LK MH MM MO MP MV MY '
         'NC NF NP NR NU NZ PF PG PH PK PN PW SB SG TH TK TL TO TV TW UM VN VU WF WS'.split())
SKIP = {'AQ', 'BV', 'HM', 'TF'}                # sin población estable


def font_safe(s):
    s = s.replace('’', "'").replace('‘', "'").replace('–', '-')
    out = []
    for ch in s:
        if ch in FONT_OK:
            out.append(ch)
        else:
            base = ''.join(c for c in unicodedata.normalize('NFKD', ch) if not unicodedata.combining(c))
            out.append(base if base and all(c in FONT_OK for c in base) else
                       {'ß': 'ss', 'Ø': 'O', 'ø': 'o', 'Å': 'A', 'å': 'a', 'Æ': 'AE', 'æ': 'ae', 'Œ': 'OE',
                        'œ': 'oe', 'Ð': 'D', 'ð': 'd', 'Þ': 'Th', 'þ': 'th', 'ı': 'i', 'Ł': 'L', 'ł': 'l'}.get(ch, ''))
    return ''.join(out)


def sort_key(s):
    return ''.join(c for c in unicodedata.normalize('NFKD', s) if not unicodedata.combining(c)).casefold()


def posix_tz(zone):
    data = (ZI / zone).read_bytes()
    return data.rstrip(b'\n').rsplit(b'\n', 1)[-1].decode()


def locator(lat, lon):
    lon += 180; lat += 90
    a = chr(65 + int(lon // 20)) + chr(65 + int(lat // 10))
    b = str(int(lon % 20 // 2)) + str(int(lat % 10))
    c = chr(97 + int(lon % 2 * 12)) + chr(97 + int(lat % 1 * 24))
    return (a + b + c).upper()


def coord(s):
    sign = -1 if s[0] == '-' else 1
    s = s[1:]
    d, m = (int(s[:2]), int(s[2:4])) if len(s) in (4, 6) else (int(s[:3]), int(s[3:5]))
    return sign * (d + m / 60)


# Países con varias zonas: la de la capital o la ciudad más poblada
PRIMARY = {'RU': 'Europe/Moscow', 'US': 'America/New_York', 'CA': 'America/Toronto', 'BR': 'America/Sao_Paulo',
           'AU': 'Australia/Sydney', 'MX': 'America/Mexico_City', 'AR': 'America/Argentina/Buenos_Aires',
           'CL': 'America/Santiago', 'ES': 'Europe/Madrid', 'PT': 'Europe/Lisbon', 'KZ': 'Asia/Almaty',
           'CN': 'Asia/Shanghai', 'ID': 'Asia/Jakarta', 'EC': 'America/Guayaquil', 'DE': 'Europe/Berlin',
           'UA': 'Europe/Kyiv', 'MN': 'Asia/Ulaanbaatar', 'CD': 'Africa/Kinshasa', 'NZ': 'Pacific/Auckland',
           'FM': 'Pacific/Pohnpei', 'KI': 'Pacific/Tarawa', 'PF': 'Pacific/Tahiti', 'MY': 'Asia/Kuala_Lumpur',
           'UZ': 'Asia/Tashkent', 'GL': 'America/Nuuk', 'PS': 'Asia/Gaza', 'CY': 'Asia/Nicosia'}
zones = {}
for line in (ZI / 'zone.tab').read_text().splitlines():
    if line.startswith('#'):
        continue
    cc, co, zone = line.split('\t')[:3]
    if cc in zones and PRIMARY.get(cc) != zone:
        continue
    i = max(co.rfind('+'), co.rfind('-'))
    zones[cc] = (zone, coord(co[i:]), coord(co[:i]))

locs = {l: Locale(l) for l in LANGS}
codes = [l.split('\t')[0] for l in (ZI / 'iso3166.tab').read_text().splitlines() if not l.startswith('#')]
rows = []
for cc in sorted(codes):
    if cc in SKIP:
        continue
    names = [font_safe(locs[l].territories.get(cc, cc)) for l in LANGS]
    region = 2 if cc in R2 else 3 if cc in R3 else 1
    tz, loc = '', ''
    if cc in zones:
        zone, lon, lat = zones[cc]
        tz, loc = posix_tz(zone), locator(lat, lon)
    rows.append((cc, region, names, tz, loc))


def c(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


out = ['/* Generado por tools/countries/gen.py: no editar a mano */',
       f'#define COUNTRIES_N {len(rows)}',
       'static const country_t country_tab[COUNTRIES_N] = {']
for cc, region, names, tz, loc in rows:
    out.append(f'    {{"{cc}", {region}, {{{", ".join(c(n) for n in names)}}}, {c(tz)}, "{loc}"}},')
out.append('};')
out.append('/* Orden alfabético en cada idioma */')
out.append(f'static const uint16_t country_order[{len(LANGS)}][COUNTRIES_N] = {{')
for li in range(len(LANGS)):
    order = sorted(range(len(rows)), key=lambda i: sort_key(rows[i][2][li]))
    out.append('    {' + ', '.join(map(str, order)) + '},')
out.append('};')
(ROOT / 'src' / 'countries_data.inc').write_text('\n'.join(out) + '\n', encoding='utf-8')
print(len(rows), 'países;', sum(1 for r in rows if r[1] == 2), 'en la Región 2,',
      sum(1 for r in rows if r[1] == 3), 'en la 3')
