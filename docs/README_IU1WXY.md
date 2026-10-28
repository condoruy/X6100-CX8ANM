> **Nota de la edición CX8ANM:** esta es una copia del README original de IU1WXY, sin cambios.
> Aclaración: el firmware de IU1WXY nació como derivado del firmware **1KO125 de Hany El Imam**
> (no directamente de R1CBU), y las aplicaciones **SSTV, WEFAX, DSC, NAVTEX y SITOR-B** fueron
> concebidas y desarrolladas originalmente en 1KO125. Ver los [créditos de esta edición](../README.md#créditos).

<h1 align="center">X6100 custom firmware</h1>

<p align="center"><i>by Giovanni Ricciardi IU1WXY</i> · <a href="https://iu1wxy.com"><b>iu1wxy.com</b></a></p>

<p align="center">Heartfelt thanks to <b>Oleg Belousov R1CBU</b>, <b>Georgy Dyuldin R2RFE</b>,
<b>1KO125</b> by <b>Hany El Imam / TheMurusTeam</b> and the whole <b>X6100 community</b>.<br>
Thanks to <b>Fabio IN3FOB</b> for the beta testing, but above all
to <b>Leo CT7AUE</b>, <b>Angelo IU2VWK</b>, <b>Gianluca IW5DUA</b> and <b>Emilio IU8QTM</b>
for their neverending patience, time and advices.</p>

<p align="center"><b>A complete SD-card firmware for the Xiegu X6100: every digital mode on the radio,
DX cluster on the waterfall, themes, and full remote control from your phone or a browser.</b></p>

<p align="center"><img src="screenshots/main_screen.png" alt="Main screen" width="720"></p>

> **First release: everything is beta, and this is only the beginning.**
> The GUI has been deeply rewritten: a new theme system draws every screen,
> button and font, so the look can change completely without touching the code.
> It runs daily on the author's radio, but expect rough edges. The
> **companion app is alpha**: transmitting from it works, test it on a dummy
> load first. Problems and ideas: report them on [x6100-link.it](https://x6100-link.it/report.php).

## Remote control

**From your phone: X6100 Link** (Android, a separate app).
Turn on the phone's hotspot, let the radio join it, open the app: it finds the
radio by itself.

- The whole radio: frequency, band, mode, filters, squelch, RF gain, NB, NR,
  AGC, S-meter, spectrum and waterfall (VFO line, filter passband, pinch to
  zoom), RX audio on the phone with its own volume. The tuning knob has its
  own step: tap it for the next one, hold it for the list.
- **DX cluster** as on the radio: the calls over the waterfall and a column
  of the spots around the VFO (or newest first, this band or all); tap a spot
  to tune it. Each part can be turned off in the settings.
- **Every digital mode, receive and transmit**: FT8/FT4, JS8, WSPR, SSTV
  (pictures from the gallery or the camera), WeFax, RTTY, CW with macros,
  NavTex (receive). The radio decodes and transmits, the phone is the screen.
- The VFO is locked while an app runs (**BASE** brings it back).
- **SOTA/POTA page** that opens by itself when the radio enters Field Ops:
  choose the reference, start the activation, log QSOs with the phone's
  keyboard, spot; the phone's GPS gives the radio its position.
- **Log** of the day, every mode, coloured by type (voice, CW, digital) with
  filters; edit the call, the reports and the other station's SOTA/POTA
  reference.
- Portrait and landscape, English or Italian, app themes; spectrum and
  waterfall can be hidden one by one.

**From any browser: the built-in web server** (`http://<radio IP>/`, nothing
to install). The **Remote** page tunes the radio with live spectrum and
waterfall and plays the RX audio; the other pages set bands, memories, cluster,
backup, and a **gallery** with folders, multi-select, ZIP download and picture
upload for SSTV/WeFax TX.

**From a PC**: wfview (the radio appears as an IC-705 on the network) and CAT
over USB or Bluetooth.

<table>
  <tr>
    <td align="center"><img src="screenshots/android/ft8_portrait.png" alt="Companion app, FT8" width="220"><br><sub>Companion app: FT8</sub></td>
    <td align="center"><img src="screenshots/android/cw_portrait.png" alt="Companion app, CW" width="220"><br><sub>CW with macros</sub></td>
    <td align="center"><img src="screenshots/android/wefax_portrait.png" alt="Companion app, WeFax" width="220"><br><sub>WeFax</sub></td>
  </tr>
</table>

## Features on the radio

### Digital modes, all inside the radio

No PC, no sound card cable: APP → **Modes** opens them, the radio decodes and
transmits by itself.

- **FT8 / FT4**: decode, CQ, reply, auto sequence, QSO log in ADIF.
- **FT8 page 4**: **Freq** opens the list of FT8, FT4 or FT2 frequencies, one
  per band, or takes any dial frequency (14.091 for a DXpedition);
  **Hound** for Fox and Hound DXpeditions, WSJT-X rules, no SuperFox;
  **Retries** 3 to 15. A **POTA** tag marks stations spotted on POTA and any
  CQ POTA, and Auto answers a POTA activator first.
- **JS8**: JS8Call compatible, all four speeds decoded at once, inbox,
  relays, heartbeats, ADIF log with POTA/SOTA, **APRS** (position, spots,
  SMS, email). Messages up to 500 characters (from randal007's x6100-js8).
- **JTTY**: the keyboard mode of WSJT-X 3.2 (K1JT and the WSJT-X team), laid
  out like RTTY and PSK, F1-F8 contest macros, spots to PSK Reporter.
- **WSPR**: decode every cycle, random beacon (TX %), upload to WSPRnet.
- **SSTV**: receive with automatic VIS, **transmit** CQ, replies and pictures
  with text templates (Martin, Scottie, Robot). **Denoise** cleans speckles
  and noise streaks and keeps the edges; the original is always saved, the
  clean copy next to it as `_dn.png`.
- **WeFax** and **NavTex**: station lists, faxes saved as pictures, big or
  full-screen view. WeFax and RTTY also transmit from the phone.
- **CW decoder**: the new upstream receiver, which copies down to about 0 dB
  SNR and shows the WPM, here tuned not to print noise. **Parse** (CW 2:2)
  splits run-together text: `CQCQDEIU1WXYK` reads `CQ CQ DE IU1WXY K`, with
  calls, RST and abbreviations coloured.
- **CW messages**: `{HIS}` sends the other station's call, the last one the
  decoder confirmed.
- **CW Keyboard**: type on a USB or Bluetooth keyboard and the radio keys it,
  with a send queue.

### DX Cluster

- Spots drawn **on the waterfall**, in a **column** beside it, and in a full
  **Cluster app** that follows the VFO; press to tune.
- Filters by mode, band and activity (POTA, SOTA, WWBOTA, HOTA); **alerts** on callsigns and prefixes
  with a popup.
- **Newest first** and **all bands**: the column and the app can list the
  spots by time (the most recent on top, without the VFO marker row) and from
  every band, not only the current one (Settings → Cluster → CLUSTER VIEW, or
  the MFK while the column is shown).
- **Spot snap**: turning the knob slowly stops the VFO on a spot.
- Ten public servers built in, add your own from the web page.

<table>
  <tr>
    <td align="center"><img src="screenshots/cluster_column.png" alt="Cluster column by frequency"><br><sub><b>Cluster column</b> by frequency, around the VFO</sub></td>
    <td align="center"><img src="screenshots/cluster_column_time.png" alt="Cluster column by time"><br><sub><b>Newest first</b>, all bands</sub></td>
  </tr>
</table>

### Field Ops: SOTA and POTA

A second, separate interface made for portable activations, with big tiles and
big text that read well outdoors and work with gloves: APP (page 2) →
**Field Ops**, a long-press action, or straight at power on (**Start here**).
The main knob still tunes, the MFK chooses, a press opens.
**GEN** goes back to the normal radio screen.

<table>
  <tr>
    <td align="center"><img src="screenshots/field_home.png" alt="Field Ops home"><br><sub><b>Home</b>: SOTA, POTA, Bands</sub></td>
    <td align="center"><img src="screenshots/field_sota.png" alt="SOTA"><br><sub><b>SOTA</b>: the summit and the ones nearby</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/field_activation.png" alt="Activation"><br><sub><b>Activation</b>: CQ, log, spot</sub></td>
    <td align="center"><img src="screenshots/field_bands.png" alt="Bands"><br><sub><b>Bands</b>: live activity and who hears you</sub></td>
  </tr>
</table>

**Where am I?** Field Ops finds the summit or park you are on by itself.
- The position comes from the radio's **GPS**, else from the **phone** (the
  companion app sends it), else from the **QTH locator**. A dot in the header
  tells which: green for a real position, yellow for the locator.
- The SOTA summits and POTA parks of the whole world (about 35 MB) are
  downloaded once over WiFi (**Update list**, about 3 minutes, or from the web
  page); after that everything works **offline**, on a summit with no signal.
- The nearest reference is chosen for you, with its name, altitude and points
  (SOTA), locator and distance; the others nearby are listed, and **Ref...**
  types one by hand.

**The activation.** **Start** opens it, and the screen becomes a dashboard
with the band, frequency, mode, S-meter, the CQ text with your reference, the
QSO count, the time since the start and the last call.
- **CQ** calls `CQ CQ SOTA DE <call>` (or POTA) in CW.
- **Log QSO** takes the call and the reports (599 / 59 by default) and writes
  the QSO at once to an **ADIF** file on the SD card, with `MY_SOTA_REF` or
  `MY_SIG POTA` / `MY_POTA_REF`: ready for the SOTA database, POTA or QRZ.
  The logs download from the web page.
- **Spot** self-spots you on **pota.app** with the radio's frequency and mode.
  For SOTA, post an alert on SOTAwatch and call CQ in CW: RBNHole spots you
  from the Reverse Beacon Network.
- **End**, pressed twice, closes the activation.

**Nothing is lost.** The reference, the activation running, its log and the
screen you were on live on the SD card: leave with GEN by mistake, switch off,
change the battery, and Field Ops comes back exactly where it was.

**WWBOTA and HOTA.** Bunkers (WWBOTA) and History On The Air (HOTA) have
their own tiles next to SOTA and POTA: reference lists, `MY_SIG` in the log,
self-spots to wwbota.org and cqhota.app (the HOTA key goes on the web Field
Ops page). Their spots show in the cluster, each with its toggle in
Settings → Cluster.

**On the phone.** When the radio enters Field Ops, the companion app switches
by itself to its SOTA/POTA page: the reference and the nearby ones to choose
with a tap, Start and End, and the QSO entry with the phone's keyboard (call,
RST sent and received), the QSO list in local time, Spot. The phone also
gives the radio its position.

### Bands: where is the band open, and who hears me?

A live picture of the HF bands from 160 to 6 m, inside Field Ops and as an
app of its own (APP page 3 → **Bands**), in the same look as the other apps.

<table>
  <tr>
    <td align="center"><img src="screenshots/field_bands.png" alt="Band overview"><br><sub>Live on the radio: 20 m is the best band</sub></td>
    <td align="center"><img src="screenshots/bands_app.png" alt="Bands app"><br><sub><b>Bands</b> app after a <b>Test beacon</b>: heard on 20 m by 3 skimmers</sub></td>
  </tr>
</table>

- For every band: the **DX cluster** spots of the last 30 minutes and the
  **Reverse Beacon Network** skimmer spots of the last 15, merged into one
  activity bar and a word (**HIGH / GOOD / FAIR / POOR**). The best band is
  framed.
- **Test beacon**: the radio sends `TEST DE <call>` in CW. The RBN skimmers
  around the world that copy you show up within a minute in the **heard**
  column of each band, with how many heard you and the best SNR: a real
  propagation test from your own antenna, with your own power, before you
  start calling CQ. Then the info line says who heard you
  (`DK8NE +18 dB, OH6BG +7 dB...`), or tells you to try the beacon.
- **Use band** (or an MFK press) tunes a typical activation frequency of the
  chosen band, CW or phone as the radio's mode.
- RBN is connected only while the screen is open; it needs WiFi and your
  callsign. No account, nothing to install.

### Radio and screen

- **IARU band plan** strip over the waterfall, **S-meter**.
- **Long press** on every front key and on the mic F1/F2 keys: screenshot,
  recorder, mute, open any app or tool.
- Editable bands, memories and digital frequencies; transverters.
- RX recorder, screen recording, screenshots, voice messages.
- CW trainer, GPS, QTH grid, **SWR scan** across the band with the
  frequency labels on top.
- **One log of the day** for every mode: FT8, FT4, JS8, Field Ops and the
  QSOs logged from the phone all go into `field/logs/YYYYMMDD.adi`.
- Times on screen in **local time** (time zone in Settings); logs and ADIF
  stay in UTC.
- Settings rewritten as cards, all reachable with the MFK.
- Tuning from **100 kHz** up (NAVTEX on 490 kHz included).
- **Walkie Talkie**: the radio held upright with a plain portrait screen, a
  bandscope of the band around you and 32 channels of its own (WLK 4: Store,
  hold for Clear). Settings → Walkie has the switch, "Main knob steps
  channels" and the microphone. No digital modes in walkie: USB-D and LSB-D
  become USB and LSB.

### Smooth screen, smooth knob

- **New display engine (DRM/KMS, from upstream)**: the screen is updated in
  step with the panel, so no tearing, and only the parts that change are
  redrawn. On top of it, a **layer system**: the GUI
  sits on its own transparent layer; the **waterfall** (and the spectrum, when
  it is on) is drawn straight onto the layer under it, one new line at a time.
  The GUI went from about 88% to about 47% of a CPU core: more room for the
  decoders, a cooler radio, a waterfall that scrolls evenly. Screenshots and
  screen recordings show both layers. On a kernel without DRM the classic
  framebuffer is used.
- **Main knob** (upstream design): every detent is timed by its own thread, so
  fast turns no longer stutter. **Acceleration** grows with the speed (Settings → General →
  CONTROLS: **Lite** or **Strong**), and **Accel. onset** (1-5) sets how fast
  you must turn before the step grows. Slow turns stay precise for spot snap.
- **CW tune indicator** (from upstream): the bar lights where the signal is and
  fades out a second after it stops.

### Wireless

- **Bluetooth CAT** for up to 3 clients at once, together with USB CAT.
- **Bluetooth headphones** (RX audio, volume, speaker mute) and **keyboards**;
  pairings survive a new image.
- **Bluetooth tab** in Settings: adapter status, the 3-minute pairing window
  (pairing is refused outside it), the device list with headphones, keyboards
  (also the ones that send no name) and phones tagged, Forget with a
  confirmation, headphone audio and CAT clients. One pair of headphones at a
  time. Based on SP2KMO's and randal007's upstream work.
- **Ethernet**: a USB Ethernet adapter with DHCP or a fixed IP, netmask,
  gateway and DNS, in Settings → Wireless and on the web page System →
  Ethernet. WiFi keeps working alongside.
- **wfview**: the radio appears as an IC-705 on the network.
- **Web server** on the radio, HTTP and HTTPS (see [Remote control](#remote-control)).

<table>
  <tr>
    <td align="center"><img src="screenshots/theme_rs_sstv.png" alt="SSTV TX"><br><sub><b>SSTV TX</b>: picture and text template</sub></td>
    <td align="center"><img src="screenshots/cluster_app.png" alt="DX Cluster"><br><sub><b>DX Cluster</b> app</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/js8_app.png" alt="JS8"><br><sub><b>JS8</b></sub></td>
    <td align="center"><img src="screenshots/wspr_app.png" alt="WSPR"><br><sub><b>WSPR</b></sub></td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/cw_keyboard.png" alt="CW Keyboard"><br><sub><b>CW Keyboard</b></sub></td>
    <td align="center"><img src="screenshots/digital_modes_menu.png" alt="Modes menu"><br><sub><b>Modes</b> menu</sub></td>
  </tr>
</table>

## Themes

The GUI has a new **theme system**: colours, buttons, corners, gradients,
fonts and the waterfall palette all come from the theme, and a theme can draw
every surface with PNG images. Seven come with the firmware: choose one in
Settings → Display → **Theme** and the radio app restarts with it at once.
Your own go on the SD card ([how to make one](THEMES.md)). The control
the encoder is adjusting lights its red LED.

**Outdoor** is made for the sun: light backgrounds, black text, strong
borders, switches and icons that stay visible when off, a grey waterfall
(the noise light, weak signals already visible, strong ones toward black),
the S-meter in one even grey gradient and the cluster in black, so
everything reads even in full daylight. **Outdoor-Negative** is the same
theme with the lightness inverted: dark background, white text, the colours
keep their hue (the VFO line stays red).

**Holo** and **Material 3** bring the look of Android to the radio, both in
Roboto: Holo black with blue accents, square corners and a thin accent line
under the top bar and over the active button; Material 3 with soft dark
surfaces and rounded pill buttons spaced apart.

<table>
  <tr>
    <td align="center"><img src="screenshots/theme_outdoor.png" alt="Outdoor theme"><br><sub><b>Outdoor</b>: light, high contrast, for the sun</sub></td>
    <td align="center"><img src="screenshots/theme_outdoor_negative.png" alt="Outdoor-Negative theme"><br><sub><b>Outdoor-Negative</b>: the same, inverted</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/theme_iu1wxy.png" alt="IU1WXY theme"><br><sub><b>IU1WXY</b> (default): dark blue-grey</sub></td>
    <td align="center"><img src="screenshots/theme_classic.png" alt="Classic theme"><br><sub><b>Classic</b>: black, grey keys</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/theme_rs.png" alt="RS theme"><br><sub><b>RS</b>: glossy buttons with LEDs, black LCD</sub></td>
    <td align="center"><img src="screenshots/theme_rs_settings.png" alt="RS settings"><br><sub><b>RS</b>: settings</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/theme_holo.png" alt="Holo theme"><br><sub><b>Holo</b>: black, blue accents</sub></td>
    <td align="center"><img src="screenshots/theme_material3.png" alt="Material 3 theme"><br><sub><b>Material 3</b>: rounded, soft surfaces</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="screenshots/theme_rs_sstv.png" alt="RS SSTV"><br><sub><b>RS</b>: SSTV TX</sub></td>
  </tr>
</table>

## Install

1. Download the microSD image (`X6100-IU1WXY-v<version>-sdcard.img`) from the
   **[latest release](https://github.com/IU1WXY/X6100-IU1WXY/releases/latest)**
   (the X6100 Link app is distributed separately).
2. Write it to a microSD card with [balenaEtcher](https://etcher.balena.io/) or Rufus.
3. Put the card in the radio and switch on. Settings and logs live on the card's
   DATA partition (`/mnt`).

Writing a new image erases DATA too: copy it to your PC first and back after
the first start (the [manual](MANUAL.md) lists the files). A patched
baseband ([x6100_base_patch](https://github.com/gdyuldin/x6100_base_patch)) r13
or newer is recommended: the TX audio and calibration options (CESSB, band
output gain), and a battery percentage from a measured X6100 discharge curve.

## Documentation

- **[Manual](MANUAL.md)**: every app and setting, the web pages, CAT commands, files, building
- [Themes](THEMES.md): make your own

## Credits

**Giovanni Ricciardi IU1WXY** designed and wrote this firmware:

- the deep **rewrite of the GUI** and the **theme system**, with the IU1WXY,
  Outdoor, Classic, RS, Holo and Material 3 themes and the new settings;
- the **X6100 Link** app and the **remote control** of the radio (remote
  API, web Remote page, gallery, backup);
- **Field Ops** (SOTA/POTA activations, offline references, ADIF, POTA
  spots) and **Bands** (Reverse Beacon Network, test beacon);
- the **DX Cluster** (waterfall spots, column, app, filters, alerts, spot snap);
- **WSPR**, **SSTV TX**, **RTTY** and **WeFax TX**, the **CW Keyboard**, the
  integration of JS8, SSTV, WeFax and NavTex into the radio;
- **Bluetooth** CAT, headphones and keyboards, **wfview** server (Icom LAN
  protocol), IARU band plan strip, configurable long-press actions, CW trainer
  stats, and the SD image itself;
- the **layer system** on top of the DRM output (GUI on a transparent layer,
  waterfall and spectrum drawn directly underneath), and the **Accel. onset**
  setting of the main knob.

Standing on the work of: the **R1CBU** GUI by Oleg Belousov R1CBU and Georgy
Dyuldin R2RFE ([gdyuldin/x6100_gui](https://github.com/gdyuldin/x6100_gui)),
whose CW receiver, CW tune indicator, DRM/KMS display output, direct spectrum
rendering and main knob handling (detents timed in their own thread) are
imported here,
**1KO125** by Hany El Imam / TheMurusTeam (WeFax, NavTex, SSTV decoders),
**JS8** by randal007 on the JS8Call engine (Jordan Sherer KN4CRD), **wsprd**
from WSJT-X (K1JT, K9AN), **JTTY** from WSJT-X 3.2 (K1JT and the WSJT-X team). Libraries: LVGL, cJSON, FFTW, liquid-dsp, libpng,
bottle, PulseAudio, BlueZ. Fonts: Roboto, DejaVu, Arial, Sony Sketch.
Hardware: Xiegu.

Skimmer spots in Field Ops > Bands come from the **Reverse Beacon Network**
([reversebeacon.net](https://www.reversebeacon.net)), thanks to its skimmer
operators.

## Privacy and disclaimer

This is an **open source** project, provided **as is, without any warranty**: you install and use it **at your own
risk** (see the [licence](../LICENSE)). The author is not liable for any damage to the radio, accessories or data.

The firmware **sends no data to the author**. It contacts external services only when you use the feature concerned:

- **POTA**: spots, Field Ops self-spots included, go to pota.app with callsign, park, frequency and mode, and are
  public; the park list comes from pota.app.
- **SOTA**: the summit list comes from sota.org.uk.
- **QRZ Logbook**, if enabled: QSOs go to QRZ.com with your API key.
- **WSPRnet**, if enabled: received spots go to wsprnet.org with callsign and locator.
- **DX Cluster** and **Reverse Beacon Network**: they receive your callsign to log you in.

The app and the radio contact the x6100-link.it server to check for updates. Every 5 minutes the app and the radio send the site some technical data, including version, QTH, callsign and device codes. I use them for the map, the counter and to manage the devices associated with users. I don't share them with anyone. For any request, write to me.

Everything else (settings, logs, pictures) stays on the SD card. Full notice, website included:
[x6100-link.it/privacy.php](https://x6100-link.it/privacy.php).

## License

The GUI is LGPL-2.1-or-later ([LICENSE](../LICENSE)); the vendored js8core and
wsprd, and JTTY (ported from WSJT-X by K1JT and the WSJT-X team, `src/jtty*`),
are GPLv3, so the firmware image is GPLv3. The web server has its own
license ([webserver/LICENSE](../webserver/LICENSE)).
