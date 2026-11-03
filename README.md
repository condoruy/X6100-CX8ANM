<h1 align="center">X6100 firmware · edición CX8ANM</h1>

<p align="center"><i>Adaptación de Daniel Niño CX8ANM (Uruguay)</i></p>

> **Edición independiente del firmware para el Xiegu X6100.**
> Hasta la versión **0.127** se basó en el **[firmware X6100 de Giovanni Ricciardi IU1WXY](https://github.com/IU1WXY/X6100-IU1WXY)**,
> publicado bajo licencia **LGPL-2.1**, que nació como derivado del **firmware 1KO125 de Hany El Imam (TheMurusTeam)**,
> que a su vez parte de **[x6100_gui de Oleg Belousov R1CBU y Georgy Dyuldin R2RFE](https://github.com/gdyuldin/x6100_gui)**
> y de **[AetherX6100Buildroot](https://github.com/gdyuldin/AetherX6100Buildroot)** (esta edición compila con su propia copia: [condoruy/AetherX6100Buildroot](https://github.com/condoruy/AetherX6100Buildroot)).
> Desde entonces se desarrolla de forma **independiente**: no incorpora código de IU1WXY posterior a la 0.127
> y tiene sus propias pantallas y temas. **No es una versión oficial de IU1WXY, 1KO125 ni R1CBU/R2RFE:**
> los problemas de esta edición se reportan aquí, no a ellos.

## De dónde viene

```
x6100_gui (R1CBU / R2RFE)
  └─ 1KO125 (Hany El Imam, TheMurusTeam)      ← origen de SSTV, WEFAX, DSC, NAVTEX y SITOR-B
       └─ firmware IU1WXY hasta 0.127 (LGPL)
            └─ edición CX8ANM (Daniel Niño)   ← este repositorio, independiente desde entonces
```

Las aplicaciones **SSTV, WEFAX, DSC, NAVTEX y SITOR-B** fueron concebidas, desarrolladas y
puestas en funcionamiento originalmente por **Hany El Imam en el firmware 1KO125**. Esta edición
las hereda a través del firmware de IU1WXY, y el crédito de su creación corresponde a 1KO125.

<p align="center"><img src="docs/screenshots/main_screen.png" alt="Pantalla principal" width="720"></p>

*English summary: an independent X6100 firmware edition. Up to version 0.127 it was based on the
IU1WXY X6100 firmware (LGPL-2.1), which was forked from Hany El Imam's 1KO125 firmware (itself based on
the R1CBU/R2RFE x6100_gui); since then it is developed independently, with no IU1WXY code newer than
0.127 and its own screens and themes. The SSTV, WEFAX, DSC, NAVTEX and SITOR-B apps were originally
conceived and developed in 1KO125. This edition adds Spanish, German, French and Italian interfaces, a
first-start assistant (language, callsign, IARU region, country), HF air band and extra apps. Please
report issues with this edition here, not upstream.*

## Qué agrega esta edición

Lo heredado del firmware de IU1WXY hasta la 0.127 (modos digitales en la radio, DX Cluster en la cascada, SOTA/POTA,
control remoto, temas…, ver [su README](docs/README_IU1WXY.md)) y además:

- **Interfaz en español, alemán, francés e italiano** (además del inglés).
- **Asistente del primer encendido**: idioma, licencia (indicativo), región IARU y país. La imagen
  no trae ningún indicativo cargado.
- **Regiones IARU 1, 2 y 3**, con sus límites de banda, su plan de banda y la lista de países de cada una.
- **Banda aérea**: frecuencias de aviación en HF (rutas oceánicas y VOLMET) y control de tránsito
  aéreo por Internet.
- **Actualizar desde la propia radio** (Ajustes > Info > Actualizar), sin volver a grabar la tarjeta.
- **Temas** estilo equipos de alta gama y de transceptores conocidos, con variantes *Deck*.
- **Radio por Internet**, por país, con emisoras que se agregan a mano desde la radio o desde el navegador.
- **Estación meteorológica** en pantalla y **alertas de INUMET**.
- **Banda Ciudadana (CB) por canales** (bandas A a E y la UK 27/81), con modo predeterminado y la perilla
  saltando de canal en canal.
- **Guardar cualquier frecuencia en memoria** desde el panel.
- Punto de acceso WiFi propio, aviso de actividad del cluster en tu banda, lista de sitios de
  operación de campo (SOTA, POTA, WWBOTA, HOTA), NAVTEX/DSC guardados solos, y más.
- **Remoto+**: la radio desde el teléfono o la PC con voz por el micrófono, FT8 con subida a QRZ,
  CW desde el teclado, el GPS del teléfono para el locator y acceso desde cualquier lugar con Tailscale.
  Ver [docs/REMOTO.md](docs/REMOTO.md).
- **Zello**: escuchar y hablar en canales de Zello desde la radio (PTT del costado y micrófono de mano),
  con las propias llaves de desarrollador de cada uno.
- **Banda aérea propia**: cada uno carga sus emisoras de control de tránsito aéreo desde la página
  *Radio por Internet* de la web de la radio; aparecen primero en la lista Banda aérea.

El detalle de cada función está en **[docs/CX8ANM.md](docs/CX8ANM.md)**.

## Instalar

1. Bajar `Xiegu_X6100-CX8ANM_v<versión>.img.zip` de la
   **[última versión](https://github.com/condoruy/X6100-CX8ANM/releases/latest)**.
2. Grabarla en una microSD con [balenaEtcher](https://etcher.balena.io/) (acepta el .zip directo).
3. Poner la tarjeta en la radio y encender.

Grabar una imagen nueva borra también la partición DATA (ajustes, logs): copiarla antes a la PC.

**Actualizar después:** con la radio conectada a internet, *Ajustes > Info > Actualizar…*.
Baja la última versión, la verifica y la instala conservando ajustes, redes WiFi y logs.

Se recomienda la banda base parcheada
([x6100_base_patch](https://github.com/gdyuldin/x6100_base_patch)) r13 o posterior.

## Aviso

Proyecto de **código abierto**, entregado **tal cual, sin ninguna garantía**. Se instala y se usa
**bajo tu propia responsabilidad** (ver la [licencia](LICENSE)). Ni el autor de esta edición ni los
autores originales responden por daños a la radio, accesorios o datos. Transmití solo en las
frecuencias y modos que tu licencia permite.

## Privacidad

Esta edición **no envía datos a ningún servidor propio ni al de IU1WXY**. A diferencia de la versión
original, se quitó el aviso periódico a x6100-link.it, y el diagnóstico de *Ajustes > Info* se
guarda en la tarjeta (carpeta `diag`) en lugar de enviarse.

La radio solo se conecta a servicios externos cuando usás la función correspondiente:

- **Actualizaciones**: consulta las versiones publicadas en GitHub (este repositorio).
- **POTA / SOTA**: listas de parques y cumbres, y los spots que publiques (son públicos).
- **QRZ Logbook** y **WSPRnet**, solo si los activás.
- **DX Cluster** y **Reverse Beacon Network**: reciben tu indicativo para conectarte.
- **Radio por Internet** (radio-browser.info y la emisora elegida) y **Meteo** (Open-Meteo e INUMET).

Todo lo demás (ajustes, logs, fotos) queda en la tarjeta SD.

## Créditos
- **Giovanni Ricciardi IU1WXY**: el firmware (hasta la 0.127, LGPL-2.1) en el que se basó esta edición (reescritura de la
  interfaz y sistema de temas, X6100 Link y control remoto, Field Ops, DX Cluster, WSPR, SSTV TX,
  RTTY, WeFax TX, Bluetooth, servidor wfview y mucho más). Ver [sus créditos completos](docs/README_IU1WXY.md#credits).
- **Hany El Imam (1KO125 / TheMurusTeam)**: el firmware **1KO125**, del que nació el de IU1WXY,
  y las aplicaciones **SSTV, WEFAX, DSC, NAVTEX y SITOR-B**, concebidas y desarrolladas
  originalmente por él en 1KO125.
- **Oleg Belousov R1CBU** y **Georgy Dyuldin R2RFE**: la interfaz x6100_gui original sobre la que
  se construyó 1KO125, y el sistema de compilación
  [AetherX6100Buildroot](https://github.com/gdyuldin/AetherX6100Buildroot).
- **JS8** de randal007 sobre JS8Call (Jordan Sherer KN4CRD); **wsprd** y **JTTY** de WSJT-X
  (K1JT, K9AN y el equipo de WSJT-X).
- Bibliotecas: LVGL, cJSON, FFTW, liquid-dsp, libpng, bottle, PulseAudio, BlueZ, FFmpeg.
- Datos: Reverse Beacon Network, POTA, SOTA, radio-browser.info, Open-Meteo, INUMET; nombres de
  países de Unicode CLDR (Babel) y zonas horarias de tzdata; frecuencias de aviación de HFUnderground y
  DX Info Centre.
- Hardware: Xiegu. Y gracias a toda la comunidad del X6100.

## Licencia

Igual que el original: la interfaz es LGPL-2.1-or-later ([LICENSE](LICENSE)); js8core, wsprd y
JTTY son GPLv3, por lo que la imagen completa es GPLv3. El servidor web tiene su propia licencia
([webserver/LICENSE](webserver/LICENSE)). El código fuente completo de esta edición está en este
repositorio, como piden esas licencias.
