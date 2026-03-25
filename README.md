# KProx HID Automation

KProx is a programmable BLE + USB keyboard, international keyboard, and mouse with an encrypted REST API, an on-device keepass credential store,
an scripting engine, and a community gadget library.

KProx runs on the [M5Stack AtomS3 Lite ESP32S3](https://shop.m5stack.com/products/atoms3-lite-esp32s3-dev-kit) and the
[M5Stack Cardputer Adv ESP32S3](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3).

It presents itself to a paired host as a Bluetooth or USB keyboard and mouse, then accepts input from a web interface or
REST API, letting you type into, click on, and control any HID-capable device without installing software on it.

Typical applications: unlocking unattended machines, mouse jiggling, canned text sequences, scripted UI testing,
automated form entry on devices where you cannot install software.

![KProx](web/kprox.png)

> ### ⚠  Ethical Use Warning
>
> This software is intended for personal use only, on systems and devices you own or have explicit authorization to
> operate. By using this project you accept full responsibility for how it is used.
>
> **The license granted by this project is automatically and irrevocably revoked for any use that is illegal,
> unethical, or unauthorized.**

> ### ⚠  Unverified Cryptographic Security
>
> **This project is a toy.** The cryptographic implementations have **not been audited or verified** by security
> professionals. Do not use this software to store, transmit, or protect sensitive data in any production environment.
> **USE AT YOUR OWN RISK.**

---

## References

| Document | Description |
|----------|-------------|
| [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md) | Complete token string DSL reference: all tokens, syntax, examples |
| [KEYPROX_SCRIPT_REFERENCE.md](KEYPROX_SCRIPT_REFERENCE.md) | KProx Script (.kps) language reference |
| [API_REFERENCE.md](API_REFERENCE.md) | REST API endpoint reference |

---

## Hardware

KProx supports the following development devices out-of-the-box: 
- [M5Stack AtomS3 Lite ESP32S3](https://shop.m5stack.com/products/atoms3-lite-esp32s3-dev-kit)
- [M5Stack Cardputer Adv ESP32S3](https://shop.m5stack.com/products/m5stack-cardputer-adv-version-esp32-s3)

---

## Flashing

### Prerequisites

- [PlatformIO](https://platformio.org/)
- [Node.js](https://nodejs.org/) (for build step)


Install the necessary dependencies with:
```
sudo apt update && sudo apt install -y platformio make nodejs npm bc
```

### First Flash (USB)

1. Connect via USB-C and enter DFU mode:
   - **AtomS3:** Hold side button 3-5 seconds until LED turns green.
   - **Cardputer ADV:** Hold BtnG0, then press BtnRst.
2. Run:
   ```bash
   make build
   ```
   This minifies web assets, copies reference docs into `data/`, uploads the SPIFFS filesystem image,
   then flashes the firmware.

### OTA Update

```bash
make ota
# or
make ota HOST=192.168.1.42
```

OTA binary can also be uploaded via **Settings -> OTA Firmware Update** in the web interface.

---

## Initial WiFi Setup

KProx does not host its own access point. Factory defaults:

| Setting  | Default    |
|----------|------------|
| SSID     | `kprox`    |
| Password | `1337prox` |

**First-boot flow:**

1. Create a phone hotspot with the default credentials (2.4 GHz).
2. Power on KProx. LED blinks orange while connecting, flashes green on success.
3. Open `http://kprox.local` or find the IP in your hotspot client list.
4. Go to **WiFi Settings**, enter your real network credentials, click **Connect**.
5. Update the **API Endpoint** field in the sidebar to the new hostname or IP.

---

## Security Model

### API Authentication

All API endpoints require authentication via HMAC-SHA256 over a single-use nonce (fetched from `GET /api/nonce`).
Request and response bodies are encrypted with AES-256-CTR + HMAC-SHA256 keyed from the API key.

**Default API key:** `kprox1337` -- change this immediately via the web interface sidebar.

### Credential Store

The credential store is an on-device [KeePass](https://keepass.info/) KDBX 3.1 database stored in NVS or on the SD card.
It can be opened by any compatible KeePass client (KeePassXC, KeePass 2, Strongbox, etc.).

Credentials support three fields: **password** (default), **username**, and **notes**. TOTP accounts are also stored
inside the credential store, fully encrypted at rest.

The store must be unlocked before any credential or TOTP operation is possible. Unlock via the web interface or the
CredStore Cardputer app.

**Gate modes:**

| Mode | Requirement |
|------|-------------|
| Key only | Symmetric passphrase (min 8 chars) |
| Key + TOTP | PIN then a live TOTP code |
| TOTP only | TOTP code alone; NTP sync required |

**Storage location** can be NVS (default) or SD card (`/kprox.kdbx`). Switching location while unlocked
migrates the database automatically.

**Security options:** auto-lock after inactivity, auto-wipe after N failed unlock attempts.

> Register contents are stored in plaintext. Do not put secrets in registers, use the credential store.

---

## Basic Operation (AtomS3 / Cardputer BtnG0)

| Action | Behavior |
|--------|----------|
| 1 click | Play active register |
| 2 clicks | Cycle to next register |
| Hold 5 s | Delete all registers |

---

## Cardputer Apps

Use arrow keys to navigate the launcher, ENTER to open an app, and backtick or `esc` to return to the launcher. BtnG0
returns to the launcher from any app (or plays the active register if the app does not handle it). Press TAB inside any
app to show its help page.

| App | Description |
|-----|-------------|
| **KProx** | Main register playback. Shows active register, IP, SSID, and credential store lock state. |
| **FuzzyProx** | Fuzzy search across all registers by name or content. |
| **RegEdit** | Full-screen register editor with token syntax support. |
| **CredStore** | Credential store manager: unlock, add/update credentials, rekey, configure gate mode, wipe. |
| **Gadgets** | Browse and install community gadgets from GitHub over WiFi. |
| **SinkProx** | View and flush the unauthenticated write-only sink buffer. |
| **Keyboard HID** | Direct keyboard input forwarding to the paired host. |
| **Clock** | Current time and NTP sync status. |
| **QRProx** | QR code of the device web URL. BtnG0 types the URL. |
| **SchedProx** | Scheduled task manager: create and manage time-triggered token string playback. |
| **TOTProx** | TOTP authenticator: live codes, account management, gate config. BtnG0 types code. Needs CS unlocked. |
| **Files** | SD card file browser. Arrow keys navigate, ENTER opens, `D` dumps to HID, `DEL` deletes. |
| **KPScript** | KProx Script runner. Scans SD for `.kps` files. ENTER runs, `R` rescans. |
| **BootProx** | Boot register config. Fire a register on boot with an optional auto-disable limit. |
| **TimerProx** | Countdown timer. FIRE DELAY = before register plays. HALT DELAY = before halting. No WiFi. |
| **psCombatProx** | Combat / `ps` clone. Kill enemy: `kill -9 <pid>`. You die: REISUB. |
| **Settings** | Device settings and configuration for WiFi, BT, USB HID, API Key, Identity, Sink, HID Timing, Startup App, Layout, CS, SD. |

![psCombatProx](img/combat_title.png)
![psCombatProx](img/combat2.png)
![psCombatProx Loss](img/combat_loss.png)
![psCombatProx Victory](img/combat_victory.png)

---

## Web Interface Tabs

| Tab | Description |
|-----|-------------|
| **Registers** | Create, edit, name, reorder, and play token string registers |
| **Mouse** | Trackpad for direct mouse control |
| **Settings** | WiFi, Bluetooth, USB, timing, device identity, app layout, CS security |
| **Scheduled Tasks** | Create and manage time-triggered token string tasks |
| **Code Reference** | Searchable token reference (loaded from device). See also |
| **API Reference** | Full REST API docs (loaded from device). See also [API_REFERENCE.md](API_REFERENCE.md). |
| **Credential Store** | Lock/unlock, add/delete/update credentials (password, username, notes), rekey, wipe, set |
| **TOTProx** | TOTP account management, live codes, gate configuration |
| **Gadgets** | Browse and install community gadgets from GitHub |
| **Keymap Editor** | Upload and manage custom keyboard layout JSON files |
| **File Browser** | SD card file manager: browse, create, edit, save, delete, run `.kps` scripts |
| **BootProx** | Configure and monitor the boot register: enable/disable, select register, set fire limit, reset count |
| **KPScript Editor** | Write, save, and run KProx Script files stored on the SD card |
| **KPS Reference** | KProx Script language reference (loaded from device). See also |

---

## SD Card

Recommended directory layout:

```
/
+-- scripts/          KProx Script files (.kps)
+-- config/           Configuration files read by scripts
+-- logs/             Log files written by scripts
+-- kprox.kdbx        Credential store database (when CS location = SD)
```

Token strings can read, write, and execute files on the SD card. See [TOKEN_REFERENCE.md](TOKEN_REFERENCE.md) for the
`{SD_READ}`, `{SD_WRITE}`, `{SD_APPEND}`, and `{SD_EXEC}` tokens.

---

## KProx Script

KProx Script (`.kps`) is a line-oriented scripting language that runs directly on the device. Scripts are stored on the
SD card and can be executed from a token string (`{SD_EXEC /scripts/hello.kps}`), the KPScript Cardputer app, or the
KPScript Editor web tab.

For the full language reference see [KEYPROX_SCRIPT_REFERENCE.md](KEYPROX_SCRIPT_REFERENCE.md).

**Quick example:**

```kps
# auto_login.kps
set user {CREDSTORE username mysite}
set pass {CREDSTORE mysite}
set code {TOTP mysite}

if ${user} == ""
    echo "Credential store locked{ENTER}"
    return
endif

echo "${user}{TAB}${pass}{ENTER}"
sleep 2000
echo "${code}{ENTER}"
```

---

## Gadgets

Gadgets are pre-built token string scripts in the [`gadgets/`](gadgets/) directory. Install them from the Gadgets web
tab or the Gadgets Cardputer app.

### Gadget format

```json
{
  "gadget": {
    "name": "Random Mouse Walk",
    "description": "Randomly moves the mouse around the screen forever.",
    "content": "{LOOP}{MOVEMOUSE {RAND -100 100} {RAND -100 100}}{SLEEP {RAND 1000 3000}}{ENDLOOP}"
  }
}
```

To contribute a gadget: fork the repo, add a file to `gadgets/` in the format above (keep `name` under 30 chars), test
it, and open a pull request.

---

## External Input

Pipe token strings from a shell script:

```bash
echo "{SLEEP 1000}hello world{ENTER}" | bash tools/kpipe.sh
```

---

## API Summary

All endpoints require `X-Auth: hmac-sha256(apiKey, nonce)`. See [API_REFERENCE.md](API_REFERENCE.md) for the full
reference.


## Architecture

```
src/
  token_parser.cpp          Token string DSL interpreter
  kps_parser.cpp            KProx Script interpreter
  sd_utils.cpp              SD card file utilities
  credential_store.cpp      KDBX 3.1 credential store (NVS or SD)
  totp.cpp                  TOTP (accounts stored inside credential store)
  registers.cpp             Register storage and playback
  api.cpp                   REST API
  cardputer/                Cardputer apps and UI manager

web/                        Web interface (minified into SPIFFS by build.js)
gadgets/                    Community gadget scripts (JSON)
tools/                      Shell utilities (kpipe, registers import/export, etc.)

TOKEN_REFERENCE.md          Token string DSL reference
KEYPROX_SCRIPT_REFERENCE.md KProx Script language reference
API_REFERENCE.md            REST API reference
```
