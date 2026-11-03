# Manual de la edición CX8ANM para el Xiegu X6100

*Firmware X6100 edición CX8ANM, de Daniel Niño CX8ANM (Cóndor Tecnológico), Uruguay.*
*English version: [MANUAL_EN.md](MANUAL_EN.md).*

Este manual explica **qué agrega esta edición** y **cómo se usa cada cosa**, paso a paso. Lo que el
X6100 ya hacía (modos, filtros, memorias rápidas, modos digitales, cluster, SOTA/POTA, control remoto)
sigue funcionando igual; para eso, ver el [manual heredado (en inglés)](MANUAL.md).

> **Cómo leer las rutas:** "**Ajustes → Audio → BIRDIES → Marcar**" quiere decir: abrir Ajustes, ir a
> la página *Audio*, buscar el recuadro *BIRDIES* y tocar *Marcar*. "**Web → Operación → Zello**" es la
> página web de la radio (ver [La página web de la radio](#la-página-web-de-la-radio)), menú *Operación*,
> página *Zello*.

## Índice

1. [Qué es esta edición](#qué-es-esta-edición)
2. [Instalar y actualizar](#instalar-y-actualizar)
3. [Primer encendido](#primer-encendido)
4. [Idioma, regiones IARU y bandas](#idioma-regiones-iaru-y-bandas)
5. [Pantalla: temas, cascada, reloj y ahorro de energía](#pantalla-temas-cascada-reloj-y-ahorro-de-energía)
6. [Birdies: sacar los silbidos de la radio](#birdies-sacar-los-silbidos-de-la-radio)
7. [Auriculares Bluetooth](#auriculares-bluetooth)
8. [WiFi y punto de acceso propio](#wifi-y-punto-de-acceso-propio)
9. [Memorias: guardar cualquier frecuencia](#memorias-guardar-cualquier-frecuencia)
10. [Banda Ciudadana (CB) por canales](#banda-ciudadana-cb-por-canales)
11. [Banda aérea](#banda-aérea)
12. [Radio por Internet](#radio-por-internet)
13. [Zello](#zello)
14. [Estación meteorológica y alertas de INUMET](#estación-meteorológica-y-alertas-de-inumet)
15. [NAVTEX y DSC](#navtex-y-dsc)
16. [Cluster: aviso de actividad en tu banda](#cluster-aviso-de-actividad-en-tu-banda)
17. [Operación de campo (SOTA, POTA, WWBOTA, HOTA)](#operación-de-campo-sota-pota-wwbota-hota)
18. [La página web de la radio](#la-página-web-de-la-radio)
19. [Remoto+: la radio desde el teléfono o la PC](#remoto-la-radio-desde-el-teléfono-o-la-pc)
20. [Diagnóstico y cómo reportar un problema](#diagnóstico-y-cómo-reportar-un-problema)
21. [Privacidad](#privacidad)
22. [Créditos y licencia](#créditos-y-licencia)

---

## Qué es esta edición

Es un firmware **independiente** para el Xiegu X6100. Hasta la versión 0.127 se basó en el firmware de
IU1WXY (licencia LGPL-2.1), que viene de 1KO125 y de x6100_gui (R1CBU / R2RFE). Desde entonces se
desarrolla por separado, con sus propias pantallas, temas y funciones. **No es una versión oficial** de
IU1WXY, 1KO125 ni R1CBU/R2RFE: los problemas de esta edición se reportan en este repositorio.

Lo principal que agrega:

| Área | Qué agrega |
|---|---|
| Idioma | Interfaz en español, alemán, francés e italiano, además del inglés |
| Primer encendido | Asistente: idioma, indicativo, región IARU y país |
| Bandas | Regiones IARU 1, 2 y 3; plan de banda de Uruguay / Región 2 |
| Pantalla | Temas propios (Pro, estilos de equipos conocidos y variantes *Deck*), 10 paletas de cascada, reloj local/UTC |
| Audio | Birdies con notch automático; auriculares Bluetooth con micrófono integrado |
| Apps | Banda aérea HF, Radio por Internet, Zello, Meteo con INUMET, CB por canales |
| Conexión | Punto de acceso WiFi propio, Remoto+ (voz, FT8 y CW desde el teléfono), GPS del teléfono |
| Mantenimiento | Actualizar desde la radio, diagnóstico guardado en la tarjeta, sin envíos a servidores ajenos |

---

## Instalar y actualizar

### Primera instalación (con la tarjeta microSD)

1. Bajar `Xiegu_X6100-CX8ANM_v<versión>.img.zip` de la
   [última versión](https://github.com/condoruy/X6100-CX8ANM/releases/latest).
2. Grabarlo en la microSD con [balenaEtcher](https://etcher.balena.io/) (acepta el `.zip` directo).
3. Poner la tarjeta en la radio y encender.

> Grabar una imagen nueva **borra todo lo de la tarjeta** (ajustes, logs, grabaciones). Si ya usabas la
> radio, primero guardá una copia: **Web → Archivos → Respaldo**.

### Actualizar desde la propia radio (recomendado)

Sin sacar la tarjeta y sin perder nada:

1. Conectar la radio a internet (WiFi de casa o el celular como punto de acceso).
2. **Ajustes → Info → Actualizar...**: busca la última versión.
3. Si hay una nueva, tocar otra vez: la baja, la verifica, la instala y reinicia.

Se conservan los ajustes, las redes WiFi, los emparejamientos Bluetooth, los logs y todo lo de la tarjeta.
**No apagar la radio mientras dice "Instalando".** El historial queda en `update/update.log`
(Web → Archivos).

- **Aviso de versión nueva:** al conectarse a internet la radio avisa sola si hay una versión más nueva.
  Se apaga en **Ajustes → Info → Aviso de actualización**.
- **Banda base:** **Ajustes → Info → Banda base** muestra la versión de la placa y "(OK)" si es la última
  conocida de Xiegu. Esa se actualiza con el procedimiento oficial de Xiegu, no desde acá.

---

## Primer encendido

La primera vez aparece un asistente de cuatro pasos. **La perilla MFK elige, apretarla pasa al siguiente,
ESC vuelve uno atrás.**

1. **Idioma**: English, Español, Deutsch, Français, Italiano.
2. **Licencia**: tu indicativo con el teclado de la pantalla, o **Sin licencia (escucha)**. La imagen no
   trae ningún indicativo cargado.
3. **Región IARU**: 1, 2 o 3. A la derecha se ve qué parte del mundo es y qué países incluye.
4. **País**: pone la hora del país, el locator de la capital (si no había uno) y el país de la Radio por
   Internet.

Para cambiarlo después: **Ajustes → General → ESTACIÓN** (*Región IARU*, *País* o *Asistente inicial*
para repetir los cuatro pasos).

---

## Idioma, regiones IARU y bandas

- **Idioma:** **Ajustes → Pantalla → Idioma**. Cambia al instante. Están traducidas también las pantallas
  de FT8, JS8, SSTV, WeFax, NAVTEX, PSK, Olivia, WSPR, CW y SOTA/POTA. La página web está en español.
- **Regiones IARU:** la región cambia los límites de las bandas y el plan de colores bajo el espectro.
  - Región 1: Europa, África, Medio Oriente, Rusia.
  - Región 2: las Américas, el Caribe, Groenlandia y Hawái.
  - Región 3: Asia y el Pacífico.
- **Bandas para Uruguay / Región 2:** 2200 m, 630 m, 160 m (1,800-2,000), 80 m (3,500-4,000),
  60 m (5,3515-5,3665), 40 m (7,000-7,300), 30 a 10 m, 6 m (50-54) y CB. La onda media comercial
  (530-1710 kHz) es una banda propia en AM, y las bandas de radiodifusión de onda corta se muestran como
  referencia al sintonizar.

---

## Pantalla: temas, cascada, reloj y ahorro de energía

### Temas

**Ajustes → Pantalla → INTERFAZ → Tema.** Cambian al instante, sin reiniciar. Todos los temas son los
diseñados para esta edición, y cada uno tiene una variante **Deck** (frecuencia grande, escalas S/SWR/ALC
al costado y teclas planas):

| Tema | Cómo se ve |
|---|---|
| **Pro Azul** (el de fábrica) | negro con teclas grises, azul para lo activo |
| **Pro Ámbar** | visor ámbar sobre negro, tonos cálidos |
| **Pro Verde** | verde fósforo de los visores clásicos |
| **Pro Grafito** | gris grafito de los SDR modernos, amarillo para lo activo |
| **Pro Marino** | azul profundo con números blancos |
| **Noche Roja** | todo rojo sobre negro, para operar de noche sin perder la visión nocturna |
| Estilos de equipos | Icom IC-7300 (y "B"), IC-7610, Yaesu FTDX, FT-991A, Kenwood TS-890, TS-990, Elecraft K4, KX3, Flex SmartSDR, ANAN Thetis, Expert SunSDR |

Se pueden agregar temas propios copiando un archivo `.json` a la carpeta `themes` de la tarjeta (formato en
[THEMES.md](THEMES.md)).

### Colores de la cascada

**Ajustes → Pantalla → CASCADA → Colores**: los del tema, Clásico, Gauss, Gris, Fuego, Hielo, Arco iris,
Verde, Violeta o Ámbar. También en **Web → Sistema → Pantalla**.

### Volumen y reloj

- El volumen se muestra de 0 a 100 % (100 % es el máximo real de la radio).
- El reloj alterna la hora local (10 s) y la UTC (5 s). Se apaga en
  **Ajustes → General → FECHA / HORA → Mostrar también UTC**.

### Ahorro de energía

**Ajustes → Info → Modo ahorro.** Viene **apagado**. Encendido, el procesador baja la velocidad en reposo y
la pantalla se atenúa antes. La pantalla tampoco se atenúa sola de fábrica; para que lo haga:
**Ajustes → Pantalla → Tiempo** y **Brillo**.

---

## Birdies: sacar los silbidos de la radio

Los *birdies* son silbidos fijos que genera la propia radio en algunas frecuencias. Esta edición los quita
con el notch (DNF) de la banda base, que se pone solo cada vez que hace falta.

**Marcar un birdie:**

1. Sintonizá el silbido en **USB, LSB o CW**, en un lugar **sin estaciones cerca**.
2. **Ajustes → Audio → BIRDIES → Marcar**.
3. La radio escucha un segundo y medio, mide el tono con menos de 1 Hz de error y guarda la frecuencia.
   Aparece "Birdie marcado en ... kHz".

Desde ahí, cada vez que ese birdie cae dentro del filtro (en cualquier modo, también AM y FM), el notch se
pone encima solo. Al salir del filtro, el DNF vuelve a como lo tenías.

| Botón | Qué hace |
|---|---|
| **Notch automático** | prende o apaga todo sin borrar la lista |
| **Marcar** | mide el tono que se escucha y lo agrega (si ya estaba, guarda la medida nueva) |
| **Quitar** | borra el birdie que está dentro del filtro |
| **Borrar** | vacía la lista |

- Si prendés el DNF a mano, manda el tuyo: el automático no lo pisa.
- Hay un solo notch: si dos birdies caen dentro del filtro a la vez, se quita el más cercano al centro.
- Si sale "no se escucha un tono fijo en el filtro", el silbido es muy débil o hay una estación: movete un
  poco o angostá el filtro y volvé a probar.
- La lista se ve y se edita en **Web → Operación → Birdies** (frecuencia en kHz con tres decimales y una
  nota). Queda en la tarjeta, en `birdies.txt`.

---

## Auriculares Bluetooth

Con auriculares Bluetooth conectados, **el audio de recepción sale por los auriculares, el parlante de la
radio se apaga y para hablar se usa el micrófono integrado** de la radio.

**Emparejar (una sola vez):**

1. Poné los auriculares en modo emparejamiento (ver su manual).
2. **Ajustes → Bluetooth → ADAPTADOR**: Bluetooth encendido, y **Emparejar (3 min)**.
3. **DISPOSITIVOS → Buscar**, elegí los auriculares de la lista y conectá.

**Después:** al prenderlos se conectan solos.

**Recuadro AURICULARES** (en **Ajustes → Bluetooth**):

| Opción | Qué hace |
|---|---|
| **RX a los auriculares** | manda el audio de recepción a los auriculares |
| **Silenciar parlante** | apaga el parlante de la radio mientras están conectados |
| **Volumen** | volumen de los auriculares |

- Se usa solo el modo de música (A2DP), que es el que da buen audio en esta radio. El micrófono de los
  auriculares no se usa: mientras están conectados, se habla por el micrófono integrado de la radio.
- Al desconectar los auriculares vuelve el parlante y el micrófono queda como estaba.
- La Radio por Internet y Zello también suenan por los auriculares.
- Si no se escuchan: guardá el diagnóstico con los auriculares conectados (ver
  [Diagnóstico](#diagnóstico-y-cómo-reportar-un-problema)) y mandalo.

---

## WiFi y punto de acceso propio

- **WiFi:** **Ajustes → Inalámbrico → WIFI**, elegir la red y poner la clave. Sirve también el celular como
  punto de acceso.
- **Punto de acceso de la radio (sin router):** **Ajustes → Inalámbrico → PUNTO DE ACCESO**. La radio arma
  su propia red `X6100-<indicativo>`; la clave se ve en la misma pantalla. Conectá la PC o el teléfono a esa
  red y abrí `http://192.168.73.1`. Funciona **sin internet** (en el campo, por ejemplo). Mientras está
  encendido, la WiFi de casa queda en pausa. La clave se cambia en el archivo `hotspot.conf` de la tarjeta.

---

## Memorias: guardar cualquier frecuencia

**V/M → página 3:**

- **Guardar canal**: guarda la frecuencia, el modo, AGC, PRE y ATT de ahora (hasta 100 canales). En un
  canal CB se guarda con su nombre ("CB C-19"); en una emisora conocida, con su nombre.
- **Lista canales**: para ir a cualquiera (la MFK elige, apretarla va), renombrar o borrar.
- También se editan en **Web → Frecuencias → Memorias**.
- Las 8 memorias rápidas de siempre siguen en las páginas 1 y 2 de V/M.

---

## Banda Ciudadana (CB) por canales

**APP → página 4 → Canales CB** (también en **V/M → página 3**).

1. **Banda** elige la banda: A, B, **C (la oficial, 26,965-27,405 MHz)**, D, E o **UK** (27/81, en FM).
   A, B, D y E no son legales en la mayoría de los países.
2. La perilla MFK elige el canal (el 9 es emergencias, el 19 rutas y viajeros) y apretarla (o **Ir**) va al
   canal.
3. **Modo** elige el modo con el que se entra a los canales: AM, FM, USB o LSB.
4. **Perilla**: en *canales*, la perilla principal salta de canal en canal dentro de la CB (y muestra
   "CB C-19"); pasa de la banda E a la UK y sigue. En *libre* sintoniza normal. Nunca actúa en 10 m.

---

## Banda aérea

La banda aérea de VHF (118-137 MHz) está fuera de lo que recibe el X6100. Hay dos formas de escuchar
aviación:

- **HF, por la radio: APP → página 4 → Banda aérea HF.** Las frecuencias de onda corta de las rutas
  oceánicas (Sudamérica, Atlántico, Caribe, Europa, África, Asia, Pacífico), los VOLMET (partes
  meteorológicos) y búsqueda y rescate. **Zona** cambia de zona, la MFK elige y apretarla va a la
  frecuencia en USB. **Guardar canal** la deja en memoria. De día se escucha más en 8-18 MHz y de noche en
  3-6 MHz.
- **Torres y aproximación, por internet:** en la Radio por Internet, lista **Banda aérea** (ver abajo).

---

## Radio por Internet

**APP → página 2 → Radio Internet.** Necesita WiFi.

1. **Lista** pasa por: *Favoritas* → *tu país* → *Banda aérea* → *Las más escuchadas* → *la última búsqueda*.
2. **País** muestra todos los países; la MFK elige y apretarla trae sus emisoras.
3. **Buscar**: nombre o género (rock, tango, noticias...).
4. **Escuchar / Parar** (o apretar la MFK sobre la emisora). **Favorita** agrega o quita.

- Suena por el parlante o por los auriculares Bluetooth. Al transmitir se corta: **nunca sale al aire**.
- Usa unos 60 MB por hora (a 128 kbps).

### Agregar emisoras a mano y la banda aérea propia

**Web → Operación → Radio Internet** tiene dos listas:

- **Favoritas:** tus emisoras.
- **Banda aérea:** tus emisoras de control de tránsito aéreo (por ejemplo, las de LiveATC). Aparecen
  primero en la lista *Banda aérea* de la radio, marcadas con ★.

En cada lista:

- **Agregar**: nombre y dirección del audio (la que empieza con `http://` o `https://`).
- **Importar archivo**: un archivo de texto con una emisora por línea:
  `nombre <TAB> dirección <TAB> formato <TAB> kbps <TAB> país`. También sirven `;` o `|` como separador.
  Solo el nombre y la dirección son obligatorios. Las que ya estaban no se repiten.
- **Exportar**: baja la lista en ese mismo formato, para guardarla o pasarla a otra radio.

Desde la radio también se puede: en **Buscar**, escribir la dirección completa (`http://...`) la agrega a
Favoritas.

---

## Zello

Escuchar y hablar en canales de Zello desde la radio, por internet. **Lo que se habla por Zello no sale al
aire.**

**Configurar (una vez), en Web → Operación → Zello:**

1. **Usuario** y **Clave** de Zello (la misma cuenta de la app del celular).
2. **Llaves de desarrollador** (gratis): entrá a [developers.zello.com](https://developers.zello.com/) con
   esa cuenta, completá el perfil, **Keys → Add Key**, y copiá acá el **Issuer** y la **Private Key**
   completa (con las líneas BEGIN y END).
3. **Canales**: uno por línea, escritos igual que en Zello (hasta 20).
4. **Guardar** y **Probar conexión**.

**Usar, en la radio: APP → página 4 → Zello.**

1. Elegí el canal con la perilla y apretá la MFK (o **Conectar**).
2. Se escucha por el parlante o los auriculares Bluetooth.
3. Para hablar: mantené el **PTT del costado** de la radio, o el botón **Hablar**, y hablá por el
   **micrófono de mano sin apretar su PTT**.

- Si apretás el PTT del micrófono de mano, la radio transmite por RF como siempre y Zello deja de hablar.
- **Recargar canales** después de cambiarlos en la web.
- La radio necesita la hora bien puesta (con internet se pone sola).

---

## Estación meteorológica y alertas de INUMET

**APP → página 3 → Meteo.** Necesita WiFi. Se actualiza al abrir y cada 10 minutos (o con **Actualizar**).
La perilla MFK pasa de una vista a otra:

- **Ahora**: temperatura, sensación térmica, humedad, presión y su tendencia, viento y ráfagas, lluvia,
  visibilidad, UV, salida y puesta del sol, y la alerta vigente.
- **Pronóstico**: 5 días y el texto de INUMET para tu zona.
- **Alertas**: todas las advertencias de INUMET vigentes, primero las de tu departamento.

Los datos medidos son de la estación de INUMET más cercana. Lo que INUMET no publica sale de Open-Meteo.
La posición es la del GPS o la del locator.

**Alertas en pantalla (Uruguay):** con WiFi, cada 15 minutos la radio consulta INUMET. Una alerta nueva para
tu departamento aparece un minuto en un recuadro de su color (amarilla, naranja, roja). Se apaga en
**Ajustes → Info → Alertas de INUMET**.

---

## NAVTEX y DSC

- **Guardado automático:** todo lo que se recibe en la app NAVTEX queda en la tarjeta, un archivo por día.
  Se baja en **Web → Archivos → Galería → "NAVTEX / DSC recibidos"**.
- **Escanear varias frecuencias:**
  1. **Frec.** → elegí una estación o canal → página 2 → **Marcar frec.** Repetí con otras.
  2. **Escanear** recorre las marcadas (NAVTEX 6 s cada una, DSC 2 s).
  3. Cuando detecta algo, se queda escuchando y decodificando; después sigue escaneando.
  4. Tocar **Frec.** o **Modo** detiene el escaneo.

---

## Cluster: aviso de actividad en tu banda

**Ajustes → Cluster → VISTA DEL CLUSTER → Avisar spots en mi banda** (prendido de fábrica). Cuando aparece
un spot nuevo en la banda donde estás, sale el cartel "SPOT NUEVO EN SU BANDA" con indicativo, frecuencia y
modo. **Apretar la MFK** mientras se ve el cartel lleva la radio a esa frecuencia.

---

## Operación de campo (SOTA, POTA, WWBOTA, HOTA)

- La imagen ya trae las listas de referencias. Al entrar a un programa aparecen los 30 sitios más cercanos:
  la MFK elige y apretarla usa ese sitio.
- La posición sale del GPS, del teléfono o del locator.
- **Actualizar lista** (página 2) la baja de nuevo con WiFi. **Ref...** permite escribir una referencia a
  mano.

---

## La página web de la radio

Con la radio en la misma red (o conectado a su punto de acceso), abrí en el navegador la dirección IP de la
radio (se ve en **Ajustes → Inalámbrico**), por ejemplo `http://192.168.1.50`.

| Menú | Páginas |
|---|---|
| **General** | resumen de la radio |
| **Frecuencias** | Bandas, Modos digitales, Memorias |
| **Operación** | Equipo, Remoto, Remoto+, Cluster, SOTA/POTA, Mensajes CW, Radio Internet, Zello, Birdies, Logs |
| **Archivos** | Archivos de la tarjeta, Galería, Respaldo |
| **Sistema** | Pantalla, Hora, Ethernet, wfview |

**Equipo** (`/equipo`) es la radio en la PC: la pantalla en vivo, todas las teclas, las perillas, el PTT
(mantener apretado; se suelta solo a los 3 minutos) y el audio de recepción. El audio necesita abrir la
página con `https://` (aceptar el certificado una vez). **Imagen** elige cuántas imágenes por segundo manda
la radio: menos imágenes = menos tráfico WiFi y menos ruido en el receptor.

---

## Remoto+: la radio desde el teléfono o la PC

Una página que se instala en el teléfono o la PC como si fuera una app: `https://IP-DE-LA-RADIO/remoto`.

| Parte | Qué hace |
|---|---|
| **Radio** | frecuencia, modo, banda, S-meter, espectro y cascada (tocar para sintonizar), audio de recepción |
| **Voz** | el micrófono del teléfono sale al aire (SSB, AM o FM) |
| **FT8** | contestar estaciones, CQ, log automático y subida a QRZ |
| **CW** | escribir y la radio lo manda |
| **GPS del teléfono** | actualiza solo el locator de la radio |

- La primera vez hay que aceptar el certificado de la radio. Para instalarla: menú del navegador →
  *Agregar a la pantalla de inicio*.
- **Permitir transmitir** empieza apagado cada vez, y la radio deja de transmitir sola si se corta la
  conexión, a los 3 minutos o si se cierra la página.
- **GPS del teléfono:** en *Ajustes* de Remoto+, **Usar el GPS del teléfono**. Mientras la página está
  abierta, el locator (QTH) de la radio se actualiza solo. Sirve con la radio conectada a la WiFi del celular.
- Para usarla **fuera de casa**, con Tailscale, sin abrir puertos del router.

Detalle completo, incluido Tailscale: [REMOTO.md](REMOTO.md).

---

## Diagnóstico y cómo reportar un problema

El informe junta los errores de toda la radio: la interfaz (y su último cierre inesperado), el sistema, el
núcleo, Zello, la Radio por Internet, las actualizaciones, el Bluetooth y los auriculares, la red, la banda base
y los servicios que están corriendo.

**Desde la página web (lo más fácil):**

1. Hacé que el problema aparezca (por ejemplo, con los auriculares conectados).
2. **Web → Sistema → Diagnóstico**: escribí qué pasó y el mensaje de error exacto, y tocá **Generar informe**.
3. **Reportar en GitHub** abre un reporte nuevo con tu texto y el resumen de errores ya cargados, y baja el
   archivo completo: arrastralo a la caja del reporte y enviá. (Hace falta una cuenta de GitHub, gratis.)

**Desde la radio:** **Ajustes → Info → Diagnóstico → Guardar.** El archivo queda en la carpeta `diag` de la
tarjeta (**Web → Archivos → diag**) para mandarlo como quieras.

El diagnóstico no se manda a ningún lado: queda en tu tarjeta hasta que vos lo compartas.

---

## Privacidad

Esta edición **no manda datos a ningún servidor propio ni al de IU1WXY** (se quitó el aviso periódico a
x6100-link.it). La radio solo se conecta afuera cuando usás la función correspondiente:

- Actualizaciones: GitHub (este repositorio).
- POTA / SOTA, QRZ Logbook, WSPRnet, DX Cluster, Reverse Beacon Network: solo si los usás.
- Radio por Internet (radio-browser.info y la emisora), Meteo (Open-Meteo e INUMET), Zello.

Todo lo demás (ajustes, logs, fotos, claves) queda en la tarjeta SD.

---

## Créditos y licencia

- **Edición CX8ANM:** Daniel Niño CX8ANM, Cóndor Tecnológico (Uruguay).
- **Hasta la 0.127:** firmware de **Giovanni Ricciardi IU1WXY** (LGPL-2.1).
- **1KO125** de **Hany El Imam** (TheMurusTeam): origen del firmware de IU1WXY y de las apps SSTV, WEFAX,
  DSC, NAVTEX y SITOR-B.
- **x6100_gui** de **Oleg Belousov R1CBU** y **Georgy Dyuldin R2RFE**, y AetherX6100Buildroot (esta edición
  compila con su propia copia).
- Bibliotecas y datos: ver el [README](../README.md).

Software libre, **sin ninguna garantía**; se instala y se usa **bajo tu propia responsabilidad**. Transmití
solo en las frecuencias y modos que tu licencia permite. Licencia: LGPL-2.1-or-later para la interfaz y
GPLv3 para la imagen completa (ver [LICENSE](../LICENSE)).
