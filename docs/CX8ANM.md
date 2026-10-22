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

## Bandas (Región 2 / Uruguay)
- Radioaficionado: 2200m, 630m, 160m (1,800-2,000), 80m (3,500-4,000), 60m (5,3515-5,3665),
  40m (7,000-7,300), 30m, 20m, 17m, 15m, 12m, 10m, 6m (50-54) y CB.
- AM comercial (onda media 530-1710 kHz) como banda propia, en AM.
- Onda corta de radiodifusión (120m a 11m): se muestran como referencia al sintonizar.
- FM comercial (88-108 MHz): el X6100 no recibe por encima de 55 MHz.
- Las radios ya usadas reciben las bandas nuevas solas al actualizar (migración 8).

## NAVTEX / DSC: escaneo de frecuencias marcadas
1. Frec. → elegir una estación o canal (NAVTEX o DSC) → página 2 → "Marcar frec.". Repetir con otras.
   Las marcadas aparecen con [ESCANEO] en la lista (se guardan en /mnt/navtex/scan.conf).
2. "Escanear" recorre las marcadas: NAVTEX 6 s cada una, DSC 2 s (cambia solo entre NAVTEX y DSC).
3. Si detecta NAVTEX (texto que decodifica) o DSC (patrón de puntos o llamada), se queda escuchando y
   decodificando; vuelve a escanear 30 s (NAVTEX) o 15 s (DSC) después de que termina.
4. Todo lo recibido se guarda solo (archivos del día). Tocar Frec. o Modo detiene el escaneo.
El receptor es uno solo: escucha una frecuencia por vez, el escaneo las va alternando.

## Aviso de versión nueva
Al conectarse a internet, la radio compara su versión (1.0.N) con la última compilación que salió
bien en github.com/condoruy/X6100-CX8ANM (Actions). Solo avisa cuando hay una compilación propia
más nueva. Ya no consulta x6100-link.it. Se apaga en Ajustes > Info > Aviso de actualización.

## Actualizar desde la radio
Cada compilación de GitHub queda publicada en Releases (v1.0.N) con la imagen y el paquete
update.tar.gz. En la radio, con internet: Ajustes > Info > Actualizar... (busca), y si hay una
versión nueva, tocar otra vez: baja el paquete a /mnt/update, lo verifica (sha256), lo instala y
reinicia. Se conservan las redes WiFi, los emparejamientos Bluetooth, las claves SSH y todo /mnt
(ajustes, logs, grabaciones). No apagar la radio mientras dice "Instalando". Si algo saliera mal,
se vuelve a grabar la imagen con balenaEtcher como siempre.

## Volumen, reloj y página Equipo
- El volumen se muestra de 0 a 100 % (la placa va de 0 a 55: 100 % es el máximo real).
- Reloj: cada 15 s muestra 10 s la hora local y 5 s la UTC (dice "UTC" en lugar de los
  segundos). Ajustes > General > FECHA / HORA > "Mostrar también UTC".
- Equipo (web): cruz "NAVEGAR MENÚS Y LISTAS" (▲▼◀▶, OK, Atrás) y flechas ← → del teclado;
  un clic en los botones de abajo de la imagen los aprieta. "Imagen: Mínima/Liviana/Normal/
  Fluida" elige cuántas imágenes por segundo manda la radio: menos imágenes, menos tráfico
  WiFi y menos ruido en el receptor (por defecto Liviana, 2 por segundo).
- Cada actualización deja su historial en /mnt/update/update.log (web > Archivos > update).

## Modo ahorro y banda base
- Ajustes > Info > Modo ahorro: el procesador baja la velocidad en reposo (schedutil en vez de
  performance) y la pantalla se atenúa a los 15 s como mucho. Apagado por defecto. Para medir:
  /mnt/battery_log.csv (vbat, batcap, cpu_mhz cada 30 s).
- Ajustes > Info > Banda base: muestra "(OK)" si es la última oficial conocida de Xiegu (V1.1.8,
  15/09/2024) o "- Xiegu 1.1.8" si hay una más nueva. Se actualiza con el procedimiento oficial
  de Xiegu (fuente de 13,8 V). La versión conocida está en src/power_save.c (BASEBAND_LATEST).

## Temas estilo equipo de alta gama y transceptores reconocidos

Temas disponibles en **Ajustes → Display → INTERFACE → Theme**, con la estética de las pantallas de los
equipos de base (colores, teclas, recuadro del modo, S-meter y paleta de la cascada), cada uno con su
variante estándar y su variante **Deck** (disposición de alta densidad con frecuencia gigante, escalas S/SWR/ALC al costado y teclas planas):

| Tema | Estilo |
|---|---|
| **Pro Azul** / **Pro Azul Deck** | negro con teclas grises en degradé, azul para lo activo, modo en recuadro, cascada azul → amarillo → rojo |
| **Pro Ambar** / **Pro Ambar Deck** | visor ámbar sobre negro, todo en tonos cálidos, cascada "fuego" |
| **Pro Verde** / **Pro Verde Deck** | verde fósforo de los visores clásicos, plano, sin degradés |
| **Pro Grafito** / **Pro Grafito Deck** | gris grafito de los equipos SDR modernos, amarillo para lo activo, línea de acento |
| **Pro Marino** / **Pro Marino Deck** | pantalla azul profundo con números blancos y modo en amarillo |
| **Noche Roja** / **Noche Roja Deck** | todo en rojo sobre negro para operar de noche sin perder la visión nocturna (con el brillo bajo) |
| **Icom IC-7300** / **Icom IC-7300 Deck** | pantalla táctil negro azabache, teclas biseladas en gris oscuro, modo en azul Icom característico y cascada multinivel |
| **Yaesu FTDX** / **Yaesu FTDX Deck** | gris metálico satinado, teclas ámbar incandescente y cascada de alto contraste estilo 3DSS |
| **Kenwood TS-890** / **Kenwood TS-890 Deck** | grafito profundo, verde esmeralda y acentos en oro suave de la gama alta de Kenwood |
| **Elecraft K4** / **Elecraft K4 Deck** | estética táctica modular SDR, negro anodizado mate, acentos amarillo oro y máxima legibilidad |

La lista de temas en Ajustes y en la web ahora admite hasta 64 temas. Se cambian al instante, sin reiniciar.
Para retocarlos o agregar nuevos: `themes/alta_gama.py` (paleta corta → los 128 colores) y volver a compilar, o editar
el `.json` en `/mnt/themes/` desde la página web (Archivos). Las variantes Deck activan `"deck": true`
(frecuencia grande con S/SWR/ALC al lado) y tipografía compacta `rcond`.
