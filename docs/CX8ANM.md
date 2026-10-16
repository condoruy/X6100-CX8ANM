# Versión CX8ANM (Cóndor Tecnológico)

Cambios sobre el firmware de IU1WXY. Todo el crédito del firmware original es de
Giovanni Ricciardi IU1WXY, R1CBU, R2RFE y la comunidad X6100 (ver README.md).

## Idioma

- **Ajustes → Pantalla → Idioma**: English / Español. Se aplica al instante.
- La interfaz, los mensajes y las pantallas de los modos (FT8, JS8, SSTV,
  WeFax, NAVTEX, PSK, Olivia, JTTY, WSPR, CW, SOTA/POTA) están en español.
- Las fuentes tienen á é í ó ú ü ñ ¡ ¿ (también la Sony Sketch, con sus letras).
- La página web de la radio también está en español.

Para corregir o agregar una traducción:

1. Editar `tools/i18n/es.txt` (inglés `TAB` español).
2. `python3 tools/i18n/build.py` (genera `src/i18n_es.inc` y revisa los `%`).
3. Si el texto se arma con `snprintf`, `python3 tools/i18n/wrap.py src/archivo.c`.

## Equipo en la PC

Página **Equipo** en la web de la radio (`http://IP-de-la-radio/equipo`): la
pantalla en vivo, todas las teclas, las perillas VOL, MFK y sintonía, el PTT
(mantener apretado, se suelta solo a los 3 minutos) y el audio de recepción.
El audio necesita abrir la página con `https://` (aceptar el certificado una vez).

## Punto de acceso WiFi (sin router)

**Ajustes → Inalámbrico → Punto de acceso**. La radio crea la red
`X6100-<indicativo>`; la clave se ve en la misma pantalla y se puede cambiar en
`/mnt/hotspot.conf` de la tarjeta SD. Conectar la PC a esa red y abrir
`http://192.168.73.1/equipo`. Mientras está encendido, la WiFi de casa queda en pausa.

## Logo de inicio

`boot/logo.png` (800×480), fuente editable en `boot/logo.svg`.
