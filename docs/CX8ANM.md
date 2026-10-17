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

## Cluster: aviso de actividad en mi banda
Ajustes > Cluster > VISTA DEL CLUSTER > "Avisar spots en mi banda" (activado por defecto).
Cuando aparece un spot nuevo (menos de 5 minutos) en la banda donde está el VFO, sale el
cartel "SPOT NUEVO EN SU BANDA" con el indicativo, la frecuencia y el modo. Apretando la
perilla MFK mientras se ve el cartel, la radio va directo a esa frecuencia. Respeta los
filtros de modos/actividad del cluster y no repite el mismo indicativo en 15 minutos.

## NAVTEX y DSC guardados solos
Todo lo que se recibe en la app NAVTEX queda en /mnt/navtex, un archivo por día (UTC):
navtex-AAAAMMDD.txt y dsc-AAAAMMDD.txt. Se descargan desde la web: Galería > "NAVTEX / DSC
recibidos" (o la página Archivos, carpeta navtex). El botón Guardar sigue funcionando igual.

## Operación de campo: lista de sitios (SOTA, POTA, WWBOTA, HOTA)
- La imagen ya trae las listas de referencias; la primera vez que entra a un programa se copian
  a /mnt/field y aparecen los 30 sitios más cercanos (MFK elige, apretar = usar ese sitio).
- La posición sale del GPS, del teléfono (X6100 Link) o del localizador QTH. Si no hay
  localizador cargado, queda GF15WC (Montevideo); cámbielo en Ajustes > General > QTH.
- Si no hay lista, se baja sola apenas hay WiFi. Para actualizarla: botón "Actualizar lista"
  (página 2). "Ref..." permite escribir una referencia a mano.
