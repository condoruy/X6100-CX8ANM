from datetime import datetime, timezone, timedelta
from importlib import resources
try:
    import gzip
except ImportError:   # images built before python3-zlib was selected
    gzip = None
try:
    import zlib
except ImportError:
    zlib = None
import csv
import io
import json
import os
import pathlib
import re
import select
import shutil
import socket
import struct
import sqlite3
import subprocess
import tarfile
import tempfile
import threading
import time
import zipfile

import bottle

from . import models
from . import settings

app = bottle.Bottle()


@app.hook('before_request')
def page_scheme():
    """Only the Remote page needs HTTPS (the browser gives audio to secure pages only): it goes there, and
    every other page back to HTTP, where the radio's self-signed certificate never shows a warning. The
    API answers on both (X6100 Link uses HTTP)."""
    env = bottle.request.environ
    if (not settings.HTTPS_PORT or bottle.request.method != "GET" or bottle.request.path.startswith("/api/")
            or "text/html" not in bottle.request.headers.get("Accept", "")):
        return
    secure = str(env.get("SERVER_PORT")) == str(settings.HTTPS_PORT)
    remote = bottle.request.path == "/remote"
    if secure == remote:
        return
    host = bottle.request.get_header("Host", "").split(":")[0] or env.get("SERVER_NAME", "")
    scheme, port, std = ("https", settings.HTTPS_PORT, 443) if remote else ("http", settings.HTTP_PORT, 80)
    query = ("?" + env["QUERY_STRING"]) if env.get("QUERY_STRING") else ""
    bottle.redirect("%s://%s%s%s%s" % (scheme, host, "" if port == std else ":%d" % port, bottle.request.path,
                                       query), 302)

bottle.TEMPLATE_PATH += [
    resources.files('x6100_webserver').joinpath('views'),
]

STATIC_PATH = resources.files('x6100_webserver').joinpath('static')


def read_callsign():
    """Callsign from params.db for the page header, "" when missing."""
    try:
        con = sqlite3.connect(f"file:{settings.DB_PATH}?mode=ro", uri=True)
        try:
            row = con.execute("SELECT val FROM params WHERE name = 'callsign'").fetchone()
        finally:
            con.close()
    except sqlite3.Error:
        return ""
    return str(row[0]) if row and row[0] else ""


bottle.BaseTemplate.defaults.update(callsign=read_callsign, request=bottle.request)


# Bands API

@app.get('/api/bands')
def get_bands(dbcon):
    bands = models.read_bands(dbcon)
    bottle.response.content_type = 'application/json'
    return json.dumps([x.asdict() for x in bands])


@app.put('/api/bands')
def add_band(dbcon):
    data = bottle.request.json
    try:
        band_param = models.BandParams(**data)
        models.add_band(dbcon, band_param)
        bottle.response.status = 201
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.post('/api/bands/<band_id:int>')
def update_band(band_id, dbcon):
    data = bottle.request.json
    try:
        band_param = models.BandParams(id=band_id, **data)
        models.update_band(dbcon, band_param)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.delete('/api/bands/<band_id:int>')
def delete_band(band_id, dbcon):
    try:
        models.delete_band(dbcon, band_id)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


# Digital modes routes

@app.get('/api/digital_modes')
def get_digital_modes(dbcon):
    d_modes = models.read_digital_modes(dbcon)
    bottle.response.content_type = 'application/json'
    return json.dumps([x.asdict() for x in d_modes])


@app.put('/api/digital_modes')
def add_digital_mode(dbcon):
    data = bottle.request.json
    try:
        d_mode = models.DigitalMode(**data)
        models.add_digital_mode(dbcon, d_mode)
        bottle.response.status = 201
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.post('/api/digital_modes/<mode_id:int>')
def update_digital_mode(mode_id, dbcon):
    data = bottle.request.json
    try:
        d_mode = models.DigitalMode(id=mode_id, **data)
        models.update_digital_mode(dbcon, d_mode)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}


@app.delete('/api/digital_modes/<mode_id:int>')
def delete_digital_mode(mode_id, dbcon):
    try:
        models.delete_digital_mode(dbcon, mode_id)
        return {"status": "OK"}
    except ValueError as e:
        bottle.response.status = 400
        return {"status": "error", "msg": str(e)}

# DX cluster routes
#
# Same file and format as the radio GUI (src/cluster/client.c): "host=..."
# and "port=..." lines are the server in use, "server=name|host|port" lines
# the saved ones. The GUI reloads the server when the file's mtime changes.
# Filters and alerts are more lines: filter_modes=CW,SSB,DIG,AMFM, filter_bands=40,20,
# filter_activity=POTA,SOTA,WWBOTA,HOTA, max_age=MIN,
# skip_spotters=A,B, alert=PATTERN|bands|mode, alert_popup=0|1, alert_beep=0|1.

CLUSTER_DEFAULT_HOST = "dxfun.com"
# Used while cluster.conf has no server= lines (keep in sync with src/cluster/client.c)
CLUSTER_DEFAULT_SERVERS = [
    ("DXFUN", "dxfun.com", 8000),
    ("VE7CC", "dxc.ve7cc.net", 23),
    ("W3LPL", "dxc.w3lpl.net", 7373),
    ("K1TTT", "k1ttt.net", 7373),
    ("NC7J", "dxc.nc7j.com", 7373),
    ("W1NR", "dxc.w1nr.net", 23),
    ("WA9PIE", "dxc.wa9pie.net", 8000),
    ("K4ZR", "k4zr.no-ip.org", 7300),
    ("DB0SUE", "db0sue.de", 8000),
    ("GB7DJK", "gb7djk.dxcluster.net", 7300),
    ("AE5E", "dxspots.com", 23),
]
CLUSTER_DEFAULT_PORT = 8000
CLUSTER_MAX_SERVERS = 16
CLUSTER_MAX_ALERTS = 50
CLUSTER_MODES = ["CW", "SSB", "DIG", "AMFM"]
CLUSTER_BANDS = [160, 80, 60, 40, 30, 20, 17, 15, 12, 10, 6]
CLUSTER_ACTIVITY = ["POTA", "SOTA", "WWBOTA", "HOTA"]   # the GUI's cluster_act_names
# Written by the radio GUI: pattern|freq_hz|HH:MM of the last spot that matched
CLUSTER_ALERTS_SEEN = "/tmp/cluster_alerts_seen"


def read_cluster_conf():
    """Returns (host, port, servers): the server in use and [(name, host, port)]."""
    host, port, servers = CLUSTER_DEFAULT_HOST, CLUSTER_DEFAULT_PORT, []
    try:
        with open(settings.CLUSTER_CONF, errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        lines = []
    for line in lines:
        if line.startswith("host=") and line[5:]:
            host = line[5:]
        elif line.startswith("port="):
            m = re.match(r"\s*[+-]?\d+", line[5:])  # atoi()
            if m and 0 < int(m.group()) < 65536:
                port = int(m.group())
        elif line.startswith("server="):
            parts = line[7:].split("|")
            if len(parts) == 3 and parts[0] and parts[1] and parts[2].isdigit() and 0 < int(parts[2]) < 65536:
                servers.append((parts[0], parts[1], int(parts[2])))
    if not servers:
        servers = list(CLUSTER_DEFAULT_SERVERS)
    if (host, port) not in [(h, p) for _, h, p in servers]:
        servers.append((host, host, port))
    return host, port, servers


def read_cluster_options():
    """Filters and alerts of cluster.conf (same rules as the GUI: missing = no filter)."""
    opts = {"modes": list(CLUSTER_MODES), "bands": [], "activity": [], "max_age": 0, "skip": "", "alerts": [], "popup": False, "beep": False}
    try:
        with open(settings.CLUSTER_CONF, errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        lines = []
    for line in lines:
        if line.startswith("filter_modes="):
            modes = [m.strip().upper() for m in line[13:].split(",")]
            modes = [m for m in CLUSTER_MODES if m in modes]
            opts["modes"] = modes or list(CLUSTER_MODES)
        elif line.startswith("filter_bands="):
            bands = {int(b) for b in re.findall(r"\d+", line[13:])}
            opts["bands"] = [b for b in CLUSTER_BANDS if b in bands]
        elif line.startswith("filter_activity="):
            act = [a.strip().upper() for a in line[16:].split(",")]
            opts["activity"] = [a for a in CLUSTER_ACTIVITY if a in act]
        elif line.startswith("max_age="):
            m = re.match(r"\s*[+-]?\d+", line[8:])  # atoi()
            opts["max_age"] = int(m.group()) if m and 0 < int(m.group()) < 1440 else 0
        elif line.startswith("skip_spotters="):
            opts["skip"] = line[14:]
        elif line.startswith("alert_popup="):
            opts["popup"] = line[12:].strip() not in ("", "0")
        elif line.startswith("alert_beep="):
            opts["beep"] = line[11:].strip() not in ("", "0")
        elif line.startswith("alert=") or line.startswith("alert_off="):   # alert_off: kept, not used (X6100 Link)
            on = line.startswith("alert=")
            parts = line[6 if on else 10:].split("|")
            if len(parts) == 3 and parts[0].strip() and len(opts["alerts"]) < CLUSTER_MAX_ALERTS:
                opts["alerts"].append({"pattern": parts[0].strip(), "bands": parts[1], "mode": parts[2].strip().upper(), "enabled": on})
    return opts


def cluster_option_lines(opts):
    lines = []
    if set(opts["modes"]) != set(CLUSTER_MODES):
        lines.append("filter_modes=" + ",".join(opts["modes"]))
    if opts["bands"] and set(opts["bands"]) != set(CLUSTER_BANDS):
        lines.append("filter_bands=" + ",".join(str(b) for b in opts["bands"]))
    if opts["activity"]:
        lines.append("filter_activity=" + ",".join(opts["activity"]))
    if opts["max_age"]:
        lines.append(f"max_age={opts['max_age']}")
    if opts["skip"]:
        lines.append("skip_spotters=" + opts["skip"])
    for a in opts["alerts"]:
        lines.append(f"{'alert' if a.get('enabled', True) else 'alert_off'}={a['pattern']}|{a['bands']}|{a['mode']}")
    lines.append(f"alert_popup={int(opts['popup'])}")
    lines.append(f"alert_beep={int(opts['beep'])}")
    return lines


def write_cluster_conf(host, port, servers, opts=None):
    if opts is None:
        opts = read_cluster_options()  # keep the filters and alerts
    tmp = settings.CLUSTER_CONF + ".tmp"
    with open(tmp, "w") as f:
        f.write(f"host={host}\nport={port}\n")
        for name, h, p in servers:
            f.write(f"server={name}|{h}|{p}\n")
        for line in cluster_option_lines(opts):
            f.write(line + "\n")
        f.flush()
        os.fsync(f.fileno())
    os.rename(tmp, settings.CLUSTER_CONF)


def cluster_alerts_seen():
    seen = {}
    try:
        with open(CLUSTER_ALERTS_SEEN, errors="replace") as f:
            for line in f.read().splitlines():
                parts = line.split("|")
                if len(parts) == 3 and parts[1].isdigit():
                    seen[parts[0]] = f"{int(parts[1]) / 1000:.1f} kHz {parts[2]}Z"
    except OSError:
        pass
    return seen


def cluster_state():
    host, port, servers = read_cluster_conf()
    opts = read_cluster_options()
    seen = cluster_alerts_seen()
    for a in opts["alerts"]:
        a["seen"] = seen.get(a["pattern"], "")
    return {"host": host, "port": port,
            "servers": [{"name": n, "host": h, "port": p} for n, h, p in servers],
            "filters": {"modes": opts["modes"], "bands": opts["bands"], "activity": opts["activity"], "max_age": opts["max_age"], "skip": opts["skip"]},
            "alerts": opts["alerts"], "popup": opts["popup"], "beep": opts["beep"],
            "options": {"modes": CLUSTER_MODES, "bands": CLUSTER_BANDS, "activity": CLUSTER_ACTIVITY}}  # what this firmware has (X6100 Link)


def cluster_error(status, msg):
    bottle.response.status = status
    return {"status": "error", "msg": msg}


def cluster_save(host, port, servers, opts=None):
    try:
        write_cluster_conf(host, port, servers, opts)
    except OSError as e:
        return cluster_error(500, f"Can't save: {e}")
    return cluster_state()


# wfview server routes
# Same file and format as the radio GUI (src/wfview_server.c): enabled=0|1, user=, pass=
WFVIEW_DEFAULTS = {"enabled": "0", "user": "x6100", "pass": "x6100"}


def read_wfview_conf():
    conf = dict(WFVIEW_DEFAULTS)
    try:
        with open(settings.WFVIEW_CONF, errors="replace") as f:
            for line in f:
                key, sep, val = line.rstrip("\r\n").partition("=")
                if sep and key in conf:
                    conf[key] = val
    except OSError:
        pass
    return {"enabled": conf["enabled"] == "1", "user": conf["user"], "pass": conf["pass"]}


@app.route('/wfview')
def wfview():
    return bottle.template('wfview')


@app.get('/api/wfview')
def get_wfview():
    return read_wfview_conf()


@app.post('/api/wfview')
def set_wfview():
    data = bottle.request.json or {}
    user = str(data.get("user", "")).strip()
    passwd = str(data.get("pass", ""))
    # The GUI keeps 15 characters; login fields are 16 bytes
    for val, what in ((user, "user"), (passwd, "password")):
        if not re.fullmatch(r"[ -~]{1,15}", val) or "=" in val:
            bottle.response.status = 400
            return {"status": "error", "msg": f"Invalid {what}: 1-15 printable characters, no '='"}
    try:
        tmp = settings.WFVIEW_CONF + ".tmp"
        with open(tmp, "w") as f:
            f.write(f"enabled={1 if data.get('enabled') else 0}\nuser={user}\npass={passwd}\n")
        os.rename(tmp, settings.WFVIEW_CONF)
    except OSError as e:
        bottle.response.status = 500
        return {"status": "error", "msg": f"Can't save: {e}"}
    return read_wfview_conf()


# Field Ops routes: x6100-field (rootfs/usr/sbin) does the downloads, same files as the GUI:
# <FIELD_DIR>/refs.tsv, qrz.conf and hota.conf ("key=..."), logs/*.adi

field_lock = threading.Lock()
field_update = {"running": False, "result": ""}
field_refs_counts = (None, {})   # (mtime, size), {"P": parks, "S": summits, "B": bunkers, "H": HOTA}


def field_path(*names):
    return os.path.join(settings.FIELD_DIR, *names)


def key_set(name):
    try:
        with open(field_path(name), errors="replace") as f:
            return any(line.startswith("key=") and line[4:].strip() for line in f)
    except OSError:
        return False


def field_refs():
    global field_refs_counts
    try:
        st = os.stat(field_path("refs.tsv"))
    except OSError:
        return None
    stamp = (st.st_mtime, st.st_size)
    if field_refs_counts[0] != stamp:   # counted once per new file
        counts = {}
        with open(field_path("refs.tsv"), "rb") as f:
            for line in f:
                counts[line[:1]] = counts.get(line[:1], 0) + 1
        field_refs_counts = (stamp, counts)
    c = field_refs_counts[1]
    return {"date": time.strftime("%Y-%m-%d %H:%M", time.localtime(st.st_mtime)), "size": fmt_size(st.st_size),
            "parks": c.get(b"P", 0), "summits": c.get(b"S", 0), "bunkers": c.get(b"B", 0), "hota": c.get(b"H", 0)}


def field_logs():
    try:
        names = os.listdir(field_path("logs"))
    except OSError:
        return []
    logs = []
    for name in names:
        path = field_path("logs", name)
        if name.endswith(".adi") and os.path.isfile(path):
            st = os.stat(path)
            logs.append({"name": name, "size": fmt_size(st.st_size), "mtime": st.st_mtime,
                         "date": time.strftime("%Y-%m-%d %H:%M", time.localtime(st.st_mtime))})
    return sorted(logs, key=lambda x: x["mtime"], reverse=True)


def run_field_update():
    env = dict(os.environ, X6100_FIELD_DIR=settings.FIELD_DIR, X6100_CALL=read_callsign())
    try:
        r = subprocess.run([settings.FIELD_CMD, "update"], capture_output=True, text=True, timeout=600, env=env)
        lines = r.stdout.strip().splitlines()
        result = lines[-1] if lines else f"error exit code {r.returncode}"
    except (OSError, subprocess.SubprocessError) as e:
        result = f"error {e}"
    with field_lock:
        field_update.update(running=False, result=result)


@app.route('/field')
def field():
    return bottle.template('field')


@app.get('/api/field')
def get_field():
    with field_lock:
        update = dict(field_update)
    return {"qrz_key": key_set("qrz.conf"), "hota_key": key_set("hota.conf"), "update": update, "refs": field_refs(),
            "logs": field_logs()}


@app.post('/api/field/<name:re:qrz|hota>')
def set_field_key(name):
    """QRZ Logbook API key, cqhota.app integration key: <name>.conf, never shown back"""
    key = str((bottle.request.json or {}).get("key", "")).strip()
    if key and not re.fullmatch(r"[!-~]{1,128}", key):
        return api_error(400, "Invalid key: 1-128 printable characters, no spaces")
    path = field_path(name + ".conf")
    try:
        os.makedirs(settings.FIELD_DIR, exist_ok=True)
        if key:
            tmp = path + ".tmp"
            with os.fdopen(os.open(tmp, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as f:
                f.write(f"key={key}\n")
            os.rename(tmp, path)
        elif os.path.exists(path):
            os.remove(path)
    except OSError as e:
        return api_error(500, f"Can't save: {e}")
    return get_field()


@app.post('/api/field/update')
def start_field_update():
    with field_lock:
        if field_update["running"]:
            return api_error(409, "An update is already running")
        field_update.update(running=True, result="")
    threading.Thread(target=run_field_update, daemon=True).start()
    return get_field()


@app.get('/field/logs/<name>')
def field_log_file(name):
    if not name.endswith(".adi"):
        raise bottle.HTTPError(404, "Not found")
    return bottle.static_file(name, root=field_path("logs"), download=True, mimetype="text/plain")


def field_active_log():
    """The log of the activation running on the radio ("" when none), from the GUI's state file."""
    try:
        with open(field_path("state"), errors="replace") as f:
            kv = dict(line.rstrip("\n").split("=", 1) for line in f if "=" in line)
    except OSError:
        return ""
    return os.path.basename(kv.get("log", "")) if kv.get("active") == "1" else ""


def adif_split(text):
    """Header (up to <eoh>) and the parts between <eor>s; the last part is what follows the last record."""
    m = re.search(r'<eoh>', text, re.I)
    head, body = (text[:m.end()], text[m.end():]) if m else ("", text)
    return head, re.split(r'(?i)<eor>', body)


def adif_set(rec, name, value):
    """rec with the field `name` set to value (added before the end when missing)."""
    m = re.search(r'<%s:(\d+)(?::\w)?>' % name, rec, re.I)
    new = "<%s:%d>%s" % (name, len(value), value)
    if not m:
        return rec.rstrip() + " " + new + " "
    return rec[:m.start()] + new + rec[m.end() + int(m.group(1)):]


def adif_del(rec, name):
    """rec without the field `name`."""
    m = re.search(r'<%s:(\d+)(?::\w)?>' % name, rec, re.I)
    return rec if not m else rec[:m.start()] + rec[m.end() + int(m.group(1)):]


SOTA_REF = r'[A-Z0-9]{1,4}/[A-Z]{2}-\d{3}'     # I/LO-123
POTA_REF = r'[A-Z0-9]{1,4}-\d{4,5}'            # IT-1234


def set_ref(rec, ref):
    """The other station's SOTA or POTA reference (by its form); "" removes it."""
    rec = adif_del(adif_del(rec, "SOTA_REF"), "POTA_REF")
    if not ref:
        return rec
    return adif_set(rec, "SOTA_REF" if re.fullmatch(SOTA_REF, ref) else "POTA_REF", ref)


def edit_qso(name, n, fn):
    """Applies fn to record n (0 = the first QSO of the file; records without CALL do not count):
    fn returns the new record, or None to delete it. Returns an error response or None."""
    if not name.endswith(".adi") or os.path.basename(name) != name:
        return api_error(400, "Not a log")
    path = field_path("logs", name)
    try:
        with open(path, errors="replace") as f:
            text = f.read()
    except FileNotFoundError:
        return api_error(404, "Not found")
    head, parts = adif_split(text)
    recs = [i for i, p in enumerate(parts[:-1]) if re.search(r'<call:\d+', p, re.I)]
    if not 0 <= n < len(recs):
        return api_error(404, "No such QSO")
    new = fn(parts[recs[n]])
    if new is None:
        del parts[recs[n]]
    else:
        parts[recs[n]] = new
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        f.write(head + "<eor>".join(parts))
    os.replace(tmp, path)
    return None


@app.delete('/api/field/logs/<name>/<n:int>')
def delete_qso(name, n):
    return edit_qso(name, n, lambda rec: None) or {"status": "OK"}


@app.put('/api/field/logs/<name>/<n:int>')
def put_qso(name, n):
    """JSON {"call", "rst_sent", "rst_rcvd", "ref"?}: the QSO's call, reports and the other station's
    SOTA or POTA reference ("" none; left as it is when missing)."""
    try:
        data = bottle.request.json
    except ValueError:
        data = None
    if not isinstance(data, dict):
        return api_error(400, "Invalid QSO")
    call = str(data.get("call", "")).strip().upper()
    rs, rr = str(data.get("rst_sent", "")).strip(), str(data.get("rst_rcvd", "")).strip()
    rst = r'[+-]?\d{1,3}'     # 59, 599, FT8 dB (-10)
    if not re.fullmatch(r'[A-Z0-9/]{3,15}', call) or not re.fullmatch(rst, rs) or not re.fullmatch(rst, rr):
        return api_error(400, "Invalid QSO")
    ref = data.get("ref")
    if ref is not None:
        ref = str(ref).strip().upper()
        if ref and not (re.fullmatch(SOTA_REF, ref) or re.fullmatch(POTA_REF, ref)):
            return api_error(400, "Invalid SOTA/POTA reference")

    def change(rec):
        rec = adif_set(adif_set(adif_set(rec, "CALL", call), "RST_SENT", rs), "RST_RCVD", rr)
        return rec if ref is None else set_ref(rec, ref)
    return edit_qso(name, n, change) or {"status": "OK"}


@app.delete('/api/field/logs/<name>')
def delete_field_log(name):
    if not name.endswith(".adi") or os.path.basename(name) != name:
        return api_error(400, "Not a log")
    if name == field_active_log():
        return api_error(409, "This activation is still running: end it on the radio first")
    try:
        os.remove(field_path("logs", name))
    except FileNotFoundError:
        return api_error(404, "Not found")
    return get_field()


# Logs page: every QSO of every log in <FIELD_DIR>/logs (the day's logs YYYYMMDD.adi and the activations),
# to see, filter, edit, delete and export. QSOs are addressed as (file, n) as edit_qso() counts them.

def adif_fields(rec):
    """The fields of an ADIF record, names in lower case."""
    out, i = {}, 0
    for m in re.finditer(r'<(\w+):(\d+)(?::\w)?>', rec):
        if m.start() < i:
            continue
        n = int(m.group(2))
        out[m.group(1).lower()] = rec[m.end():m.end() + n]
        i = m.end() + n
    return out


def log_records(name):
    """The QSO records of a log (records without CALL do not count, as in edit_qso)."""
    try:
        with open(field_path("logs", name), errors="replace") as f:
            text = f.read()
    except OSError:
        return []
    return [p for p in adif_split(text)[1][:-1] if re.search(r'<call:\d+', p, re.I)]


@app.route('/logs')
def logs_page():
    return bottle.template('logs')


@app.get('/api/logs')
def get_logs():
    qsos = []
    for log in field_logs():
        for n, rec in enumerate(log_records(log["name"])):
            qsos.append(dict(adif_fields(rec), file=log["name"], n=n))
    return {"qsos": qsos, "active": field_active_log()}


# Editable fields and what they may hold (no < or >: they would break the ADIF tags)
LOG_FIELDS = {"call": r'[A-Z0-9/]{3,15}', "qso_date": r'\d{8}', "time_on": r'\d{4}(\d{2})?',
              "band": r'\d{1,4}(\.\d+)?C?M|', "freq": r'\d{1,4}(\.\d{1,6})?|', "mode": r'[A-Z0-9-]{1,12}|',
              "rst_sent": r'[+-]?\d{1,3}|', "rst_rcvd": r'[+-]?\d{1,3}|', "name": r'[^<>]{0,40}',
              "qth": r'[^<>]{0,40}', "gridsquare": r'([A-R]{2}\d{2}([A-X]{2})?)?', "comment": r'[^<>]{0,80}',
              "sota_ref": r'(%s)?' % SOTA_REF, "pota_ref": r'(%s)?' % POTA_REF}


@app.put('/api/logs/<name>/<n:int>')
def put_log_qso(name, n):
    """JSON {field: value} for any of LOG_FIELDS; an empty value removes the field (CALL and the date stay)."""
    try:
        data = bottle.request.json
    except ValueError:
        data = None
    if not isinstance(data, dict) or not data:
        return api_error(400, "Invalid QSO")
    clean = {}
    for k, v in data.items():
        v = str(v).strip()
        v = v if k in ("name", "qth", "comment") else v.upper()
        if k not in LOG_FIELDS or not re.fullmatch(LOG_FIELDS[k], v) or (k in ("call", "qso_date", "time_on") and not v):
            return api_error(400, f"Invalid {k.upper()}: {v}")
        clean[k] = v

    def change(rec):
        for k, v in clean.items():
            rec = adif_set(rec, k.upper(), v) if v else adif_del(rec, k.upper())
        return rec
    return edit_qso(name, n, change) or {"status": "OK"}


@app.post('/logs/export')
def export_logs():
    """Form field sel = "file:n,file:n,...", fmt = adi | csv: those QSOs as one ADIF or CSV download."""
    want = {}
    for item in bottle.request.forms.get("sel", "").split(","):
        name, _, n = item.rpartition(":")
        if name.endswith(".adi") and os.path.basename(name) == name and n.isdigit():
            want.setdefault(name, set()).add(int(n))
    recs = [rec for name in sorted(want) for n, rec in enumerate(log_records(name)) if n in want[name]]
    stamp = time.strftime("%Y%m%d-%H%M")
    if bottle.request.forms.get("fmt") == "csv":
        cols = ["qso_date", "time_on", "call", "band", "freq", "mode", "rst_sent", "rst_rcvd", "name", "qth",
                "gridsquare", "sota_ref", "pota_ref", "my_sota_ref", "my_pota_ref", "comment"]
        out = io.StringIO()
        w = csv.writer(out)
        w.writerow([c.upper() for c in cols])
        for rec in recs:
            f = adif_fields(rec)
            w.writerow([f.get(c, "") for c in cols])
        body, ext, mime = out.getvalue(), "csv", "text/csv"
    else:
        body = "X6100 log export\n<adif_ver:5>3.1.4 <programid:12>X6100 IU1WXY <eoh>\n" + \
               "".join(rec.strip() + " <eor>\n" for rec in recs)
        ext, mime = "adi", "text/plain"
    bottle.response.content_type = mime + "; charset=utf-8"
    bottle.response.set_header("Content-Disposition", f'attachment; filename="x6100-log-{stamp}.{ext}"')
    return body


# Web remote: control and spectrum through the GUI's local socket (src/remote_server.cpp,
# one JSON object per line), RX audio from PulseAudio. No PTT: only the apps transmit
# (X6100 Link).

REMOTE_CMDS = {"freq", "tune", "mode", "band", "pre", "att", "agc", "nb", "nr", "rfgain", "sql", "filter", "pwr",
               "app", "ft8_cq", "ft8_reply", "ft8_stop", "ft8_auto", "ft8_hold", "ft8_txfreq",
               "js8_send", "js8_cq", "js8_hb", "js8_stop", "wspr_txpct", "wspr_txnext", "sstv_tx", "sstv_stop",
               "app_base", "wefax_mode", "wefax_save", "wefax_clear", "wefax_tx", "wefax_stop",
               "rtty_send", "rtty_stop", "rtty_rate", "rtty_shift", "rtty_center", "rtty_rev",
               "psk_mode", "psk_freq", "psk_send", "psk_stop", "olivia_mode", "olivia_freq", "olivia_send", "olivia_stop",
               "jtty_freq", "jtty_send", "jtty_stop",
               "cw_send", "cw_stop", "cw_wpm", "cw_dec", "cw_clear", "cw_calls", "navtex_mode", "location",
               "field_screen", "field_ref", "field_start", "field_end", "field_log", "field_spot", "field_update",
               "qso_log", "nb_level", "nb_width", "nr_level", "agc_mode"}
# The radio RX audio as the GUI's decoders get it (src/audio.c), 16 bit mono
REMOTE_AUDIO_CMD = ["parec", "--device=alsa_input.platform-sound.stereo-fallback", "--format=s16le",
                    "--rate=12000", "--channels=1", "--latency-msec=100",
                    "--client-name=X6100 web remote", "--stream-name=Web remote RX"]
remote_audio_lock = threading.Lock()
remote_audio_busy = False


def remote_connect(timeout):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect(settings.REMOTE_SOCK)
    except OSError:
        sock.close()
        raise
    return sock


@app.route('/remote')
def remote():
    return bottle.template('remote')


# The remote protocol (X6100 Link and the Remote page) lives under LINK; it changes path when it changes,
# so an app of another protocol finds nothing here. The old paths only say so.
LINK = '/link/v2'
OLD_APP = "Update X6100 Link: this radio firmware needs a newer app"


@app.route('/api/remote', method=['GET', 'POST'])
@app.route('/api/remote/<rest:path>')
@app.route('/api/info')
def remote_gone(rest=None):
    return api_error(410, OLD_APP)


@app.post(LINK + '/cmd')
def remote_command():
    try:
        data = bottle.request.json
    except ValueError:
        data = None
    if not isinstance(data, dict) or data.get("cmd") not in REMOTE_CMDS:
        return api_error(400, "Invalid command")
    val = data.get("val")
    # The GUI checks the values; keep the line short and plain
    if not (val is None or (isinstance(val, int) and not isinstance(val, bool)) or (isinstance(val, str) and len(val) <= 8)):
        return api_error(400, "Invalid value")
    text = data.get("text")
    if not (text is None or (isinstance(text, str) and len(text) <= 200 and text.isascii() and text.isprintable())):
        return api_error(400, "Invalid text")
    cmd = {"cmd": data["cmd"], "val": val}
    if text is not None:
        cmd["text"] = text
    try:
        with remote_connect(3) as sock, sock.makefile("rb") as f:
            sock.sendall(json.dumps(cmd).encode() + b"\n")
            reply = json.loads(f.readline() or b"{}")
    except (OSError, ValueError):
        return api_error(503, "Radio GUI not reachable")
    if reply.get("t") != "ok":
        return api_error(409, reply.get("err", "Rejected by the radio"))
    return {"status": "OK"}


@app.get(LINK + '/info')
def remote_info():
    """Discovery for the phone app"""
    return {"radio": "x6100", "api": 2, "callsign": read_callsign(), "remote": os.path.exists(settings.REMOTE_SOCK)}


@app.get(LINK + '/events')
def remote_stream():
    """Server-sent events: the GUI's state and spectrum lines, until the page is closed."""
    bottle.response.content_type = 'text/event-stream'
    bottle.response.set_header('Cache-Control', 'no-cache')

    def events():
        try:
            sock = remote_connect(5)
        except OSError:
            yield b"retry: 3000\nevent: fail\ndata: Radio GUI not reachable\n\n"
            return
        # A closed page ends the generator at a yield: the GUI sees the socket close
        try:
            sock.sendall(b'{"cmd":"sub"}\n')
            yield b"retry: 3000\n\n"
            for line in sock.makefile("rb"):
                yield b"data: " + line.rstrip(b"\n") + b"\n\n"
        except OSError:   # GUI gone or silent for 5 s: the browser reconnects
            pass
        finally:
            sock.close()

    return events()


@app.get(LINK + '/audio')
def remote_audio():
    """Raw s16le mono 12 kHz. One listener; parec is killed when the page stops listening."""
    bottle.response.content_type = 'application/octet-stream'
    bottle.response.set_header('Cache-Control', 'no-store')

    def pcm():
        global remote_audio_busy
        with remote_audio_lock:
            if remote_audio_busy:
                raise bottle.HTTPError(409, "Another listener is connected: 1 listener at a time")
            remote_audio_busy = True
        proc = None
        try:
            env = dict(os.environ)
            env.setdefault("PULSE_SERVER", "unix:/var/run/pulse/native")
            try:
                proc = subprocess.Popen(REMOTE_AUDIO_CMD, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, env=env)
            except OSError:
                raise bottle.HTTPError(503, "parec is not available")
            fd, first = proc.stdout.fileno(), True
            while True:
                data = os.read(fd, 4800) if select.select([fd], [], [], 5)[0] else b""
                if not data:   # parec failed or stalled
                    if first:
                        raise bottle.HTTPError(503, "No audio from the radio")
                    break
                first = False
                yield data
        finally:
            if proc:
                proc.kill()
                proc.wait()
            with remote_audio_lock:
                remote_audio_busy = False

    return pcm()


# Equipo: la radio duplicada en la PC (CX8ANM / Cóndor Tecnológico).
# La pantalla la copia la GUI (src/mirror.c) en MIRROR_JPEG mientras MIRROR_FLAG
# esté fresco: se toca en cada pedido. Las teclas y perillas de la página se
# escriben en /dev/input/eventN, como si fueran las del frente: la GUI no ve
# ninguna diferencia, así anda en todas las pantallas y apps.

MIRROR_FLAG = "/tmp/x6100-mirror"
MIRROR_JPEG = "/tmp/x6100-screen.jpg"
EV_SYN, EV_KEY, EV_REL = 0, 1, 2
BTN_TRIGGER_HAPPY1 = 0x2c0
# Nombre en la página -> BTN_TRIGGER_HAPPYn (src/keypad.c)
MIRROR_KEYS = {"GEN": 1, "KEY": 2, "DFN": 3, "PTT": 4, "BAND_DOWN": 5, "BAND_UP": 6, "APP": 7, "MSG": 8,
               "DFL": 9, "AM": 10, "CW": 11, "SSB": 12, "F1": 13, "F2": 14, "F3": 15, "AB": 16, "PRE": 17,
               "ATU": 18, "F4": 19, "F5": 20, "VOL_PUSH": 21, "VM": 22, "AGC": 23, "FST": 24, "LOCK": 25,
               "MFK_PUSH": 27}
MIRROR_KEYPADS = ["/dev/input/event0", "/dev/input/event4"]
MIRROR_KNOBS = {"main": "/dev/input/event1", "vol": "/dev/input/event2", "mfk": "/dev/input/event3"}
MIRROR_PTT_MAX_S = 180   # PTT apretado desde la web: se suelta solo a los 3 minutos
mirror_lock = threading.Lock()
mirror_key_dev = {}      # código -> /dev/input/eventN que lo tiene
mirror_ptt_timer = None


def mirror_touch():
    try:
        with open(MIRROR_FLAG, "a"):
            pass
        os.utime(MIRROR_FLAG)
    except OSError:
        pass


def evdev_bits(path, ev, nbits):
    """Los códigos que un dispositivo de entrada tiene (EVIOCGBIT)"""
    import fcntl
    size = (nbits + 7) // 8
    buf = bytearray(size)
    req = (2 << 30) | (size << 16) | (ord("E") << 8) | (0x20 + ev)
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
    try:
        fcntl.ioctl(fd, req, buf, True)
    finally:
        os.close(fd)
    return {i for i in range(nbits) if buf[i // 8] >> (i % 8) & 1}


def evdev_write(path, events):
    """Eventos (tipo, código, valor) al dispositivo, y un SYN al final"""
    now = time.time()
    sec, usec = int(now), int((now % 1) * 1e6)
    data = b"".join(struct.pack("@llHHi", sec, usec, t, c, v) for t, c, v in events + [(EV_SYN, 0, 0)])
    fd = os.open(path, os.O_WRONLY | os.O_NONBLOCK)
    try:
        os.write(fd, data)
    finally:
        os.close(fd)


def mirror_key_path(code):
    if code not in mirror_key_dev:
        for path in MIRROR_KEYPADS:
            try:
                if code in evdev_bits(path, EV_KEY, 0x300):
                    mirror_key_dev[code] = path
                    break
            except OSError:
                continue
        else:
            return None
    return mirror_key_dev[code]


def mirror_key(name, down):
    global mirror_ptt_timer
    code = BTN_TRIGGER_HAPPY1 + MIRROR_KEYS[name] - 1
    path = mirror_key_path(code)
    if not path:
        raise OSError("tecla no encontrada")
    with mirror_lock:
        evdev_write(path, [(EV_KEY, code, 1 if down else 0)])
        if name == "PTT":
            if mirror_ptt_timer:
                mirror_ptt_timer.cancel()
                mirror_ptt_timer = None
            if down:
                mirror_ptt_timer = threading.Timer(MIRROR_PTT_MAX_S, mirror_key, ("PTT", False))
                mirror_ptt_timer.daemon = True
                mirror_ptt_timer.start()


def mirror_knob(name, steps):
    path = MIRROR_KNOBS[name]
    rel = sorted(evdev_bits(path, EV_REL, 16))
    with mirror_lock:
        evdev_write(path, [(EV_REL, rel[0] if rel else 0, steps)])


@app.route('/equipo')
def equipo():
    return bottle.template('equipo')


@app.get('/api/equipo/pantalla.jpg')
def equipo_frame():
    """La pantalla de la radio, una vez"""
    mirror_touch()
    for _ in range(30):   # la primera vez la GUI tarda un poco
        try:
            with open(MIRROR_JPEG, "rb") as f:
                data = f.read()
            if data:
                bottle.response.content_type = "image/jpeg"
                bottle.response.set_header("Cache-Control", "no-store")
                return data
        except OSError:
            pass
        time.sleep(0.1)
    return api_error(503, "La radio no manda la pantalla")


@app.get('/api/equipo/pantalla.mjpg')
def equipo_stream():
    """La pantalla en vivo (MJPEG): una imagen nueva cada vez que la GUI escribe una"""
    bottle.response.content_type = "multipart/x-mixed-replace; boundary=x6100frame"
    bottle.response.set_header("Cache-Control", "no-store")

    def frames():
        last, sent, touched = None, 0, 0
        deadline = time.time() + 6 * 3600
        while time.time() < deadline:
            now = time.time()
            if now - touched > 1:
                mirror_touch()
                touched = now
            try:
                st = os.stat(MIRROR_JPEG)
                stamp = (st.st_mtime_ns, st.st_size)
            except OSError:
                stamp = None
            if stamp and (stamp != last or now - sent > 3):
                try:
                    with open(MIRROR_JPEG, "rb") as f:
                        data = f.read()
                except OSError:
                    data = b""
                if data:
                    last, sent = stamp, now
                    yield (b"--x6100frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                           str(len(data)).encode() + b"\r\n\r\n" + data + b"\r\n")
            time.sleep(0.04)

    return frames()


@app.post('/api/equipo/tecla')
def equipo_key():
    data = bottle.request.json
    if not isinstance(data, dict) or data.get("tecla") not in MIRROR_KEYS or data.get("estado") not in (0, 1):
        return api_error(400, "Pedido inválido")
    try:
        mirror_key(data["tecla"], data["estado"] == 1)
    except OSError as e:
        return api_error(503, "No se pudo enviar la tecla: %s" % e)
    return {"status": "OK"}


@app.post('/api/equipo/perilla')
def equipo_knob():
    data = bottle.request.json
    steps = data.get("pasos") if isinstance(data, dict) else None
    if not isinstance(data, dict) or data.get("perilla") not in MIRROR_KNOBS or \
            not isinstance(steps, int) or isinstance(steps, bool) or not -50 <= steps <= 50 or steps == 0:
        return api_error(400, "Pedido inválido")
    try:
        mirror_knob(data["perilla"], steps)
    except OSError as e:
        return api_error(503, "No se pudo mover la perilla: %s" % e)
    return {"status": "OK"}


# Memories: MEM button slots (params.db, table `memory`) and named channels
# (channels.json, same format as the radio GUI, src/channels.c)
# Names and enum values are x6100_mode_t / x6100_agc_t from x6100_control
MODES = ["LSB", "LSB-D", "USB", "USB-D", "CW", "CWR", "AM", "NFM"]
AGCS = ["OFF", "SLOW", "FAST", "AUTO"]
MEM_SLOTS = range(1, 10)  # MEM_HKEY_MAX_ID in src/main_screen.h; 10 is the GUI's backup slot
FREQ_MIN, FREQ_MAX = 100000, 56000000  # the GUI HF range (HF_MIN_FREQ)
CHANNELS_MAX = 100
CHANNEL_NAME_RE = re.compile(r"[ -~]{0,31}")


def parse_entry(data, named):
    """Validated slot/channel dict from request JSON; ValueError with a message."""
    if not isinstance(data, dict):
        raise ValueError("Invalid request")
    freq = data.get("frequency")
    if isinstance(freq, bool) or not isinstance(freq, int) or not FREQ_MIN <= freq <= FREQ_MAX:
        raise ValueError("Frequency must be between 0.5 and 56 MHz")
    if data.get("mode") not in MODES:
        raise ValueError("Invalid mode")
    if data.get("agc") not in AGCS:
        raise ValueError("Invalid AGC")
    for key in ("pre", "att"):
        if not isinstance(data.get(key), bool):
            raise ValueError(f"Invalid {key.upper()}")
    entry = {"frequency": freq, "mode": data["mode"], "agc": data["agc"], "pre": data["pre"], "att": data["att"]}
    if named:
        name = data.get("name")
        if not isinstance(name, str) or not CHANNEL_NAME_RE.fullmatch(name):
            raise ValueError("Invalid name: up to 31 printable ASCII characters")
        entry = {"name": name, **entry}
    return entry


def api_error(status, msg):
    bottle.response.status = status
    return {"status": "error", "msg": msg}


@app.route('/memories')
def memories():
    return bottle.template('memories')


def read_slots(dbcon):
    vals = {}
    for id_, name, val in dbcon.execute("SELECT id, name, val FROM memory"):
        vals.setdefault(id_, {})[name] = val
    slots = []
    for id_ in MEM_SLOTS:
        v = vals.get(id_, {})
        if "vfoa_freq" in v:  # the GUI treats a slot without vfoa_freq as empty
            slots.append({"id": id_, "frequency": v["vfoa_freq"],
                          "mode": MODES[v.get("vfoa_mode", 0) % len(MODES)],
                          "agc": AGCS[v.get("vfoa_agc", 0) % len(AGCS)],
                          "pre": bool(v.get("vfoa_pre", 0)), "att": bool(v.get("vfoa_att", 0))})
        else:
            slots.append({"id": id_, "frequency": None})
    return {"slots": slots}


# The same memories and channels for X6100 Link, under LINK
@app.get(LINK + '/memories')
@app.get('/api/memories')
def get_memories(dbcon):
    return read_slots(dbcon)


@app.put(LINK + '/memories/<id:int>')
@app.put('/api/memories/<id:int>')
def set_memory(dbcon, id):
    if id not in MEM_SLOTS:
        return api_error(404, "No such slot")
    try:
        e = parse_entry(bottle.request.json, False)
    except ValueError as ex:
        return api_error(400, str(ex))
    vals = {"vfoa_freq": e["frequency"], "vfoa_mode": MODES.index(e["mode"]), "vfoa_agc": AGCS.index(e["agc"]),
            "vfoa_pre": int(e["pre"]), "vfoa_att": int(e["att"])}
    try:
        with dbcon:
            dbcon.executemany("INSERT OR REPLACE INTO memory(id, name, val) VALUES(?, ?, ?)",
                              [(id, k, v) for k, v in vals.items()])
    except sqlite3.Error as ex:
        return api_error(500, f"Can't save: {ex}")
    return read_slots(dbcon)


@app.delete(LINK + '/memories/<id:int>')
@app.delete('/api/memories/<id:int>')
def clear_memory(dbcon, id):
    if id not in MEM_SLOTS:
        return api_error(404, "No such slot")
    try:
        with dbcon:
            dbcon.execute("DELETE FROM memory WHERE id = ?", (id,))
    except sqlite3.Error as ex:
        return api_error(500, f"Can't save: {ex}")
    return read_slots(dbcon)


@app.route('/cw_messages')
def cw_messages():
    return bottle.template('cw_messages')


# Display: the screen options of Settings > Display, read and set live through the GUI socket
# (disp_get / disp_set, src/remote_server.cpp); the GUI checks names and ranges

def display_call(cmd):
    try:
        with remote_connect(3) as sock, sock.makefile("rb") as f:
            sock.sendall(json.dumps(cmd).encode() + b"\n")
            return json.loads(f.readline() or b"{}")
    except (OSError, ValueError):
        return None


@app.route('/display')
def display():
    return bottle.template('display')


@app.get('/api/display')
def display_get():
    reply = display_call({"cmd": "disp_get"})
    if not reply or reply.get("t") != "disp":
        return api_error(503, "Radio GUI not reachable")
    reply.pop("t", None)
    return reply


@app.post('/api/display')
def display_set():
    try:
        data = bottle.request.json
    except ValueError:
        data = None
    if not isinstance(data, dict) or not isinstance(data.get("name"), str) or len(data["name"]) > 23:
        return api_error(400, "Invalid option")
    cmd = {"cmd": "disp_set", "text": data["name"]}
    if data["name"] == "theme":
        if not isinstance(data.get("theme"), str) or len(data["theme"]) > 31:
            return api_error(400, "Invalid theme")
        cmd["theme"] = data["theme"]
    else:
        val = data.get("val")
        if not isinstance(val, int) or isinstance(val, bool):
            return api_error(400, "Invalid value")
        cmd["val"] = val
    reply = display_call(cmd)
    if reply is None:
        return api_error(503, "Radio GUI not reachable")
    if reply.get("t") != "ok":
        return api_error(409, reply.get("err", "Rejected by the radio"))
    return {"status": "OK"}


# Ethernet: the USB adapter's address, as Settings > Wireless > Ethernet. Read and applied through the
# GUI socket (eth_get / eth_set, src/remote_server.cpp); the GUI checks the values as its own page does

ETH_FIELDS = ("ip", "mask", "gw", "dns")


@app.route('/network')
def network():
    return bottle.template('network')


@app.get('/api/network')
def network_get():
    reply = display_call({"cmd": "eth_get"})
    if not reply or reply.get("t") != "eth":
        return api_error(503, "Radio GUI not reachable")
    reply.pop("t", None)
    return reply


@app.post('/api/network')
def network_set():
    try:
        data = bottle.request.json
    except ValueError:
        data = None
    if not isinstance(data, dict) or not isinstance(data.get("dhcp"), bool):
        return api_error(400, "Invalid mode")
    cmd = {"cmd": "eth_set", "dhcp": data["dhcp"]}
    for k in ETH_FIELDS:
        v = data.get(k, "")
        if not isinstance(v, str) or len(v) > 15:
            return api_error(400, "Invalid value")
        cmd[k] = v.strip()
    reply = display_call(cmd)
    if reply is None:
        return api_error(503, "Radio GUI not reachable")
    if reply.get("t") != "ok":
        return api_error(409, reply.get("err", "Rejected by the radio"))
    return {"status": "OK"}


CW_MSG_MAX = 64     # as the radio's editor (textarea_window)


def read_cw_messages(dbcon):
    return {"messages": [{"id": i, "text": t} for i, t in dbcon.execute("SELECT id, val FROM msg_cw ORDER BY id")],
            "max": CW_MSG_MAX}


def cw_text():
    """The text of the request: printable ASCII, 1 to CW_MSG_MAX characters."""
    data = bottle.request.json
    text = str(data.get("text", "")).strip() if isinstance(data, dict) else ""
    if not text or len(text) > CW_MSG_MAX or not all(" " <= c <= "~" for c in text):
        raise ValueError("1 to %d characters, letters, digits and symbols only" % CW_MSG_MAX)
    return text


@app.get('/api/cw_messages')
def get_cw_messages(dbcon):
    return read_cw_messages(dbcon)


@app.post('/api/cw_messages')
def add_cw_message(dbcon):
    try:
        text = cw_text()
        with dbcon:
            dbcon.execute("INSERT INTO msg_cw (val) VALUES (?)", (text,))
    except ValueError as ex:
        return api_error(400, str(ex))
    except sqlite3.Error as ex:
        return api_error(500, f"Can't save: {ex}")
    return read_cw_messages(dbcon)


@app.put('/api/cw_messages/<id:int>')
def set_cw_message(dbcon, id):
    try:
        text = cw_text()
        with dbcon:
            dbcon.execute("UPDATE msg_cw SET val = ? WHERE id = ?", (text, id))
    except ValueError as ex:
        return api_error(400, str(ex))
    except sqlite3.Error as ex:
        return api_error(500, f"Can't save: {ex}")
    return read_cw_messages(dbcon)


@app.delete('/api/cw_messages/<id:int>')
def delete_cw_message(dbcon, id):
    try:
        with dbcon:
            dbcon.execute("DELETE FROM msg_cw WHERE id = ?", (id,))
    except sqlite3.Error as ex:
        return api_error(500, f"Can't save: {ex}")
    return read_cw_messages(dbcon)


def read_channels():
    """Valid channels from channels.json; invalid ones are skipped like the GUI does."""
    try:
        with open(settings.CHANNELS_FILE) as f:
            items = json.load(f)["channels"]
        items = [parse_entry(x, True) for x in items if isinstance(x, dict)] if isinstance(items, list) else []
    except (OSError, ValueError, KeyError, TypeError):
        return []
    return items[:CHANNELS_MAX]


def write_channels(channels):
    tmp = settings.CHANNELS_FILE + ".tmp"
    with open(tmp, "w") as f:
        json.dump({"version": 1, "channels": channels}, f, indent="\t")
        f.write("\n")
    os.rename(tmp, settings.CHANNELS_FILE)


def channels_state():
    return {"channels": read_channels(), "max": CHANNELS_MAX}


def channels_edit(change):
    """Read, change (raises ValueError), write, return the new list."""
    channels = read_channels()
    try:
        change(channels)
    except ValueError as ex:
        return api_error(400, str(ex))
    try:
        write_channels(channels)
    except OSError as ex:
        return api_error(500, f"Can't save: {ex}")
    return channels_state()


def check_index(channels, i):
    if not 0 <= i < len(channels):
        raise ValueError("No such channel")


@app.get(LINK + '/channels')
@app.get('/api/channels')
def get_channels():
    return channels_state()


@app.post(LINK + '/channels')
@app.post('/api/channels')
def add_channel():
    def change(channels):
        if len(channels) >= CHANNELS_MAX:
            raise ValueError(f"At most {CHANNELS_MAX} channels")
        channels.append(parse_entry(bottle.request.json, True))
    return channels_edit(change)


@app.put(LINK + '/channels/<i:int>')
@app.put('/api/channels/<i:int>')
def set_channel(i):
    def change(channels):
        check_index(channels, i)
        channels[i] = parse_entry(bottle.request.json, True)
    return channels_edit(change)


@app.delete(LINK + '/channels/<i:int>')
@app.delete('/api/channels/<i:int>')
def delete_channel(i):
    def change(channels):
        check_index(channels, i)
        del channels[i]
    return channels_edit(change)


@app.post(LINK + '/channels/<i:int>/move')
@app.post('/api/channels/<i:int>/move')
def move_channel(i):
    def change(channels):
        check_index(channels, i)
        to = (bottle.request.json or {}).get("to")
        if isinstance(to, bool) or not isinstance(to, int) or not 0 <= to < len(channels):
            raise ValueError("Invalid position")
        channels.insert(to, channels.pop(i))
    return channels_edit(change)


@app.route('/cluster')
def cluster():
    return bottle.template('cluster')


@app.get('/api/cluster')
def get_cluster():
    return cluster_state()


@app.post('/api/cluster/servers')
def add_cluster_server():
    data = bottle.request.json or {}
    name = str(data.get("name", "")).strip()
    host = str(data.get("host", "")).strip()
    if not name or len(name) > 31 or "|" in name or "\r" in name or "\n" in name:
        return cluster_error(400, "Invalid name: 1-31 characters, no '|'")
    # Hosts the GUI accepts (cluster_parse_server): letters, digits, '.', '-'
    if not re.fullmatch(r"[A-Za-z0-9.-]{1,127}", host):
        return cluster_error(400, "Invalid host: use letters, digits, '.' and '-'")
    try:
        port = int(str(data.get("port", "")).strip())
    except ValueError:
        port = 0
    if not 0 < port < 65536:
        return cluster_error(400, "Invalid port: 1-65535")
    cur_host, cur_port, servers = read_cluster_conf()
    if len(servers) >= CLUSTER_MAX_SERVERS:
        return cluster_error(400, f"At most {CLUSTER_MAX_SERVERS} servers")
    if any(h == host and p == port for _, h, p in servers):
        return cluster_error(400, "This server is already in the list")
    return cluster_save(cur_host, cur_port, servers + [(name, host, port)])


def cluster_text_ok(text, max_len):
    return len(text) <= max_len and not any(c in text for c in "|\r\n")


@app.post('/api/cluster/filters')
def set_cluster_filters():
    data = bottle.request.json or {}
    modes = data.get("modes", [])
    if not isinstance(modes, list) or any(m not in CLUSTER_MODES for m in modes):
        return cluster_error(400, "Invalid modes")
    if not modes:
        return cluster_error(400, "Select at least one mode")
    try:
        max_age = int(data.get("max_age", 0))
    except (TypeError, ValueError):
        max_age = -1
    if not 0 <= max_age < 1440:
        return cluster_error(400, "Invalid max age")
    skip = ",".join(w.strip() for w in str(data.get("skip", "")).split(",") if w.strip())
    if not cluster_text_ok(skip, 120) or not re.fullmatch(r"[A-Za-z0-9/*#@,-]*", skip):
        return cluster_error(400, "Invalid spotters: letters, digits, '*', '#', '-', '/' separated by commas")
    bands = data.get("bands", [])
    activity = data.get("activity", [])
    if not isinstance(bands, list) or any(not isinstance(b, int) or b not in CLUSTER_BANDS for b in bands):
        return cluster_error(400, "Invalid bands")
    if not isinstance(activity, list) or any(a not in CLUSTER_ACTIVITY for a in activity):
        return cluster_error(400, "Invalid activity")
    host, port, servers = read_cluster_conf()
    opts = read_cluster_options()
    opts.update(modes=[m for m in CLUSTER_MODES if m in modes], max_age=max_age, skip=skip,
                bands=[b for b in CLUSTER_BANDS if b in bands], activity=[a for a in CLUSTER_ACTIVITY if a in activity])
    return cluster_save(host, port, servers, opts)


def cluster_alert_input(data):
    """(alert, None) from a request, or (None, error response)."""
    pattern = str(data.get("pattern", "")).strip().upper()
    if not pattern or len(pattern) > 31 or not re.fullmatch(r"[A-Z0-9/*]+", pattern):
        return None, cluster_error(400, "Invalid pattern: 1-31 characters, letters, digits, '/' and '*'")
    bands = data.get("bands", [])
    if not isinstance(bands, list):
        return None, cluster_error(400, "Invalid bands")
    try:
        bands = sorted({int(b) for b in bands}, reverse=True)
    except (TypeError, ValueError):
        return None, cluster_error(400, "Invalid bands")
    if any(b not in CLUSTER_BANDS for b in bands):
        return None, cluster_error(400, "Invalid bands")
    mode = str(data.get("mode", "")).strip().upper()
    if mode not in ("", "CW", "SSB", "DIG"):
        return None, cluster_error(400, "Invalid mode")
    return {"pattern": pattern, "bands": ",".join(str(b) for b in bands), "mode": mode,
            "enabled": data.get("enabled", True) is not False}, None


def cluster_alert_dup(alerts, a):
    return any(x["pattern"].upper() == a["pattern"] and x["bands"] == a["bands"] and x["mode"] == a["mode"] for x in alerts)


@app.post('/api/cluster/alerts')
def add_cluster_alert():
    a, err = cluster_alert_input(bottle.request.json or {})
    if err:
        return err
    host, port, servers = read_cluster_conf()
    opts = read_cluster_options()
    if len(opts["alerts"]) >= CLUSTER_MAX_ALERTS:
        return cluster_error(400, f"At most {CLUSTER_MAX_ALERTS} alerts")
    if cluster_alert_dup(opts["alerts"], a):
        return cluster_error(400, "This alert is already in the list")
    opts["alerts"].append(a)
    return cluster_save(host, port, servers, opts)


# Edit an alert, or turn it off and on ("enabled"; an alert off stays in the list as alert_off=)
@app.put('/api/cluster/alerts/<index:int>')
def edit_cluster_alert(index):
    a, err = cluster_alert_input(bottle.request.json or {})
    if err:
        return err
    host, port, servers = read_cluster_conf()
    opts = read_cluster_options()
    if not 0 <= index < len(opts["alerts"]):
        return cluster_error(404, "No such alert")
    if cluster_alert_dup(opts["alerts"][:index] + opts["alerts"][index + 1:], a):
        return cluster_error(400, "This alert is already in the list")
    opts["alerts"][index] = a
    return cluster_save(host, port, servers, opts)


@app.delete('/api/cluster/alerts/<index:int>')
def delete_cluster_alert(index):
    host, port, servers = read_cluster_conf()
    opts = read_cluster_options()
    if not 0 <= index < len(opts["alerts"]):
        return cluster_error(404, "No such alert")
    del opts["alerts"][index]
    return cluster_save(host, port, servers, opts)


@app.post('/api/cluster/alert-options')
def set_cluster_alert_options():
    data = bottle.request.json or {}
    host, port, servers = read_cluster_conf()
    opts = read_cluster_options()
    opts.update(popup=bool(data.get("popup")), beep=bool(data.get("beep")))
    return cluster_save(host, port, servers, opts)


@app.post('/api/cluster/use/<index:int>')
def use_cluster_server(index):
    _, _, servers = read_cluster_conf()
    if not 0 <= index < len(servers):
        return cluster_error(404, "No such server")
    _, host, port = servers[index]
    return cluster_save(host, port, servers)


@app.delete('/api/cluster/servers/<index:int>')
def delete_cluster_server(index):
    host, port, servers = read_cluster_conf()
    if not 0 <= index < len(servers):
        return cluster_error(404, "No such server")
    if servers[index][1:] == (host, port):
        return cluster_error(400, "This server is in use: choose another one first")
    del servers[index]
    return cluster_save(host, port, servers)


# Main routes

@app.route('/static/<filepath:path>')
def server_static(filepath):
    # The radio's WiFi is slow: send static files gzipped and let the browser keep them
    response = bottle.static_file(filepath, root=STATIC_PATH)
    if isinstance(response, bottle.HTTPError) or response.status_code != 200:
        return response
    response.set_header('Cache-Control', 'max-age=86400')
    if gzip and 'gzip' in bottle.request.headers.get('Accept-Encoding', ''):
        if filepath not in _gzip_cache:
            with open(STATIC_PATH.joinpath(filepath), 'rb') as f:
                _gzip_cache[filepath] = gzip.compress(f.read())
        response.body.close()
        response.body = _gzip_cache[filepath]
        response.set_header('Content-Encoding', 'gzip')
        response.set_header('Content-Length', str(len(response.body)))
    return response


_gzip_cache = {}


@app.route('/')
def home():
    return bottle.template('index')


@app.route('/bands')
def bands():
    return bottle.template('bands')


@app.route('/digital_modes')
def digital_modes():
    return bottle.template('digital_modes')


@app.route('/files/')
@app.route('/files/<filepath:path>')
@app.route('/files/<filepath:path>/')
def files(filepath=""):
    path = pathlib.Path(settings.FILEBROWSER_PATH) / filepath
    if path.is_file():
        os.sync()
        response = bottle.static_file(str(path.relative_to(settings.FILEBROWSER_PATH)), root=settings.FILEBROWSER_PATH, download=True)
        response.set_header("Cache-Control", "private, no-cache, no-store")
        return response
    else:
        dirs = []
        files = []
        for item in sorted(path.iterdir()):
            if item.is_dir():
                dirs.append(item.relative_to(path))
            else:
                files.append(item.relative_to(path))
        return bottle.template('files', dirs=dirs, files=files)

# Gallery: screen recordings, audio recordings and screenshots

GALLERY_DIRS = {"screenrec": ("screenrec", (".avi", ".mp4")), "rec": ("rec", (".mp3", ".wav")), "shots": ("", (".png",)),
                "sstv": ("sstv", (".png",)), "sstv_tx": ("sstv/tx_pics", (".png",)),
                "wefax": ("wefax", (".png",)), "wefax_tx": ("wefax/tx_pics", (".png",))}
GALLERY_NAME = re.compile(r'^[A-Za-z0-9][A-Za-z0-9._ -]*$')


def gallery_path(kind, name):
    """Real path of a gallery file, 404 for anything else (no separators, no dot dirs)."""
    sub, exts = GALLERY_DIRS.get(kind, (None, ()))
    if sub is None or not GALLERY_NAME.match(name) or not name.lower().endswith(exts):
        raise bottle.HTTPError(404, "Not found")
    root = os.path.realpath(os.path.join(settings.FILEBROWSER_PATH, sub))
    path = os.path.realpath(os.path.join(root, name))
    if os.path.dirname(path) != root or not os.path.isfile(path):
        raise bottle.HTTPError(404, "Not found")
    return root, path


def avi_first_frame(path):
    """First 00dc (JPEG) chunk of an AVI, or None."""
    with open(path, 'rb') as f:
        head = f.read(12)
        if head[:4] != b'RIFF' or head[8:12] != b'AVI ':
            return None
        while True:
            head = f.read(8)
            if len(head) < 8:
                return None
            fourcc, size = head[:4], int.from_bytes(head[4:], 'little')
            if fourcc == b'LIST':
                if f.read(4) != b'movi':    # go into movi, skip hdrl
                    f.seek(size - 4, os.SEEK_CUR)
            elif fourcc == b'00dc':
                return f.read(size)
            else:
                f.seek(size + (size & 1), os.SEEK_CUR)


@app.route('/gallery')
def gallery():
    return bottle.template('gallery')


@app.get('/api/gallery')
def get_gallery():
    items = []
    for kind, (sub, exts) in GALLERY_DIRS.items():
        if bottle.request.query.get("kind", kind) != kind:
            continue
        folder = os.path.join(settings.FILEBROWSER_PATH, sub)
        try:
            names = os.listdir(folder)
        except OSError:
            continue
        for name in names:
            if not GALLERY_NAME.match(name) or not name.lower().endswith(exts):
                continue
            try:
                st = os.stat(os.path.join(folder, name))
            except OSError:
                continue
            items.append({"kind": kind, "name": name, "size": st.st_size, "mtime": int(st.st_mtime)})
    items.sort(key=lambda x: (x["mtime"], x["name"]), reverse=True)   # same second: sstv-X_dn.png above sstv-X.png
    bottle.response.content_type = 'application/json'
    return json.dumps(items)


@app.get('/gallery/file/<kind>/<name>')
def gallery_file(kind, name):
    root, path = gallery_path(kind, name)
    os.sync()
    response = bottle.static_file(name, root=root, download=bool(bottle.request.query.get('dl')))
    response.set_header("Cache-Control", "private, no-cache, no-store")
    return response


@app.get('/gallery/thumb/<kind>/<name>')
def gallery_thumb(kind, name):
    root, path = gallery_path(kind, name)
    if kind in ("shots", "sstv", "sstv_tx", "wefax", "wefax_tx"):
        return bottle.static_file(name, root=root)
    jpeg = avi_first_frame(path) if kind == "screenrec" else None
    if not jpeg:
        raise bottle.HTTPError(404, "No frame")
    bottle.response.content_type = 'image/jpeg'
    bottle.response.set_header("Cache-Control", "private, max-age=3600")
    return jpeg


@app.delete('/api/gallery/<kind>/<name>')
def delete_gallery_file(kind, name):
    root, path = gallery_path(kind, name)
    os.remove(path)
    return {"status": "OK"}


GALLERY_BULK_MAX = 1000


def gallery_selection(kind, names, items):
    """[(kind, name)] from kind + names and/or items ("kind/name"); ValueError when malformed.
    Names themselves are checked by gallery_path."""
    if not isinstance(names, list) or not isinstance(items, list) or not all(isinstance(x, str) for x in names + items):
        raise ValueError("Invalid request")
    if names and kind not in GALLERY_DIRS:
        raise ValueError("Invalid kind")
    if any("/" not in x for x in items):
        raise ValueError("Invalid item")
    pairs = list(dict.fromkeys([(kind, n) for n in names] + [tuple(x.split("/", 1)) for x in items]))
    if not 0 < len(pairs) <= GALLERY_BULK_MAX:
        raise ValueError(f"Select 1 to {GALLERY_BULK_MAX} files")
    return pairs


@app.post('/api/gallery/delete')
def delete_gallery_files():
    """JSON {"kind", "names": [...]} or {"items": ["kind/name", ...]}; results per file."""
    try:
        data = bottle.request.json
        if not isinstance(data, dict):
            raise ValueError("Invalid request")
        pairs = gallery_selection(data.get("kind"), data.get("names", []), data.get("items", []))
    except ValueError as e:
        return api_error(400, str(e))
    deleted, failed = [], []
    for kind, name in pairs:
        try:
            os.remove(gallery_path(kind, name)[1])
            deleted.append({"kind": kind, "name": name})
        except bottle.HTTPError:
            failed.append({"kind": kind, "name": name, "msg": "Not found"})
        except OSError as e:
            failed.append({"kind": kind, "name": name, "msg": e.strerror or str(e)})
    return {"status": "OK", "deleted": deleted, "failed": failed}


class ZipSink:
    """Write-only file for zipfile: no seek, so it writes data descriptors and the
    zip can be sent while it's made (nothing in RAM or on the SD card)."""

    def __init__(self):
        self.parts = []

    def write(self, data):
        self.parts.append(bytes(data))
        return len(data)

    def flush(self):
        pass

    def take(self):
        data, self.parts = b"".join(self.parts), []
        return data


def zip_stream(files):
    """Stored (the files are already compressed) zip of [(name in the zip, path)], in pieces."""
    sink = ZipSink()
    with zipfile.ZipFile(sink, "w") as zf:
        for arcname, path in files:
            info = zipfile.ZipInfo.from_file(path, arcname, strict_timestamps=False)   # radio clock may be 1970
            with open(path, "rb") as src, zf.open(info, "w") as dst:
                for chunk in iter(lambda: src.read(65536), b""):
                    dst.write(chunk)
                    yield sink.take()
    yield sink.take()


@app.route('/api/gallery/zip', method=['GET', 'POST'])
def zip_gallery_files():
    """Fields kind + names (repeated) and/or items ("kind/name", repeated); files gone meanwhile are skipped."""
    params = bottle.request.params
    try:
        pairs = gallery_selection(params.get("kind"), params.getall("names"), params.getall("items"))
    except ValueError as e:
        return api_error(400, str(e))
    files = []
    for kind, name in pairs:
        try:
            files.append((kind, name, gallery_path(kind, name)[1]))
        except bottle.HTTPError:
            pass
    if not files:
        return api_error(404, "Not found")
    one = len({k for k, _, _ in files}) == 1
    os.sync()
    zip_name = f"x6100-{files[0][0] if one else 'all'}-{datetime.now().strftime('%Y%m%d-%H%M%S')}.zip"
    bottle.response.content_type = "application/zip"
    bottle.response.set_header("Content-Disposition", f'attachment; filename="{zip_name}"')
    bottle.response.set_header("Cache-Control", "private, no-cache, no-store")
    return zip_stream([(name if one else f"{kind}/{name}", path) for kind, name, path in files])


SSTV_TX_MAX = 4 * 1024 * 1024


@app.post('/api/sstv/tx_pics')
def upload_sstv_tx_pic():
    """A PNG for the radio's SSTV TX pictures (multipart field "file")"""
    return upload_tx_pic("sstv_tx")


@app.post('/api/wefax/tx_pics')
def upload_wefax_tx_pic():
    """A PNG for the radio's WeFax TX pictures (multipart field "file")"""
    return upload_tx_pic("wefax_tx")


def upload_tx_pic(kind):
    upload = bottle.request.files.get("file")
    if upload is None:
        return api_error(400, "No file")
    name = upload.raw_filename
    if not GALLERY_NAME.match(name) or not name.endswith(".png"):
        return api_error(400, "Invalid file name")
    data = upload.file.read(SSTV_TX_MAX + 1)
    if len(data) > SSTV_TX_MAX:
        return api_error(413, "Larger than 4 MB")
    if not data.startswith(b"\x89PNG"):
        return api_error(400, "Not a PNG")
    folder = os.path.join(settings.FILEBROWSER_PATH, GALLERY_DIRS[kind][0])
    os.makedirs(folder, exist_ok=True)
    if bottle.request.query.get("unique"):   # the gallery page: keep an existing file, name-1.png, name-2.png...
        # ponytail: check then replace, two uploads of the same name at the same moment can still clash
        stem, n = name[:-4], 1
        while os.path.exists(os.path.join(folder, name)):
            name, n = f"{stem}-{n}.png", n + 1
    tmp = os.path.join(folder, "." + name + ".tmp")   # hidden: the radio and the gallery skip it
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, os.path.join(folder, name))
    return {"status": "OK", "name": name}


# Backup / restore routes
# A backup is a tar(.gz) with manifest.json and a fixed set of member names. An uploaded
# archive is never extracted by tarfile: only whitelisted names are copied, to a work dir.

BACKUP_MAX_BYTES = 50 * 1024 * 1024   # upload size and total unpacked size
BACKUP_MAX_MEMBERS = 200
BACKUP_TEXT_MAX = 1024 * 1024         # any member that is not a .db or .mp3
BACKUP_NAME = r"[A-Za-z0-9][A-Za-z0-9 ._-]{0,59}"
BACKUP_DIRS = (("voice", "msg/", re.compile("msg/" + BACKUP_NAME + r"\.mp3")),
               ("wifi", "wifi/", re.compile("wifi/" + BACKUP_NAME + r"\.nmconnection")))
BACKUP_GUI_PARTS = ("settings", "memories", "qso", "cw")   # files the GUI owns: restore with it stopped
TAR_ERRORS = (tarfile.TarError, EOFError, OSError) + ((zlib.error,) if zlib else ())

backup_lock = threading.Lock()


class BackupError(Exception):
    pass


def backup_files():
    """Fixed members: name -> (part, path on the radio). memory_slots.json is made from params.db."""
    return {"params.db": ("settings", settings.DB_PATH),
            "channels.json": ("memories", settings.CHANNELS_FILE),
            "memory_slots.json": ("memories", settings.DB_PATH),
            "cluster.conf": ("cluster", settings.CLUSTER_CONF),
            "wfview.conf": ("wfview", settings.WFVIEW_CONF),
            "qso_log.db": ("qso", settings.QSO_LOG_DB),
            "cw_trainer.json": ("cw", settings.CW_TRAINER_FILE)}


def backup_member(name):
    """(part, path on the radio) of a member name, None when it isn't a known one."""
    if name in backup_files():
        return backup_files()[name]
    for part, prefix, pattern in BACKUP_DIRS:
        if pattern.fullmatch(name):
            return part, os.path.join(settings.VOICE_DIR if part == "voice" else settings.WIFI_DIR, name[len(prefix):])
    return None


def backup_current():
    """member -> path of everything the radio has now."""
    files = {n: p for n, (_, p) in backup_files().items() if os.path.isfile(p)}
    for part, prefix, pattern in BACKUP_DIRS:
        d = settings.VOICE_DIR if part == "voice" else settings.WIFI_DIR
        try:
            names = sorted(os.listdir(d))
        except OSError:
            continue
        files.update({prefix + n: os.path.join(d, n) for n in names if pattern.fullmatch(prefix + n)})
    return files


def plural(n, word):
    return f"{n} {word}" + ("" if n == 1 else "s")


def fmt_size(n):
    return f"{n / 1048576:.1f} MB" if n >= 1048576 else f"{max(n // 1024, 1)} kB" if n else "0 kB"


def read_text(path):
    with open(path, errors="replace") as f:
        return f.read(BACKUP_TEXT_MAX)


def memory_rows(path):
    """[[id, name, val]] of the MEM slots, from memory_slots.json or from a params.db."""
    if path.endswith(".json"):
        with open(path) as f:
            return json.load(f)["rows"]
    con = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    try:
        return [list(r) for r in con.execute("SELECT id, name, val FROM memory")]
    finally:
        con.close()


def describe_backup(files, date=None):
    """part -> one line summary of the members in files (member -> path); date overrides the file time."""
    def when(member):
        return date or datetime.fromtimestamp(os.path.getmtime(files[member])).strftime("%Y-%m-%d %H:%M")

    def count(member, sql):
        con = sqlite3.connect(f"file:{files[member]}?mode=ro", uri=True)
        try:
            return con.execute(sql).fetchone()[0]
        finally:
            con.close()

    out = {}
    try:
        if "params.db" in files:
            out["settings"] = when("params.db")
        if "channels.json" in files or "memory_slots.json" in files:
            slots = len({r[0] for r in memory_rows(files["memory_slots.json"]) if r[1] == "vfoa_freq" and r[0] in MEM_SLOTS}) \
                if "memory_slots.json" in files else 0
            try:
                with open(files["channels.json"]) as f:
                    chans = len(json.load(f)["channels"])
            except (KeyError, OSError, ValueError, TypeError):
                chans = 0
            out["memories"] = f"{plural(slots, 'slot')}, {plural(chans, 'channel')}"
        if "cluster.conf" in files:
            lines = read_text(files["cluster.conf"]).splitlines()
            servers = sum(1 for x in lines if x.startswith("server="))
            alerts = sum(1 for x in lines if x.startswith("alert"))
            out["cluster"] = plural(servers, "server") + (f", {plural(alerts, 'alert')}" if alerts else "")
        if "wfview.conf" in files:
            on = "enabled=1" in read_text(files["wfview.conf"]).splitlines()
            out["wfview"] = "server enabled" if on else "server disabled"
        if "qso_log.db" in files:
            out["qso"] = plural(count("qso_log.db", "SELECT count(*) FROM qso_log"), "QSO")
        voice = [n for n in files if n.startswith("msg/")]
        if voice:
            size = sum(os.path.getsize(files[n]) for n in voice)
            out["voice"] = f"{plural(len(voice), 'message')}, {fmt_size(size)}"
        if "cw_trainer.json" in files:
            out["cw"] = when("cw_trainer.json")
        wifi = [n for n in files if n.startswith("wifi/")]
        if wifi:
            out["wifi"] = plural(len(wifi), "network")
    except (OSError, ValueError, KeyError, TypeError, sqlite3.Error):
        pass   # a part that can't be read just has no summary
    return out


def check_sqlite(path, table):
    try:
        con = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
        try:
            ok = con.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
            ok = ok and con.execute("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?", (table,)).fetchone()
        finally:
            con.close()
    except sqlite3.Error:
        ok = False
    if not ok:
        raise BackupError(f"{os.path.basename(path)} in the file is not a valid database")


def check_member(name, path):
    """Content check of one unpacked member; raises BackupError."""
    try:
        if name == "params.db":
            check_sqlite(path, "params")
        elif name == "qso_log.db":
            check_sqlite(path, "qso_log")
        elif name == "memory_slots.json":
            with open(path) as f:
                rows = json.load(f)["rows"]
            if not isinstance(rows, list) or len(rows) > 1000 or not all(
                    isinstance(r, list) and len(r) == 3 and isinstance(r[0], int) and isinstance(r[1], str) and len(r[1]) <= 32
                    and isinstance(r[2], (int, float)) and not isinstance(r[2], bool) for r in rows):
                raise ValueError
        elif name == "channels.json":
            with open(path) as f:
                if not isinstance(json.load(f)["channels"], list):
                    raise ValueError
        elif name == "cw_trainer.json":
            with open(path) as f:
                json.load(f)
        elif name.endswith((".conf", ".nmconnection")):
            with open(path, "rb") as f:
                data = f.read()
            if b"\0" in data or (name.endswith(".nmconnection") and b"[connection]" not in data):
                raise ValueError
    except (OSError, ValueError, KeyError, TypeError):
        raise BackupError(f"{name} in the file is not valid")


def unpack_backup(archive, stage):
    """Checks a backup and copies its members into stage. Returns the manifest."""
    with open(archive, "rb") as f:
        gz = f.read(2) == b"\x1f\x8b"
    if gz and not zlib:
        raise BackupError("This radio image can't read .tar.gz backups: upload a plain .tar")
    try:
        with tarfile.open(archive, "r:gz" if gz else "r:") as tf:
            seen, total = set(), 0
            for m in tf:
                if len(seen) >= BACKUP_MAX_MEMBERS:
                    raise BackupError("Too many items in the file")
                if m.name != "manifest.json" and not backup_member(m.name):
                    raise BackupError(f"Unexpected item in the file: {m.name[:60]!r}")
                if m.type not in (tarfile.REGTYPE, tarfile.AREGTYPE) or m.name in seen:
                    raise BackupError(f"Unexpected item in the file: {m.name[:60]!r}")
                total += m.size
                if total > BACKUP_MAX_BYTES or (m.size > BACKUP_TEXT_MAX and not m.name.endswith((".db", ".mp3"))):
                    raise BackupError("The file is too large")
                seen.add(m.name)
                dst = os.path.join(stage, m.name)
                os.makedirs(os.path.dirname(dst), mode=0o700, exist_ok=True)
                with tf.extractfile(m) as src, open(dst, "wb") as out:
                    shutil.copyfileobj(src, out)
                    out.flush()
                    os.fsync(out.fileno())
    except TAR_ERRORS as e:
        raise BackupError(f"Not a valid backup file ({e.__class__.__name__})")
    try:
        with open(os.path.join(stage, "manifest.json")) as f:
            manifest = json.load(f)
        if manifest["version"] != 1 or not isinstance(manifest["date"], str):
            raise ValueError
    except (OSError, ValueError, KeyError, TypeError):
        raise BackupError("Not an X6100 backup: manifest.json is missing or has an unknown version")
    for name in seen - {"manifest.json"}:
        check_member(name, os.path.join(stage, name))
    return manifest


def backup_error(status, msg):
    bottle.response.status = status
    return {"status": "error", "msg": msg}


@app.route('/backup')
def backup():
    return bottle.template('backup')


@app.get('/api/backup')
def get_backup():
    return {"parts": describe_backup(backup_current())}


@app.get('/api/backup/download')
def download_backup():
    parts = [p for p in bottle.request.query.get("parts", "").split(",") if p]
    files = {n: p for n, p in backup_current().items() if backup_member(n)[0] in parts}
    if not files:
        return backup_error(400, "Nothing to back up")
    now = datetime.now()
    os.makedirs(settings.BACKUP_WORK, mode=0o700, exist_ok=True)
    out = tempfile.TemporaryFile(dir=settings.BACKUP_WORK)
    try:
        with tempfile.TemporaryDirectory(dir=settings.BACKUP_WORK) as tmp:
            for name, path in list(files.items()):
                copy = os.path.join(tmp, name.replace("/", "_"))
                if name == "memory_slots.json":
                    with open(copy, "w") as f:
                        json.dump({"rows": memory_rows(path)}, f)
                elif name.endswith(".db"):   # consistent copy while the GUI has it open
                    src, dst = sqlite3.connect(f"file:{path}?mode=ro", uri=True), sqlite3.connect(copy)
                    src.backup(dst)
                    src.close()
                    dst.close()
                else:
                    shutil.copyfile(path, copy)
                files[name] = copy
            callsign = read_callsign()
            call = re.sub(r"[^A-Za-z0-9_-]", "-", callsign)
            manifest = {"version": 1, "date": now.strftime("%Y-%m-%d %H:%M"), "callsign": callsign,
                        "parts": describe_backup(files, now.strftime("%Y-%m-%d %H:%M"))}
            with tarfile.open(fileobj=out, mode="w:gz" if zlib else "w") as tf:
                data = json.dumps(manifest, indent=1).encode()
                info = tarfile.TarInfo("manifest.json")
                info.size, info.mtime = len(data), int(now.timestamp())
                tf.addfile(info, io.BytesIO(data))
                for name, copy in files.items():
                    info = tf.gettarinfo(copy, name)
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    info.mode = 0o600
                    with open(copy, "rb") as f:
                        tf.addfile(info, f)
    except (OSError, sqlite3.Error, ValueError) as e:
        out.close()
        return backup_error(500, f"Can't make the backup: {e}")
    out.seek(0, os.SEEK_END)
    size = out.tell()
    out.seek(0)
    ext = "tar.gz" if zlib else "tar"
    name = f"x6100-backup-{call + '-' if call else ''}{now.strftime('%Y-%m-%d')}.{ext}"
    bottle.response.content_type = "application/gzip" if zlib else "application/x-tar"
    bottle.response.set_header("Content-Disposition", f'attachment; filename="{name}"')
    bottle.response.set_header("Content-Length", str(size))
    return out


def backup_stage():
    return os.path.join(settings.BACKUP_WORK, "stage")


@app.post('/api/backup/upload')
def upload_backup():
    length = bottle.request.content_length
    if length <= 0 or length > BACKUP_MAX_BYTES:
        return backup_error(413, "The file is empty or larger than 50 MB")
    if not backup_lock.acquire(blocking=False):
        return backup_error(409, "Another backup operation is running")
    try:
        stage = backup_stage()
        shutil.rmtree(stage, ignore_errors=True)
        os.makedirs(stage, mode=0o700)
        archive = os.path.join(settings.BACKUP_WORK, "upload.tar")
        try:
            with open(archive, "wb") as f:
                left, src = length, bottle.request.environ["wsgi.input"]
                while left > 0:
                    chunk = src.read(min(left, 65536))
                    if not chunk:
                        raise BackupError("Upload interrupted")
                    f.write(chunk)
                    left -= len(chunk)
            manifest = unpack_backup(archive, stage)
        except (BackupError, OSError) as e:
            shutil.rmtree(stage, ignore_errors=True)
            return backup_error(400, str(e))
        finally:
            try:
                os.remove(archive)
            except OSError:
                pass
        in_file = describe_backup(stage_files(), manifest["date"][:32])
        return {"date": manifest["date"][:32], "callsign": str(manifest.get("callsign", ""))[:16],
                "parts": {p: {"file": s, "now": describe_backup(backup_current()).get(p, "")} for p, s in in_file.items()}}
    finally:
        backup_lock.release()


def stage_files():
    """member -> path of what the last upload unpacked."""
    files = {}
    for root, _, names in os.walk(backup_stage()):
        for n in names:
            path = os.path.join(root, n)
            member = os.path.relpath(path, backup_stage()).replace(os.sep, "/")
            if member != "manifest.json":
                files[member] = path
    return files


def install_file(src, dst, mode):
    """Atomic copy: tmp file in the target directory, fsync, rename."""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    tmp = dst + ".tmp"
    with open(src, "rb") as i, open(tmp, "wb") as o:
        shutil.copyfileobj(i, o)
        o.flush()
        os.fsync(o.fileno())
    os.chmod(tmp, mode)
    os.replace(tmp, dst)


def save_safety_copy(member, path):
    if not os.path.isfile(path):
        return
    dst = os.path.join(settings.BACKUP_SAFETY, member)
    os.makedirs(os.path.dirname(dst), mode=0o700, exist_ok=True)
    if member.endswith(".db"):
        try:
            src, out = sqlite3.connect(path), sqlite3.connect(dst)
            src.backup(out)
            src.close()
            out.close()
            return
        except sqlite3.Error:
            pass   # not a database (corrupt): plain copy
    shutil.copyfile(path, dst)


def apply_memory_slots(path):
    rows = [r for r in memory_rows(path) if r[0] in MEM_SLOTS]
    con = sqlite3.connect(settings.DB_PATH)
    try:
        with con:
            con.execute("DELETE FROM memory WHERE id BETWEEN ? AND ?", (MEM_SLOTS[0], MEM_SLOTS[-1]))
            con.executemany("INSERT OR REPLACE INTO memory(id, name, val) VALUES(?, ?, ?)", rows)
    finally:
        con.close()


def gui_service(action):
    subprocess.run([settings.GUI_INIT, action], capture_output=True, timeout=60)


@app.post('/api/backup/restore')
def restore_backup():
    parts = (bottle.request.json or {}).get("parts")
    if not isinstance(parts, list) or not parts:
        return backup_error(400, "Nothing selected")
    if not backup_lock.acquire(blocking=False):
        return backup_error(409, "Another backup operation is running")
    try:
        files = stage_files()
        todo = {}   # member -> path on the radio, in the order of params.db first
        for name, path in sorted(files.items(), key=lambda x: x[0] != "params.db"):
            member = backup_member(name)
            if member and member[0] in parts:
                todo[name] = member[1]
        if not todo:
            return backup_error(409, "No uploaded backup: choose the file again")
        gui = any(backup_member(n)[0] in BACKUP_GUI_PARTS for n in todo)
        if gui:
            try:
                gui_service("stop")
            except (OSError, subprocess.SubprocessError) as e:
                return backup_error(500, f"Can't stop the radio screen: {e}")
            time.sleep(1)
        failed, restored = {}, []
        try:
            for name, dst in todo.items():
                part = backup_member(name)[0]
                try:
                    if name == "memory_slots.json":
                        if "params.db" not in todo:   # else already saved, and dst is the new file
                            save_safety_copy("params.db", dst)
                        apply_memory_slots(files[name])
                    else:
                        save_safety_copy(name, dst)
                        install_file(files[name], dst, 0o600 if part == "wifi" else 0o644)
                        if name.endswith(".db"):
                            for ext in ("-wal", "-shm"):   # stale journal of the replaced file
                                try:
                                    os.remove(dst + ext)
                                except OSError:
                                    pass
                    if part not in restored:
                        restored.append(part)
                except (OSError, sqlite3.Error, ValueError, KeyError) as e:
                    failed[part] = str(e)
            os.sync()
            if "wifi" in restored:
                try:
                    subprocess.run(["nmcli", "connection", "reload"], capture_output=True, timeout=20)
                except (OSError, subprocess.SubprocessError):
                    pass
        finally:
            if gui:
                try:
                    gui_service("start")
                except (OSError, subprocess.SubprocessError) as e:
                    failed["gui"] = f"Can't start the radio screen: {e}"
            shutil.rmtree(backup_stage(), ignore_errors=True)
        restored = [p for p in restored if p not in failed]
        return {"restored": restored, "failed": failed, "gui_restarted": gui}
    finally:
        backup_lock.release()


# Timezone routes


@app.route('/time')
def time_editor():
    return bottle.template('time')


@app.get('/api/get_time')
def get_time():
    tz = timezone(timedelta())
    server_time = datetime.now(tz).isoformat()
    bottle.response.content_type = 'application/json'
    return {"server_time": server_time}


def update_time_by_ntp(server_address):
    ntp_args = ["ntpdate", "-u", server_address]
    p = subprocess.Popen(
        ntp_args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        _, errs = p.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        p.kill()
        _, errs = p.communicate()
        bottle.response.status = 500
        return {"status": "error", "msg": "NTP update timeout"}

    if p.returncode != 0:
        bottle.response.status = 500
        return {"status": "error", "msg": f"NTP update failed: {errs.decode()}"}

    return {"status": "success", "msg": "NTP update successful"}


@app.post('/api/update_time')
def update_time():
    data = bottle.request.json

    update_mode = data.get("update_mode")
    if not update_mode:
        bottle.response.status = 400
        return {"status": "error", "msg": "update_mode is required"}

    if update_mode == "ntp":
        server_address = data.get("server_address")
        return update_time_by_ntp(server_address)

    elif update_mode == "manual":
        manual_time = data.get("manual_time")
        if not manual_time:
            bottle.response.status = 400
            return {"status": "error", "msg": "manual_time is required"}

        try:
            # Update system time manually
            manual_time = datetime.strptime(manual_time, "%Y-%m-%d %H:%M:%S")
            subprocess.run(
                ["date", "-s", manual_time.strftime("%Y-%m-%d %H:%M:%S")], check=True)
            return {"status": "success", "msg": "Server time updated manually"}
        except Exception as e:
            bottle.response.status = 500
            return {"status": "error", "msg": f"Failed to set manual time: {str(e)}"}

    else:
        bottle.response.status = 400
        return {"status": "error", "msg": f"unknown update_mode: {update_mode}"}


@app.get('/api/get_timezone')
def get_timezone():
    """Get the current server timezone."""
    try:
        p = subprocess.run(["realpath", "/etc/localtime"],
                           stdout=subprocess.PIPE, check=True)
        timezone_path = p.stdout.decode().strip()
        tz_list = timezone_path.split("/posix/")
        if len(tz_list) < 2:
            tz_list = timezone_path.split("/zoneinfo/")
        tz = tz_list[-1]
        return {"timezone": tz}
    except Exception as e:
        bottle.response.status = 500
        return {"status": "error", "msg": f"Failed to fetch timezone: {str(e)}"}


@app.post('/api/set_timezone')
def set_timezone():
    """Set the server timezone."""
    data = bottle.request.json
    timezone = data.get("timezone")
    if not timezone:
        bottle.response.status = 400
        return {"status": "error", "msg": "Timezone is required"}

    target_tz = f"/usr/share/zoneinfo/{timezone}"
    if not os.path.exists(target_tz):
        bottle.response.status = 400
        return {"status": "error", "msg": f"Invalid timezone: {timezone}"}

    try:
        subprocess.run(["ln", "-sf", target_tz, "/etc/localtime"], check=True)
        return {"status": "success", "msg": "Timezone updated successfully"}
    except subprocess.CalledProcessError as e:
        bottle.response.status = 500
        return {"status": "error", "msg": f"Failed to set timezone: {str(e)}"}
