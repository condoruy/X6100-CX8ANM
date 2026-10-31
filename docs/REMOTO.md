# Remoto+ — la radio desde el teléfono o la PC


**Remoto+** es una página web que sirve la propia radio y que se instala en el teléfono o la PC
como si fuera una app, gratis y sin tienda. Tiene cinco partes:

| Parte | Qué hace |
|---|---|
| **Radio** | Frecuencia, modo, banda, S-meter, espectro y cascada en vivo (tocar o arrastrar para sintonizar), filtro, ganancia RF, potencia y el audio de recepción. |
| **Voz** | El micrófono del teléfono o de la PC sale al aire: PTT manteniendo apretado, o tocar para empezar y tocar para terminar. SSB, AM o FM. |
| **FT8** | Las estaciones decodificadas, tocar una para contestarle, CQ, Auto y Parar. Los QSO se anotan solos en el log de la radio y, si se quiere, se suben a QRZ. |
| **CW** | Escribir y la radio lo manda, con mensajes guardados ({MI} = su indicativo) y el texto decodificado. |
| **Seguridad** | "Permitir transmitir" empieza apagado cada vez; la radio corta sola si se pierde la conexión. |

## Abrir la página

En la misma WiFi que la radio:

```
https://IP-DE-LA-RADIO/remoto
```

La primera vez el navegador avisa que el certificado no es de confianza: la radio usa uno propio.
Hay que aceptarlo una vez (en Chrome: *Configuración avanzada → Continuar*). Hace falta **https**
porque los navegadores solo dejan usar el micrófono en páginas seguras.

**Instalar como app:** en el teléfono, menú del navegador → *Agregar a la pantalla de inicio*. En
la PC con Chrome o Edge, el ícono de instalar en la barra de direcciones.

## Voz: cómo funciona y qué la corta

- Se transmite desde la **pantalla principal** de la radio, en **LSB, USB, AM o FM** (no con una app
  abierta ni en CW o digital).
- El audio viaja en trozos de 0,2 s; la radio junta 0,3 s antes de salir al aire, así que hay
  medio segundo de demora, como en otras apps de control remoto.
- La radio **deja de transmitir sola**:
  - si no le llega audio por 0,8 s (se cortó la WiFi o internet),
  - a los **3 minutos** de una pasada,
  - si se abre una app, se cambia a CW o digital,
  - si se suelta el PTT, se cierra la página o el teléfono pasa a segundo plano.
- La ganancia del micrófono se ajusta en la pestaña Voz; mirar el ALC de la radio la primera vez.

## FT8 y QRZ

- **Abrir FT8 en la radio** abre la app en la radio; desde ahí todo se maneja en la página.
- Tocar un mensaje lo contesta (como tocarlo en la pantalla de la radio).
- Los QSO se anotan en el log del día de la radio (`/mnt/field/logs/AAAAMMDD.adi`).
- **QRZ**: con la clave de QRZ Logbook cargada en la página *SOTA/POTA* de la web de la radio,
  "Subir cada QSO a QRZ" sube cada QSO al terminarlo, y "Subir el día" sube todo el log de hoy
  (QRZ descarta los repetidos).

## CW

La radio tiene que estar en **CW** (botón *Pasar a CW*) y en la pantalla principal. Velocidad,
texto libre y mensajes guardados; se mantienen apretados para cambiarlos.

## GPS del teléfono → locator

En *Ajustes*, **Usar el GPS del teléfono** manda la posición del teléfono a la radio mientras la
página está abierta, y el locator (QTH) de la radio se actualiza solo: sirve para FT8, el log y
las apps que usan el locator. El último valor queda guardado al cerrar la página.

- Funciona igual con la radio conectada a la WiFi del celular (punto de acceso del teléfono), a la
  WiFi de casa o al punto de acceso de la radio.
- El navegador pide permiso de ubicación la primera vez y necesita la página en **https**.
- Manda la posición cuando el teléfono se mueve unos 50 m, y como mínimo una vez por minuto.

## Desde cualquier lugar (fuera de casa)

La forma recomendada es **Tailscale**: una red privada entre sus equipos, gratis para uso
personal, sin abrir puertos del router ni tener IP fija. La radio no corre Tailscale: lo corre
un equipo que queda prendido en su casa, en la misma red que la radio, y hace de **puente**.

### 1. Antes de empezar

- Darle a la radio una **IP fija** en el router (reserva DHCP), por ejemplo `192.168.1.50`.
- Saber la red de su casa, por ejemplo `192.168.1.0/24`.
- Crear una cuenta gratuita en tailscale.com e instalar la app **Tailscale** en el teléfono.

### 2a. Puente con una Raspberry Pi (recomendado)

Una Raspberry Pi 3, 4 o 5 con Raspberry Pi OS, conectada a la red de su casa:

```bash
curl -fsSL https://tailscale.com/install.sh | sh
echo 'net.ipv4.ip_forward = 1' | sudo tee -a /etc/sysctl.d/99-tailscale.conf
echo 'net.ipv6.conf.all.forwarding = 1' | sudo tee -a /etc/sysctl.d/99-tailscale.conf
sudo sysctl -p /etc/sysctl.d/99-tailscale.conf
sudo tailscale up --advertise-routes=192.168.1.0/24
```

(Cambiar `192.168.1.0/24` por la red de su casa.)

### 2b. Puente con una PC con Windows

1. Instalar Tailscale para Windows y entrar con la misma cuenta.
2. Abrir **Símbolo del sistema como administrador** y escribir:
   ```
   tailscale set --advertise-routes=192.168.1.0/24
   ```
3. Que la PC **no se suspenda** (Configuración → Sistema → Energía).

### 3. Aprobar la ruta

En tailscale.com → **Machines**, el equipo puente aparece con la etiqueta **Subnets**: abrirlo,
sección *Subnets*, marcar la ruta y guardar.

### 4. Usarlo

Con Tailscale encendido en el teléfono, abrir la misma dirección de siempre:
`https://192.168.1.50/remoto`. Funciona igual que en casa: el audio y la voz van por la red
privada de Tailscale.

> **Ojo con los datos móviles:** el audio de recepción usa unos 24 kB/s y la voz otro tanto
> mientras se transmite (≈ 90 MB por hora escuchando).

## Seguridad

- Cualquiera que abra la página dentro de su red (o de su red Tailscale) puede usar la radio:
  no la comparta con quien no corresponda.
- No abra puertos del router hacia la radio: use Tailscale.
- Transmitir a distancia sigue las mismas reglas que en la estación: identificarse y estar
  atento. Si algo falla, la radio deja de transmitir sola.

## Para quien programa

- Página: `webserver/src/x6100_webserver/views/remoto.html` (sin dependencias).
- Voz: `POST /link/v2/voice/start` → `{session}`; `POST /link/v2/voice/data?s=…` con PCM s16le
  mono 16 kHz (hasta 2 s por pedido); `POST /link/v2/voice/stop?s=…`. En la radio,
  `src/remote_voice.c` (socket `/tmp/x6100_voice.sock`).
- QRZ: `POST /link/v2/qrz` `{"what": "last" | "today"}`.
- El resto usa el protocolo de siempre (`/link/v2/cmd`, `/link/v2/events`, `/link/v2/audio`).
