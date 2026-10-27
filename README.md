<h1 align="center">X6100 firmware · edición CX8ANM</h1>

<p align="center"><i>Adaptación de Daniel Niño CX8ANM (Uruguay)</i></p>

> **Esta es una versión derivada, no un firmware original.**
> Está basada en el **[firmware X6100 de Giovanni Ricciardi IU1WXY](https://github.com/IU1WXY/X6100-IU1WXY)**,
> que a su vez se basa en **[x6100_gui de Oleg Belousov R1CBU y Georgy Dyuldin R2RFE](https://github.com/gdyuldin/x6100_gui)**
> y en **[AetherX6100Buildroot](https://github.com/gdyuldin/AetherX6100Buildroot)**.
> Todo el mérito del diseño de base es de ellos. Esta edición agrega funciones, el idioma español
> y ajustes para la Región 2 / Uruguay. **No es una versión oficial de IU1WXY ni de R1CBU/R2RFE:**
> los problemas de esta edición se reportan aquí, no a ellos.

<p align="center"><img src="docs/screenshots/main_screen.png" alt="Pantalla principal" width="720"></p>

*English summary: this is a community edition of the IU1WXY X6100 firmware (itself based on the
R1CBU/R2RFE x6100_gui), adding a Spanish interface, Region 2 band plans and extra apps. All credit
for the base firmware goes to its original authors; please report issues with this edition here,
not upstream.*

## Qué agrega esta edición

Todo lo del firmware de IU1WXY (modos digitales en la radio, DX Cluster en la cascada, SOTA/POTA,
control remoto, temas…, ver [su README](docs/README_IU1WXY.md)) y además:

- **Interfaz en español**, elegible en Ajustes.
- **Planes de banda de la Región 2 / Uruguay.**
- **Actualizar desde la propia radio** (Ajustes > Info > Actualizar), sin volver a grabar la tarjeta.
- **Temas** estilo equipos de alta gama y de transceptores conocidos, con variantes *Deck*.
- **Radio por Internet**, con emisoras que se agregan a mano desde la radio o desde el navegador.
- **Estación meteorológica** en pantalla y **alertas de INUMET**.
- **Banda Ciudadana (CB) por canales** (bandas A a E), con modo predeterminado y la perilla
  saltando de canal en canal.
- **Guardar cualquier frecuencia en memoria** desde el panel.
- Punto de acceso WiFi propio, aviso de actividad del cluster en tu banda, lista de sitios de
  operación de campo (SOTA, POTA, WWBOTA, HOTA), NAVTEX/DSC guardados solos, y más.

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

- **Giovanni Ricciardi IU1WXY**: el firmware en el que se basa esta edición (reescritura de la
  interfaz y sistema de temas, X6100 Link y control remoto, Field Ops, DX Cluster, WSPR, SSTV TX,
  RTTY, WeFax TX, Bluetooth, servidor wfview y mucho más). Ver [sus créditos completos](docs/README_IU1WXY.md#credits).
- **Oleg Belousov R1CBU** y **Georgy Dyuldin R2RFE**: la interfaz x6100_gui original y el sistema
  de compilación [AetherX6100Buildroot](https://github.com/gdyuldin/AetherX6100Buildroot).
- **1KO125** de Hany El Imam / TheMurusTeam (decodificadores WeFax, NavTex, SSTV);
  **JS8** de randal007 sobre JS8Call (Jordan Sherer KN4CRD); **wsprd** y **JTTY** de WSJT-X
  (K1JT, K9AN y el equipo de WSJT-X).
- Bibliotecas: LVGL, cJSON, FFTW, liquid-dsp, libpng, bottle, PulseAudio, BlueZ, FFmpeg.
- Datos: Reverse Beacon Network, POTA, SOTA, radio-browser.info, Open-Meteo, INUMET.
- Hardware: Xiegu. Y gracias a toda la comunidad del X6100.

## Licencia

Igual que el original: la interfaz es LGPL-2.1-or-later ([LICENSE](LICENSE)); js8core, wsprd y
JTTY son GPLv3, por lo que la imagen completa es GPLv3. El servidor web tiene su propia licencia
([webserver/LICENSE](webserver/LICENSE)). El código fuente completo de esta edición está en este
repositorio, como piden esas licencias.
