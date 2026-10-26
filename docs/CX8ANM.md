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
| **Icom IC-7300 B** / **Deck** | la pantalla "tipo B" de Icom: fondo azul en degradé, teclas grises y naranja para lo activo |
| **Icom IC-7610** / **Deck** | negro con teclas grises en degradé, azul para lo activo y el modo en recuadro |
| **Yaesu FT-991A** / **Deck** | paneles gris azulado, teclas azules y amarillo para lo activo |
| **Kenwood TS-990** / **Deck** | frecuencia ámbar sobre negro con acentos azules |
| **Elecraft KX3** / **Deck** | visor de cristal líquido claro con números negros: se lee muy bien al sol |
| **Flex SmartSDR** / **Deck** | pizarra oscura de los SDR de PC, amarillo para lo activo, cascada azul a rojo |
| **ANAN Thetis** / **Deck** | negro, frecuencia amarilla y rótulos verdes |
| **Expert SunSDR** / **Deck** | negro y naranja |

La lista de temas en Ajustes y en la web ahora admite hasta 64 temas. Se cambian al instante, sin reiniciar.
Para retocarlos o agregar nuevos: `themes/alta_gama.py` (paleta corta → los 128 colores) y volver a compilar, o copiar
un `.json` a `/mnt/themes/` en la tarjeta. Las variantes Deck activan `"deck": true`
(frecuencia grande con S/SWR/ALC al lado) y tipografía compacta `rcond`.
Vistas previas aproximadas: `docs/screenshots/themes_alta_gama/pro.png` y `estilos.png`.

## Radio por Internet

**APP → (página 2) → Radio Internet.** Emisoras del directorio abierto radio-browser.info:

- **Lista** pasa por: *Favoritas* → *País* (Uruguay al empezar) → *Las más escuchadas del mundo* → la última búsqueda.
- **Buscar**: nombre de la emisora o género (rock, tango, noticias...). `pais AR` cambia el país de la lista.
- **Reproducir** (o apretar la perilla MFK sobre la emisora), **Parar**, **Favorita** (agrega o quita).
- Arriba: la emisora, el formato (MP3 128 kbps...) y el tema que está sonando, si la emisora lo manda.
- El volumen es el de siempre. Suena por el parlante o por los auriculares Bluetooth si están conectados.
- Al transmitir (PTT) la emisora se corta: nunca sale al aire. Al cerrar la aplicación se detiene.
- Necesita WiFi (o el celular como punto de acceso). Unos 60 MB por hora a 128 kbps.
- **Agregar una emisora a mano:** en la página web, **Operación → Radio Internet** (o la ficha
  *Radio por Internet* de la página principal): **Agregar**, nombre y dirección del audio, y guardar.
  También desde la radio: en **Buscar** se escribe la dirección completa (`http://...`) y queda en Favoritas.
- Las favoritas quedan en `/mnt/iradio/favorites.tsv` (nombre, dirección, formato, kbps, país; una por
  línea, separadas por tabulador).

## Estación meteorológica y alertas de INUMET

**APP → (página 3) → Meteo.** Necesita WiFi. Se actualiza al abrir y cada 10 minutos (o con **Actualizar**).

- **Ahora**: temperatura, cielo, sensación térmica, mínima y máxima del día, humedad, punto de rocío,
  presión con su tendencia de 3 horas (↑ sube, ↓ baja, = estable), viento y dirección, ráfagas,
  lluvia de la última hora y de 24 h, visibilidad, nubosidad, índice UV, salida y puesta del sol,
  mínima y máxima de las últimas 24 h y la probabilidad de lluvia de mañana. Abajo, la alerta vigente.
- **Pronóstico**: 5 días (máxima, mínima, cielo, probabilidad y milímetros de lluvia, viento) y el texto
  de INUMET para la zona (hoy y mañana, mañana y tarde/noche).
- **Alertas**: todas las advertencias de INUMET vigentes, primero las del departamento.
- La perilla MFK pasa de una vista a otra; las flechas mueven el texto largo.

Los valores medidos son de la **estación de INUMET más cercana** con datos de las últimas 3 horas
(su nombre, distancia y hora se ven a la izquierda). Lo que INUMET no publica (cielo, nubes, UV, sol,
pronóstico por días) sale de **Open-Meteo**. La posición es la del GPS si tiene, si no la del locator QTH.

**Alertas en pantalla**: con WiFi, cada 15 minutos la radio consulta las advertencias de INUMET. Una nueva
para tu departamento aparece en un recuadro del color de la alerta (amarilla, naranja, roja) durante un
minuto, una sola vez. Se apaga en **Ajustes → Info → Alertas de INUMET**. El departamento sale de la
posición; para fijarlo a mano, el archivo `/mnt/meteo/departamento` con el nombre (por ejemplo `CANELONES`).

INUMET no tiene una API publicada: se usan los mismos datos públicos que alimentan su sitio
(como hace el visor libre clima-uy). Si INUMET cambia su sitio, puede dejar de andar hasta corregirlo.

## Banda Ciudadana (CB) por canales

**APP → página 4 → Canales CB** (también en **V/M → página 3**).

- La lista muestra los 40 canales de la banda elegida, con su frecuencia. El 9 es emergencias y el 19 rutas y viajeros.
  **Banda** pasa por A, B, C (la oficial, 26.965–27.405 MHz), D y E.
- Con la perilla MFK se elige el canal; al apretarla (o **Ir**) la radio va al canal en el **modo
  predeterminado**. **Modo** lo cambia: AM, FM, USB o LSB. Después el modo se cambia como siempre.
- **Perilla**: *canales* hace que en la CB la perilla principal salte de canal en canal (y muestra
  "CB C-19"); *libre* sintoniza normal. Nunca actúa en 10 m (desde 28.000 MHz).
- Las bandas CB del cambio de banda ahora cubren los 40 canales enteros y entran en AM en el canal 19.

## Guardar cualquier frecuencia en memoria

**V/M → página 3**:

- **Guardar canal**: la frecuencia, el modo, AGC, PRE y ATT de ahora quedan en la lista de canales de
  memoria (hasta 100). En un canal CB se guarda con el nombre "CB C-19"; en una emisora conocida, con
  su nombre.
- **Lista canales**: la lista para volver a cualquiera (MFK elige, apretar va), renombrar o borrar.
  También se editan en la web, en **Memorias**.
- Las 8 memorias rápidas de siempre siguen en las páginas 1 y 2 de V/M.
