# CX8ANM Edition Manual for the Xiegu X6100

*X6100 firmware, CX8ANM edition, by Daniel Niño CX8ANM (Cóndor Tecnológico), Uruguay.*
*Versión en español: [MANUAL_ES.md](MANUAL_ES.md).*

This manual explains **what this edition adds** and **how to use each feature**, step by step. Everything
the X6100 already did (modes, filters, quick memories, digital modes, cluster, SOTA/POTA, remote control)
still works the same; for that, see the [inherited manual](MANUAL.md).

> **How to read the paths:** "**Settings → Audio → BIRDIES → Mark**" means: open Settings, go to the
> *Audio* page, find the *BIRDIES* box and tap *Mark*. On-screen names are given in English; with the radio
> in another language, they appear translated. "**Web → Operación → Zello**" is the radio's web page (see
> [The radio's web page](#the-radios-web-page)), which is in Spanish: menu *Operación* (Operation),
> page *Zello*.

## Contents

1. [What this edition is](#what-this-edition-is)
2. [Installing and updating](#installing-and-updating)
3. [First start](#first-start)
4. [Language, IARU regions and bands](#language-iaru-regions-and-bands)
5. [Display: themes, waterfall, clock and power saving](#display-themes-waterfall-clock-and-power-saving)
6. [Birdies: removing the radio's own whistles](#birdies-removing-the-radios-own-whistles)
7. [Bluetooth headphones](#bluetooth-headphones)
8. [WiFi and the radio's own access point](#wifi-and-the-radios-own-access-point)
9. [Memories: saving any frequency](#memories-saving-any-frequency)
10. [Citizens Band (CB) by channel](#citizens-band-cb-by-channel)
11. [Air band](#air-band)
12. [Internet radio](#internet-radio)
13. [Zello](#zello)
14. [Weather station and INUMET alerts](#weather-station-and-inumet-alerts)
15. [NAVTEX and DSC](#navtex-and-dsc)
16. [Cluster: activity alert on your band](#cluster-activity-alert-on-your-band)
17. [Field operation (SOTA, POTA, WWBOTA, HOTA)](#field-operation-sota-pota-wwbota-hota)
18. [The radio's web page](#the-radios-web-page)
19. [Remoto+: the radio from your phone or PC](#remoto-the-radio-from-your-phone-or-pc)
20. [Diagnostics and reporting a problem](#diagnostics-and-reporting-a-problem)
21. [Privacy](#privacy)
22. [Credits and licence](#credits-and-licence)

---

## What this edition is

An **independent** firmware for the Xiegu X6100. Up to version 0.127 it was based on the IU1WXY firmware
(LGPL-2.1 licence), which comes from 1KO125 and x6100_gui (R1CBU / R2RFE). Since then it has been developed
separately, with its own screens, themes and features. **It is not an official release** of IU1WXY, 1KO125
or R1CBU/R2RFE: please report problems with this edition in this repository.

Main additions:

| Area | What it adds |
|---|---|
| Language | Spanish, German, French and Italian interface, besides English |
| First start | Assistant: language, callsign, IARU region and country |
| Bands | IARU regions 1, 2 and 3; Uruguay / Region 2 band plan |
| Display | Own themes (Pro, styles of well-known rigs and *Deck* variants), 10 waterfall palettes, local/UTC clock |
| Audio | Birdies with automatic notch; Bluetooth headphones with the built-in microphone |
| Apps | HF air band, Internet radio, Zello, Weather with INUMET, CB by channel |
| Connectivity | The radio's own WiFi access point, Remoto+ (voice, FT8 and CW from a phone), phone GPS |
| Maintenance | Update from the radio, diagnostics saved to the card, nothing sent to third-party servers |

---

## Installing and updating

### First install (with the microSD card)

1. Download `Xiegu_X6100-CX8ANM_v<version>.img.zip` from the
   [latest release](https://github.com/condoruy/X6100-CX8ANM/releases/latest).
2. Write it to the microSD with [balenaEtcher](https://etcher.balena.io/) (it takes the `.zip` directly).
3. Put the card in the radio and switch on.

> Writing a new image **erases everything on the card** (settings, logs, recordings). If you were already
> using the radio, make a copy first: **Web → Archivos → Respaldo** (Files → Backup).

### Updating from the radio itself (recommended)

Without removing the card and without losing anything:

1. Connect the radio to the internet (home WiFi or a phone hotspot).
2. **Settings → Info → Update...**: looks for the latest version.
3. If there is a newer one, tap again: it downloads, verifies, installs and restarts.

Settings, WiFi networks, Bluetooth pairings, logs and everything on the card are kept. **Do not switch the
radio off while it says "Installing".** The history is in `update/update.log` (Web → Archivos).

- **New version notice:** when the radio connects to the internet it tells you if there is a newer version.
  Turn it off in **Settings → Info → Update alert**.
- **Baseband:** **Settings → Info → Baseband** shows the board's version and "(OK)" if it is Xiegu's latest
  known one. The baseband is updated with Xiegu's official procedure, not from here.

---

## First start

On the first start a four-step assistant appears. **The MFK knob chooses, pressing it goes to the next
step, ESC goes back one.**

1. **Language**: English, Español, Deutsch, Français, Italiano.
2. **Licence**: your callsign with the on-screen keyboard, or **No license (listen)**. The image comes
   with no callsign loaded.
3. **IARU region**: 1, 2 or 3. The right side shows which part of the world it is and which countries it
   includes.
4. **Country**: sets the country's time zone, the capital's locator (if there was none) and the Internet
   radio country.

To change it later: **Settings → General → STATION** (*IARU region*, *Country*, or *First-start assistant*
to repeat the four steps).

---

## Language, IARU regions and bands

- **Language:** **Settings → Display → Language**. Applies at once. The FT8, JS8, SSTV, WeFax, NAVTEX, PSK,
  Olivia, WSPR, CW and SOTA/POTA screens are translated too. The web page is in Spanish.
- **IARU regions:** the region changes the band edges and the band-plan colours under the spectrum.
  - Region 1: Europe, Africa, Middle East, Russia.
  - Region 2: the Americas, the Caribbean, Greenland and Hawaii.
  - Region 3: Asia and the Pacific.
- **Uruguay / Region 2 bands:** 2200 m, 630 m, 160 m (1.800-2.000), 80 m (3.500-4.000),
  60 m (5.3515-5.3665), 40 m (7.000-7.300), 30 to 10 m, 6 m (50-54) and CB. Commercial medium wave
  (530-1710 kHz) is a band of its own in AM, and shortwave broadcast bands are shown as a reference when
  tuning.

---

## Display: themes, waterfall, clock and power saving

### Themes

**Settings → Display → INTERFACE → Theme.** They change at once, no restart. All themes are the ones
designed for this edition, and each has a **Deck** variant (large frequency, S/SWR/ALC scales at the side
and flat keys):

| Theme | Look |
|---|---|
| **Pro Azul** (default) | black with grey keys, blue for what is active |
| **Pro Ambar** | amber display on black, warm tones |
| **Pro Verde** | phosphor green of classic displays |
| **Pro Grafito** | graphite grey of modern SDRs, yellow for what is active |
| **Pro Marino** | deep blue with white digits |
| **Noche Roja** | all red on black, to operate at night without losing night vision |
| Rig styles | Icom IC-7300 (and "B"), IC-7610, Yaesu FTDX, FT-991A, Kenwood TS-890, TS-990, Elecraft K4, KX3, Flex SmartSDR, ANAN Thetis, Expert SunSDR |

You can add your own themes by copying a `.json` file to the card's `themes` folder (format in
[THEMES.md](THEMES.md)).

### Waterfall colours

**Settings → Display → WATERFALL → Colors**: the theme's, Classic, Gauss, Grey, Fire, Ice, Rainbow, Green,
Purple or Amber. Also in **Web → Sistema → Pantalla** (System → Display).

### Volume and clock

- Volume is shown from 0 to 100 % (100 % is the radio's real maximum).
- The clock alternates local time (10 s) and UTC (5 s). Turn it off in
  **Settings → General → DATE / TIME → Show UTC too**.

### Power saving

**Settings → Info → Power save.** **Off** by default. When on, the processor slows down when idle and the
screen dims sooner. By default the screen does not dim by itself either; to make it dim:
**Settings → Display → Timeout** and **Brightness**.

---

## Birdies: removing the radio's own whistles

*Birdies* are fixed whistles the radio itself generates on some frequencies. This edition removes them with
the baseband notch (DNF), placed automatically whenever needed.

**Marking a birdie:**

1. Tune the whistle in **USB, LSB or CW**, somewhere **with no stations nearby**.
2. **Settings → Audio → BIRDIES → Mark**.
3. The radio listens for a second and a half, measures the tone to better than 1 Hz and saves the
   frequency. "Birdie marked at ... kHz" appears.

From then on, whenever that birdie falls inside the filter (in any mode, AM and FM included), the notch is
placed on it by itself. Once it is out of the filter, the DNF goes back to how you had it.

| Button | What it does |
|---|---|
| **Automatic notch** | turns everything on or off without erasing the list |
| **Mark** | measures the tone heard and adds it (if it was already there, keeps the new measurement) |
| **Remove** | deletes the birdie that is inside the filter |
| **Clear** | empties the list |

- If you turn the DNF on by hand, yours wins: the automatic one does not override it.
- There is a single notch: if two birdies fall inside the filter at once, the one nearest the centre is
  removed.
- If you get "no steady tone heard in the filter", the whistle is too weak or there is a station: move a
  little or narrow the filter and try again.
- The list can be seen and edited in **Web → Operación → Birdies** (frequency in kHz with three decimals
  and a note). It is stored on the card as `birdies.txt`.

---

## Bluetooth headphones

With Bluetooth headphones connected, **the receive audio goes to the headphones, the radio's speaker is
muted and you talk through the radio's built-in microphone.**

**Pairing (once):**

1. Put the headphones in pairing mode (see their manual).
2. **Settings → Bluetooth → ADAPTER**: Bluetooth on, and **Pairing (3 min)**.
3. **DEVICES → Scan**, choose the headphones in the list and connect.

**Afterwards:** they connect by themselves when switched on.

**HEADPHONES box** (in **Settings → Bluetooth**):

| Option | What it does |
|---|---|
| **RX to headphones** | sends the receive audio to the headphones |
| **Mute speaker** | mutes the radio's speaker while they are connected |
| **Volume** | headphone volume |

- Only the music profile (A2DP) is used, which is the one that gives good audio on this radio. The
  headphones' microphone is not used: while they are connected, you talk through the radio's built-in
  microphone.
- When the headphones disconnect, the speaker comes back and the microphone returns to how it was.
- Internet radio and Zello also play through the headphones.
- If you hear nothing: save the diagnostics with the headphones connected (see
  [Diagnostics](#diagnostics-and-reporting-a-problem)) and send it.

---

## WiFi and the radio's own access point

- **WiFi:** **Settings → Wireless → WIFI**, choose the network and enter the password. A phone hotspot
  works too.
- **The radio's access point (no router):** **Settings → Wireless → ACCESS POINT**. The radio creates its
  own network `X6100-<callsign>`; the password is shown on the same screen. Connect the PC or phone to that
  network and open `http://192.168.73.1`. It works **without internet** (in the field, for example). While
  it is on, the home WiFi is paused. The password is changed in the card's `hotspot.conf` file.

---

## Memories: saving any frequency

**V/M → page 3:**

- **Save channel**: saves the current frequency, mode, AGC, PRE and ATT (up to 100 channels). On a CB
  channel it is saved with its name ("CB C-19"); on a known station, with its name.
- **Channel list**: go to any of them (MFK chooses, pressing it goes), rename or delete.
- They can also be edited in **Web → Frecuencias → Memorias** (Frequencies → Memories).
- The usual 8 quick memories are still on V/M pages 1 and 2.

---

## Citizens Band (CB) by channel

**APP → page 4 → CB Channels** (also in **V/M → page 3**).

1. **Band** chooses the band: A, B, **C (the official one, 26.965-27.405 MHz)**, D, E or **UK** (27/81, FM).
   A, B, D and E are not legal in most countries.
2. The MFK knob chooses the channel (9 is emergency, 19 roads and travellers) and pressing it (or **Go**)
   goes to the channel.
3. **Mode** chooses the mode used to enter channels: AM, FM, USB or LSB.
4. **Knob**: in *channels*, the main knob jumps from channel to channel within CB (and shows "CB C-19"); it
   goes from band E into UK and carries on. In *free* it tunes normally. It never acts on 10 m.

---

## Air band

The VHF air band (118-137 MHz) is outside what the X6100 receives. There are two ways to listen to
aviation:

- **HF, on the radio: APP → page 4 → HF Air Band.** The shortwave frequencies of the oceanic routes
  (South America, Atlantic, Caribbean, Europe, Africa, Asia, Pacific), the VOLMETs (weather broadcasts) and
  search and rescue. **Zone** changes zone, MFK chooses and pressing it goes to the frequency in USB.
  **Save channel** stores it in memory. By day 8-18 MHz is best, by night 3-6 MHz.
- **Towers and approach, over the internet:** in Internet radio, the **Air band** list (see below).

---

## Internet radio

**APP → page 2 → Internet radio.** Needs WiFi.

1. **List** cycles through: *Favourites* → *your country* → *Air band* → *Most listened* → *last search*.
2. **Country** shows all countries; MFK chooses and pressing it loads its stations.
3. **Search**: name or genre (rock, tango, news...).
4. **Listen / Stop** (or press MFK on the station). **Favourite** adds or removes.

- It plays through the speaker or the Bluetooth headphones. It stops when you transmit: **it never goes on
  the air**.
- It uses about 60 MB per hour (at 128 kbps).

### Adding stations by hand and your own air band list

**Web → Operación → Radio Internet** has two lists:

- **Favoritas** (Favourites): your stations.
- **Banda aérea** (Air band): your air traffic control streams (for example, LiveATC ones). They appear
  first in the radio's *Air band* list, marked with ★.

In each list:

- **Agregar** (Add): name and audio address (the one starting with `http://` or `https://`).
- **Importar archivo** (Import file): a text file with one station per line:
  `name <TAB> address <TAB> format <TAB> kbps <TAB> country`. `;` or `|` also work as separators. Only name
  and address are required. Stations already in the list are not repeated.
- **Exportar** (Export): downloads the list in the same format, to keep it or move it to another radio.

From the radio too: typing the full address (`http://...`) in **Search** adds it to Favourites.

---

## Zello

Listen and talk on Zello channels from the radio, over the internet. **What you say on Zello does not go on
the air.**

**Set up (once), in Web → Operación → Zello:**

1. Zello **Usuario** (user) and **Clave** (password): the same account as the phone app.
2. **Developer keys** (free): log in to [developers.zello.com](https://developers.zello.com/) with that
   account, complete the profile, **Keys → Add Key**, and copy here the **Issuer** and the full
   **Private Key** (with the BEGIN and END lines).
3. **Canales** (channels): one per line, written exactly as in Zello (up to 20).
4. **Guardar** (Save) and **Probar conexión** (Test connection).

**Use, on the radio: APP → page 4 → Zello.**

1. Choose the channel with the knob and press MFK (or **Connect**).
2. You hear it through the speaker or the Bluetooth headphones.
3. To talk: hold the radio's **side PTT**, or the **Talk** button, and speak into the **hand microphone
   without pressing its PTT**.

- If you press the hand microphone's PTT, the radio transmits on RF as usual and Zello stops talking.
- **Reload channels** after changing them on the web.
- The radio needs the correct time (with internet it sets itself).

---

## Weather station and INUMET alerts

**APP → page 3 → Meteo.** Needs WiFi. Updates when opened and every 10 minutes (or with **Update**). The MFK
knob switches views:

- **Now**: temperature, feels-like, humidity, pressure and trend, wind and gusts, rain, visibility, UV,
  sunrise and sunset, and the current alert.
- **Forecast**: 5 days and INUMET's text for your area.
- **Alerts**: all current INUMET warnings, your department's first.

Measured values come from the nearest INUMET station. What INUMET does not publish comes from Open-Meteo.
The position is the GPS one or the locator.

**On-screen alerts (Uruguay):** with WiFi, every 15 minutes the radio checks INUMET. A new alert for your
department appears for one minute in a box of its colour (yellow, orange, red). Turn off in
**Settings → Info → INUMET weather alerts**.

---

## NAVTEX and DSC

- **Automatic saving:** everything received in the NAVTEX app is stored on the card, one file per day.
  Download it from **Web → Archivos → Galería → "NAVTEX / DSC recibidos"**.
- **Scanning several frequencies:**
  1. **Freq.** → choose a station or channel → page 2 → **Mark freq.** Repeat with others.
  2. **Scan** goes through the marked ones (NAVTEX 6 s each, DSC 2 s).
  3. When it detects something, it stays to listen and decode; then it resumes scanning.
  4. Touching **Freq.** or **Mode** stops the scan.

---

## Cluster: activity alert on your band

**Settings → Cluster → CLUSTER VIEW → Notify spots on my band** (on by default). When a new spot appears on
the band you are on, a "NEW SPOT ON YOUR BAND" banner shows the callsign, frequency and mode. **Pressing
MFK** while the banner is shown takes the radio to that frequency.

---

## Field operation (SOTA, POTA, WWBOTA, HOTA)

- The image already includes the reference lists. When you enter a programme, the 30 nearest sites appear:
  MFK chooses and pressing it uses that site.
- The position comes from the GPS, the phone or the locator.
- **Update list** (page 2) downloads it again over WiFi. **Ref...** lets you type a reference by hand.

---

## The radio's web page

With the radio on the same network (or connected to its access point), open the radio's IP address in a
browser (shown in **Settings → Wireless**), for example `http://192.168.1.50`. The page is in Spanish:

| Menu | Pages |
|---|---|
| **General** | radio overview |
| **Frecuencias** (Frequencies) | Bandas (Bands), Modos digitales (Digital modes), Memorias (Memories) |
| **Operación** (Operation) | Equipo (Rig), Remoto, Remoto+, Cluster, SOTA/POTA, Mensajes CW, Radio Internet, Zello, Birdies, Logs |
| **Archivos** (Files) | Archivos (card files), Galería (Gallery), Respaldo (Backup) |
| **Sistema** (System) | Pantalla (Display), Hora (Time), Ethernet, wfview |

**Equipo** (`/equipo`) is the radio on your PC: the live screen, all keys, the knobs, PTT (hold it; it
releases by itself after 3 minutes) and the receive audio. Audio needs the page opened with `https://`
(accept the certificate once). **Imagen** chooses how many frames per second the radio sends: fewer frames =
less WiFi traffic and less noise in the receiver.

---

## Remoto+: the radio from your phone or PC

A page you install on a phone or PC like an app: `https://RADIO-IP/remoto`.

| Part | What it does |
|---|---|
| **Radio** | frequency, mode, band, S-meter, spectrum and waterfall (tap to tune), receive audio |
| **Voice** | the phone's microphone goes on the air (SSB, AM or FM) |
| **FT8** | answer stations, CQ, automatic log and upload to QRZ |
| **CW** | type and the radio sends it |
| **Phone GPS** | updates the radio's locator by itself |

- The first time you must accept the radio's certificate. To install it: browser menu →
  *Add to home screen*.
- **Allow transmit** starts off every time, and the radio stops transmitting by itself if the connection
  drops, after 3 minutes or if the page is closed.
- **Phone GPS:** in Remoto+ *Settings*, **Use the phone's GPS**. While the page is open, the radio's locator
  (QTH) updates by itself. Works with the radio connected to the phone's hotspot.
- To use it **away from home**, with Tailscale, without opening router ports.

Full details, including Tailscale (in Spanish): [REMOTO.md](REMOTO.md).

---

## Diagnostics and reporting a problem

The report gathers the errors of the whole radio: the interface (and its last unexpected restart), the
system, the kernel, Zello, Internet radio, updates, Bluetooth and headphones, the network, the baseband and
the services that are running.

**From the web page (easiest):**

1. Make the problem happen (for example, with the headphones connected).
2. **Web → Sistema → Diagnóstico** (System → Diagnostics): write what happened and the exact error message,
   and tap **Generar informe** (Generate report).
3. **Reportar en GitHub** (Report on GitHub) opens a new issue with your text and the error summary already
   filled in, and downloads the full file: drag it into the issue box and submit. (A free GitHub account is
   needed.)

**From the radio:** **Settings → Info → Diagnostics → Save.** The file goes to the card's `diag` folder
(**Web → Archivos → diag**) to send it any way you like.

Diagnostics are not sent anywhere: the file stays on your card until you share it.

---

## Privacy

This edition **sends no data to any server of its own or to IU1WXY's** (the periodic report to
x6100-link.it was removed). The radio only connects out when you use the matching feature:

- Updates: GitHub (this repository).
- POTA / SOTA, QRZ Logbook, WSPRnet, DX Cluster, Reverse Beacon Network: only if you use them.
- Internet radio (radio-browser.info and the station), Weather (Open-Meteo and INUMET), Zello.

Everything else (settings, logs, photos, passwords) stays on the SD card.

---

## Credits and licence

- **CX8ANM edition:** Daniel Niño CX8ANM, Cóndor Tecnológico (Uruguay).
- **Up to 0.127:** firmware by **Giovanni Ricciardi IU1WXY** (LGPL-2.1).
- **1KO125** by **Hany El Imam** (TheMurusTeam): origin of the IU1WXY firmware and of the SSTV, WEFAX, DSC,
  NAVTEX and SITOR-B apps.
- **x6100_gui** by **Oleg Belousov R1CBU** and **Georgy Dyuldin R2RFE**, and AetherX6100Buildroot (this
  edition builds with its own copy).
- Libraries and data: see the [README](../README.md).

Free software, **with no warranty at all**; installed and used **at your own risk**. Transmit only on the
frequencies and modes your licence allows. Licence: LGPL-2.1-or-later for the interface and GPLv3 for the
complete image (see [LICENSE](../LICENSE)).
