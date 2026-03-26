# KProx Token String Reference

Token strings are KProx's scripting language for keyboard and mouse automation. Plain text outside `{...}` is typed verbatim. Everything inside braces is a command token. Tokens are case-insensitive.

```
Hello {ENTER}
admin{TAB}{CREDSTORE admin_pass}{ENTER}
```

**Transport notation used in this document:**

- `BLE+USB` — produces HID output on both Bluetooth and USB
- `BLE` — Bluetooth only
- `—` — no HID output (device control only)

---

## HID Output Routing

Every token that produces HID output accepts an optional trailing transport word. When omitted, output goes to all connected transports.

| Suffix | Effect |
|--------|--------|
| `BLUETOOTH` or `BLE` | Send via BLE only |
| `USB` | Send via USB only |
| *(none)* | Send to all connected transports (default) |

The routing suffix is always the **last word** in the token and applies to all token forms: key taps, hold, press, release, mouse buttons, scroll, chords, and raw HID.

```
{ENTER BLE}
{SHIFT press USB}
{MOUSECLICK RIGHT BLUETOOTH}
{MOUSESCROLL -3 USB}
{CHORD ctrl+c BLE}
{HID 00 04 USB}
{MUTE BLE}
```

The routing suffix takes precedence over the `BLUETOOTH_HID`, `USB_HID`, `BLUETOOTH_MOUSE`, and `USB_MOUSE` enable flags — if routing says BLE but BLE is not connected, the output is silently dropped.

---

## Key Token Argument Syntax

Every keyboard key token accepts an optional argument controlling how the key is sent. `BLE+USB`

| Syntax | Behaviour |
|--------|-----------|
| `{KEY}` | Tap — press then immediately release |
| `{KEY ms}` | Hold — press, hold for *ms* milliseconds, then release |
| `{KEY press}` | Press down only, no auto-release |
| `{KEY release}` | Release only (use after a `press`) |
| `{KEY BLUETOOTH}` | Tap, BLE only |
| `{KEY press USB}` | Press down, USB only |
| `{KEY ms BLUETOOTH}` | Hold for ms, BLE only |

```
{ENTER}
{ENTER 1000}
{ENTER press}
{ENTER release}
{SHIFT press}{a}{b}{c}{SHIFT release}
{ENTER BLE}
{SHIFT press USB}{a}{b}{c}{SHIFT release USB}
```

The `ms` argument is evaluated so variables and math work: `{ENTER {MATH {hold} * 2}}`

---

## Basic Keys — BLE+USB

| Token | Aliases | Description |
|-------|---------|-------------|
| `{ENTER}` | `{RETURN}` | Enter / Return |
| `{TAB}` | | Tab |
| `{ESC}` | `{ESCAPE}` | Escape |
| `{SPACE}` | | Spacebar |
| `{BACKSPACE}` | `{BKSP}` | Backspace |
| `{DELETE}` | `{DEL}` | Delete |
| `{INSERT}` | | Insert |
| `{102ND}` | | Non-US backslash / extra key |

---

## Navigation Keys — BLE+USB

`{UP}` `{DOWN}` `{LEFT}` `{RIGHT}` `{HOME}` `{END}` `{PAGEUP}` `{PAGEDOWN}`

---

## Modifier Keys — BLE+USB

Modifier keys support all four argument forms (`press`, `release`, `ms`, tap). For simple chords use `{CHORD}` instead.

| Token | Aliases | Description |
|-------|---------|-------------|
| `{GUI}` | `{MOD}` `{WIN}` `{CMD}` `{SUPER}` `{WINDOWS}` | Left GUI (Win / Cmd / Super) |
| `{RGUI}` | `{RWIN}` `{RMETA}` | Right GUI |
| `{CTRL}` | `{LCTRL}` | Left Ctrl |
| `{RCTRL}` | | Right Ctrl |
| `{ALT}` | `{LALT}` | Left Alt |
| `{RALT}` | `{ALTGR}` | Right Alt / AltGr |
| `{SHIFT}` | `{LSHIFT}` | Left Shift |
| `{RSHIFT}` | | Right Shift |

---

## Function Keys

`{F1}`–`{F12}` — `BLE+USB`

`{F13}`–`{F24}` — `BLE` (USB host support varies by OS)

---

## Lock and Toggle Keys — BLE+USB

| Token | Aliases | Description |
|-------|---------|-------------|
| `{CAPSLOCK}` | `{CAPS}` | Caps Lock |
| `{NUMLOCK}` | | Num Lock |
| `{SCROLLLOCK}` | `{SCRLK}` | Scroll Lock |
| `{PAUSE}` | `{PAUSEBREAK}` | Pause / Break |

---

## System Keys — BLE+USB

| Token | Aliases | Description |
|-------|---------|-------------|
| `{PRINTSCREEN}` | `{PRTSC}` `{SYSRQ}` | Print Screen / SysRq |
| `{APPLICATION}` | `{MENU}` `{APP}` | Context Menu key |

---

## Numpad Keys — BLE+USB

`{KP0}`–`{KP9}` `{KPENTER}` `{KPPLUS}` `{KPMINUS}` `{KPMULTIPLY}` `{KPSTAR}` `{KPDIVIDE}` `{KPSLASH}` `{KPDOT}` `{KPDECIMAL}` `{KPEQUAL}` `{KPEQUALS}`


## International / Language Keys — BLE+USB

Keys with HID usages that cannot be encoded in the standard keyboard report. Sent via a dedicated extended report (Report ID 5).

| Token | Aliases | Description |
|-------|---------|-------------|
| `{KPCOMMA}` | `{KPJPCOMMA}` | Keypad Comma (JIS) |
| `{RO}` | | International1 — RO (JIS) |
| `{KATAKANAHIRAGANA}` | | International2 — Katakana/Hiragana toggle |
| `{YEN}` | | International3 — Yen sign (JIS) |
| `{HENKAN}` | | International4 — Henkan (convert) |
| `{MUHENKAN}` | | International5 — Muhenkan (no-convert) |
| `{HANGUEL}` | `{HANGEUL}` | Lang1 — Hanguel/Korean toggle |
| `{HANJA}` | | Lang2 — Hanja |
| `{KATAKANA}` | | Lang3 — Katakana |
| `{HIRAGANA}` | | Lang4 — Hiragana |
| `{ZENKAKUHANKAKU}` | `{ZENKAKU}` | Lang5 — Zenkaku/Hankaku toggle |

All international key tokens support the routing suffix: `{HANGUEL USB}`, `{YEN BLE}`.

---

## Media / Consumer Keys — BLE+USB

Uses the HID Consumer Control report. No `press`/`release`/`ms` arguments. Produces correct kernel events visible in `evtest`.

| Token | Aliases | Description |
|-------|---------|-------------|
| `{MUTE}` | | Toggle mute |
| `{MICMUTE}` | | Mic mute (same Consumer 0xE2 usage as `{MUTE}`; OS determines which device is affected) |
| `{VOLUMEUP}` | `{VOLUP}` | Volume up |
| `{VOLUMEDOWN}` | `{VOLDOWN}` | Volume down |
| `{PLAYPAUSE}` | | Play / Pause |
| `{NEXTTRACK}` | `{NEXT}` | Next track |
| `{PREVTRACK}` | `{PREV}` | Previous track |
| `{STOPTRACK}` | `{STOP}` | Stop playback |
| `{WWWHOME}` | `{BROWSER}` | Open browser home |
| `{EMAIL}` | | Open email client |
| `{CALCULATOR}` | `{CALC}` | Open calculator |
| `{MYCOMPUTER}` | | Open file manager |
| `{WWWSEARCH}` | | Browser search |
| `{WWWBACK}` | | Browser back |
| `{WWWFORWARD}` | | Browser forward |
| `{WWWSTOP}` | | Browser stop |
| `{WWWREFRESH}` | | Browser refresh |
| `{BOOKMARKS}` | | Open bookmarks |
| `{MEDIASEL}` | | Media selection |
| `{BRIGHTNESSUP}` | | Display brightness increase — Consumer 0x006F — `KEY_BRIGHTNESSUP` |
| `{BRIGHTNESSDOWN}` | | Display brightness decrease — Consumer 0x0070 — `KEY_BRIGHTNESSDOWN` |
| `{KBDILLUMUP}` | | Keyboard backlight increase — Consumer 0x0079 — `KEY_KBDILLUMUP` |
| `{KBDILLUMDOWN}` | | Keyboard backlight decrease — Consumer 0x0078 — `KEY_KBDILLUMDOWN` |
| `{KBDILLUMTOGGLE}` | | Keyboard backlight toggle — Consumer 0x0077 — `KEY_KBDILLUMTOGGLE` |
| `{SCREENLOCK}` | | AL Terminal Lock / Screensaver — Consumer 0x019E — `KEY_SCREENLOCK` |
| `{EJECTCD}` | | Eject — Consumer 0x00B8 — `KEY_EJECTCD` |

```
{MUTE}
{VOLUP}{SLEEP 100}{VOLUP}
{PLAYPAUSE}
{CALC}{SLEEP 800}123+456={ENTER}
```

---

## AC Keys (Application Control) — BLE+USB

Sent via the keyboard report (HID Usage Page 0x07). These are native key codes recognised by Linux, macOS, and Windows without any modifier combination.

| Token | HID Usage | `evtest` event |
|-------|-----------|----------------|
| `{UNDO}` | 0x7A | `KEY_UNDO` |
| `{REDO}` | 0x79 | `KEY_REDO` |
| `{CUT}` | 0x7B | `KEY_CUT` |
| `{COPY}` | 0x7C | `KEY_COPY` |
| `{PASTE}` | 0x7D | `KEY_PASTE` |
| `{FIND}` | 0x7E | `KEY_FIND` |
| `{HELP}` | 0x75 | `KEY_HELP` |

All AC key tokens accept the optional routing suffix and `press`/`release`/`ms` argument:

```
{COPY}
{PASTE USB}
{UNDO press}{SLEEP 500}{UNDO release}
```

---

## System Power / Sleep / Wake Keys — BLE+USB

Uses the HID Generic Desktop / System Control report. No `press`/`release`/`ms` arguments.

| Token | Aliases | Description |
|-------|---------|-------------|
| `{SYSTEMPOWER}` | `{SYSPOWER}` `{POWERDOWN}` | System power down |
| `{SYSTEMSLEEP}` | `{SYSSLEEP}` | System sleep |
| `{SYSTEMWAKE}` | `{SYSWAKE}` `{WAKEUP}` | System wake up |

```
{SYSTEMSLEEP}
{SYSTEMPOWER}
```

---

## Release All Keys

| Token | Description |
|-------|-------------|
| `{RELEASEALL}` | Zero all keyboard, consumer, and system reports on both BLE and USB |
| `{RELEASEALL_BLE}` | Same, BLE only |
| `{RELEASEALL_USB}` | Same, USB only |

```
{CTRL press}{ALT press}{DELETE}{RELEASEALL}
{SHIFT press BLE}{a}{b}{c}{RELEASEALL_BLE}
```

---

## Key Chords — BLE+USB

`{CHORD modifier+key}` — presses modifiers and key simultaneously, then auto-releases all.

**Modifier prefixes** (chain with `+`):

`CTRL+` `LCTRL+` `RCTRL+` `ALT+` `LALT+` `RALT+` `ALTGR+` `SHIFT+` `LSHIFT+` `RSHIFT+` `GUI+` `MOD+` `WIN+` `CMD+` `SUPER+` `WINDOWS+`

The key after modifiers can be any letter, digit, or named key: `ENTER` `RETURN` `SPACE` `TAB` `ESC` `ESCAPE` `DELETE` `DEL` `BACKSPACE` `BKSP` `LEFT` `RIGHT` `UP` `DOWN` `HOME` `END` `PAGEUP` `PAGEDOWN` `INSERT` `PRINTSCREEN` `PRTSC` `SYSRQ` `CAPSLOCK` `CAPS` `NUMLOCK` `SCROLLLOCK` `SCRLK` `PAUSE` `PAUSEBREAK` `APPLICATION` `MENU` `APP` `F1`–`F24` `KP0`–`KP9` `KPENTER` `KPPLUS` `KPMINUS` `KPMULTIPLY` `KPSTAR` `KPDIVIDE` `KPSLASH` `KPDOT` `KPDECIMAL`

```
{CHORD CTRL+C}
{CHORD CTRL+ALT+DELETE}
{CHORD GUI+R}
{CHORD MOD+L}
{CHORD RALT+E}
{CHORD ALT+F4}
{CHORD CTRL+SHIFT+ESC}
{CHORD ALT+SYSRQ+B}
```

---

## Raw HID Keycodes — BLE+USB

`{HID keycode}` — raw HID keyboard/keypad usage code (hex or decimal). Bypasses key translation.

`{HID modifier keycode ...}` — modifier byte + up to 6 key codes.

Modifier bitmask: `0x01`=LCtrl `0x02`=LShift `0x04`=LAlt `0x08`=LGUI `0x10`=RCtrl `0x20`=RShift `0x40`=RAlt `0x80`=RGUI

```
{HID 0x28}
{HID 0x02 0x04}
```

---

## Mouse Control — BLE+USB

| Token | Description |
|-------|-------------|
| `{SETMOUSE x y}` | Move to absolute position |
| `{MOVEMOUSE dx dy}` | Relative movement |
| `{MOUSECLICK}` | Left click |
| `{MOUSECLICK button}` | Click the specified button |
| `{MOUSEDOUBLECLICK}` | Double left click |
| `{MOUSEDOUBLECLICK button}` | Double-click the specified button |
| `{MOUSEPRESS button}` | Press and hold the specified button |
| `{MOUSERELEASE button}` | Release held button |
| `{MOUSESCROLL amount}` | Scroll vertically (positive = down, negative = up) |
| `{MOUSEHSCROLL amount}` | Scroll horizontally (positive = right, negative = left) |

All mouse tokens accept an optional trailing routing suffix. The suffix follows all other arguments.

```
{MOUSECLICK RIGHT BLE}
{MOUSESCROLL -3 USB}
{MOUSEPRESS BACK BLUETOOTH}
{SETMOUSE 400 300 BLE}
{MOVEMOUSE 10 0 USB}
```

**Button values** — named aliases and numeric bitmasks are both accepted:

| Name | Aliases | Bitmask | Description |
|------|---------|---------|-------------|
| `LEFT` | `L` | `1` | Primary button |
| `RIGHT` | `R` | `2` | Secondary button |
| `MIDDLE` | `M` | `4` | Middle / scroll-wheel click |
| `BACK` | `B` | `8` | Browser back (BTN_SIDE) |
| `FORWARD` | `F` | `16` | Browser forward (BTN_EXTRA) |

Named aliases are case-insensitive. Numeric bitmasks can be OR'd for chords (e.g. `3` = left+right simultaneously).

```
{MOUSECLICK RIGHT}
{MOUSEPRESS BACK}{SLEEP 200}{MOUSERELEASE BACK}
{MOUSESCROLL -5}
{MOUSEHSCROLL 3}
{MOUSECLICK 4}
```

---

## Timing — —

| Token | Description |
|-------|-------------|
| `{SLEEP ms}` | Pause for *ms* milliseconds |
| `{SCHEDULE HH:MM}` | Wait until wall-clock time (NTP required) |
| `{SCHEDULE HH:MM:SS}` | Wait until exact second |

---

## Loops — —

| Token | Description |
|-------|-------------|
| `{LOOP}...{ENDLOOP}` | Infinite loop |
| `{LOOP ms}...{ENDLOOP}` | Timed loop |
| `{LOOP var start inc end}...{ENDLOOP}` | Counter loop |
| `{FOR var start inc end}...{ENDFOR}` | Counter loop (named variable) |
| `{WHILE cond}...{ENDWHILE}` | Condition loop |
| `{BREAK}` | Exit enclosing loop |
| `{BREAK var value}` | Exit loop when `var == value` |

```
{LOOP i 1 1 5}Line {i}{ENTER}{ENDLOOP}
{WHILE {x} < 10}{SET x {MATH {x}+1}}{x}{ENTER}{ENDWHILE}
```

---

## Variables — —

`{SET varname value}` — assign. Reference anywhere with `{varname}`.

Loop counter variables are scoped to their loop. `{SET}` variables persist for the lifetime of the current execution.

```
{SET n 5}
{SET n {MATH {n} + 1}}
```

---

## Conditionals — —

```
{IF left op right}...{ELSE}...{ENDIF}
```

Operators: `==` `!=` `<` `>` `<=` `>=`

---

## Math — —

`{MATH expression}` — evaluates and outputs the result.

Operators: `+` `-` `*` `/` `%`
Functions: `sin(x)` `cos(x)` `tan(x)` `sqrt(x)` `abs(x)` `floor(x)` `ceil(x)` `round(x)`
Constants: `PI` `E`

---

## Random Numbers — BLE+USB

All random output uses `mbedtls_ctr_drbg_random` seeded from the hardware entropy source — cryptographically secure pseudo-random numbers.

`{RAND}` — raw unsigned 32-bit integer from the CSPRNG (0–4294967295).

`{RAND min max}` — cryptographically random integer in [min, max] inclusive.

```
{RAND}
{RAND 1000 9999}
{SET pin {RAND 1000 9999}}{pin}
{LOOP}{MOVEMOUSE {RAND -50 50} {RAND -50 50}}{SLEEP {RAND 1000 3000}}{ENDLOOP}
```

---

## ASCII / Raw Character Output — BLE+USB

`{ASCII value}` / `{RAW value}` — output character by decimal or `0x` hex code.

---

## Credential Store — BLE+USB

`{CREDSTORE label}` — resolves to the **password** field of the named credential from the encrypted on-device store. Resolves to an empty string when the store is locked or the credential does not exist.

`{CREDSTORE field label}` — resolves to a specific field of the credential. `field` is case-insensitive and must be one of:

| Field | Description |
|-------|-------------|
| `password` | The password / secret value (default when no field is given) |
| `username` | The username / login name |
| `notes` | The notes field |

```
{CREDSTORE github}
{CREDSTORE password github}
{CREDSTORE username github}
{CREDSTORE notes work-vpn}
{CREDSTORE username corp}{TAB}{CREDSTORE corp}{ENTER}
```

---

## Quoted Arguments

Multi-argument tokens accept both bare words and double-quoted strings. Quotes are only needed when
an argument contains spaces; simple values work without them.

```
{STR_REPLACE hello world earth}           ← three bare words
{STR_REPLACE "hello world" world earth}   ← first arg quoted (contains a space)
{STR_CONTAINS "hello world" "lo wo"}      ← both args quoted
{SD_WRITE /log.txt "line one here"}       ← content quoted
{PAD_LEFT 10 " " {CREDSTORE username x}}  ← pad char is a quoted space
```

Escape sequences inside quoted strings:

| Escape | Meaning |
|--------|---------|
| `\"` | Literal double-quote |
| `\n` | Newline |
| `\t` | Tab |
| `\\` | Literal backslash |

Tokens that take a **single** argument (e.g. `{STR_UPPER text}`, `{SHA256 text}`, `{URL_ENCODE text}`,
`{BASE64 text}`) do not need quotes because their entire argument is treated as one string. Quoting
still works and is useful if the value contains leading/trailing spaces you want to preserve.

The following tokens support quoted arguments:

`{STR_SLICE}`, `{STR_REPLACE}`, `{STR_CONTAINS}`, `{STR_STARTS}`, `{STR_ENDS}`, `{STR_INDEX}`,
`{STR_REPEAT}`, `{PAD_LEFT}`, `{PAD_RIGHT}`, `{REPEAT}`, `{SD_WRITE}`, `{SD_APPEND}`

---

## String Functions — BLE+USB

All string function arguments are token-evaluated before the function runs, so `{CREDSTORE}`, `{SD_READ}`, variables, and other tokens can be composed freely.

| Token | Description |
|-------|-------------|
| `{STR_UPPER text}` | Convert to upper case |
| `{STR_LOWER text}` | Convert to lower case |
| `{STR_LEN text}` | Length in characters |
| `{STR_TRIM text}` | Strip leading and trailing whitespace |
| `{STR_SLICE text start end}` | Substring; indices are 0-based and exclusive at `end`; negative values count from the end |
| `{STR_REPLACE text find replacement}` | Replace every occurrence of `find` with `replacement` |
| `{STR_CONTAINS text substring}` | `1` if `substring` is found, `0` otherwise |
| `{STR_STARTS text prefix}` | `1` if `text` starts with `prefix` |
| `{STR_ENDS text suffix}` | `1` if `text` ends with `suffix` |
| `{STR_INDEX text substring}` | 0-based index of first occurrence; `-1` if not found |
| `{STR_REVERSE text}` | Reverse the string character by character |
| `{STR_REPEAT text n}` | Repeat `text` n times (max 200) |

```
{STR_UPPER {CREDSTORE username github}}
{STR_LEN {CREDSTORE password github}}
{STR_SLICE mystring 0 4}
{STR_REPLACE {SD_READ /config/template.txt} __IP__ {KPROX_IP}}
{STR_CONTAINS {WIFI_SSID} Office}
{STR_REPEAT - 40}
```

---

## REPEAT — BLE+USB

`{REPEAT n text}` — evaluates `text` n times and concatenates the results (max 200 iterations). Unlike `{STR_REPEAT}`, `text` is token-evaluated on each iteration, so tokens with side-effects (e.g. `{RAND}`) produce a different value each time.

```
{REPEAT 3 -{ENTER}}
{REPEAT 5 {RAND 1 9}}
```

---

## Padding — BLE+USB

`{PAD_LEFT width char text}` — left-pad `text` with `char` until total length reaches `width`. No-op if text is already at or beyond `width`.

`{PAD_RIGHT width char text}` — same, padding on the right.

```
{PAD_LEFT 6 0 {REGISTER_COUNT}}
{PAD_RIGHT 20 . {CREDSTORE username github}}
{PAD_LEFT 8 0 {MATH {UPTIME} mod 100000}}
```

---

## URL Encoding — BLE+USB

`{URL_ENCODE text}` — percent-encodes `text` per RFC 3986. Unreserved characters (`A–Z a–z 0–9 - _ . ~`) are passed through unchanged; all other bytes become `%XX`.

```
{URL_ENCODE {CREDSTORE password github}}
http://{KPROX_IP}/api?key={URL_ENCODE {apiKey}}
```

---

## Base64 — BLE+USB

`{BASE64 text}` — encodes `text` as standard Base64 (RFC 4648, alphabet `A–Z a–z 0–9 + /`, `=` padding). Uses `mbedtls_base64_encode` internally.

`{BASE64_DECODE text}` — decodes a Base64 string back to its original bytes. Invalid input resolves to an empty string.

```
{BASE64 hello world}
{SET encoded {BASE64 {CREDSTORE password github}}}
{BASE64_DECODE SGVsbG8gV29ybGQ=}
```

---

## SHA-256 — BLE+USB

`{SHA256 text}` — resolves to the lower-case hex-encoded SHA-256 digest of `text` (64 hex characters). Uses `mbedtls_sha256` internally — the same library already linked for API authentication.

```
{SHA256 hello}
{SET fingerprint {SHA256 {CREDSTORE password corp}}}{fingerprint}{ENTER}
{SHA256 {SD_READ /keys/token.txt}}
```

---

## Device Info — BLE+USB

| Token | Description |
|-------|-------------|
| `{KPROX_IP}` | Current WiFi IP address; empty string when not connected |
| `{WIFI_SSID}` | Connected network name; empty string when not connected |
| `{WIFI_RSSI}` | WiFi signal strength in dBm; empty string when not connected |
| `{WIFI_STATE}` | `1` when WiFi is connected, `0` otherwise |
| `{NTP_STATE}` | `1` when the system clock has been set via NTP, `0` otherwise |
| `{CREDSTORE_STATE}` | `1` when the credential store is unlocked, `0` when locked |
| `{FREE_HEAP}` | Free heap memory in bytes |
| `{UPTIME}` | Seconds since last boot |
| `{BATTERY}` | Battery level 0-100 (Cardputer only) |
| `{REGISTER_COUNT}` | Number of registers currently loaded |
| `{REGISTER_NAME n}` | Name of register n (1-based); empty string if unnamed or out of range |
| `{ACTIVE_REGISTER}` | Index of the currently active register (1-based) |

```
http://{KPROX_IP}
SSID: {WIFI_SSID}  RSSI: {WIFI_RSSI} dBm
WiFi: {WIFI_STATE}  NTP: {NTP_STATE}  CredStore: {CREDSTORE_STATE}
Heap: {FREE_HEAP} bytes  Uptime: {UPTIME}s  Registers: {REGISTER_COUNT}
Battery: {BATTERY}%
```

---

## Sink — BLE+USB

| Token | Description |
|-------|-------------|
| `{SINK}` | Resolves to the current content of the sink buffer (read without flushing) |
| `{SINK_SIZE}` | Resolves to the current sink buffer size in bytes as a plain integer |

```
{SET captured {SINK}}
Buffer has {SINK_SIZE} bytes
```

---

## Time and Date — BLE+USB

| Token | Description |
|-------|-------------|
| `{TIMESTAMP}` | Unix epoch as a plain integer string |
| `{TIME}` | Current time typed as `HH:MM:SS` |
| `{DATE}` | Current date as `YYYY-MM-DD` |
| `{DATE +format}` | Current date/time formatted with `strftime` directives |

`{DATE}` and `{DATE +format}` use the same format directives as the Linux `date` command.

```
{TIMESTAMP}
{TIME}
{DATE}
{DATE +"%Y-%m-%dT%H:%M:%SZ"}
{DATE +"%d/%m/%Y"}
{DATE +"%H:%M"}
{SET ts {DATE +"%Y%m%d_%H%M%S"}}log_{ts}.txt
```

Common directives: `%Y` year · `%m` month · `%d` day · `%H` hour · `%M` minute · `%S` second · `%s` Unix epoch · `%A` weekday name · `%B` month name.

Requires NTP sync. If time is not yet synced the output reflects the Unix epoch origin (1970-01-01T00:00:00Z).

---

## Network — —

`{WIFI_WAIT}` — blocks execution until WiFi is connected or the user aborts (ESC / BtnA). `{WIFI_WAIT}` is a retained alias.

`{NTP_WAIT}` — blocks execution until the system clock has been synchronised via NTP, or the user aborts (ESC / BtnA). The validity sentinel is `epoch > 2001-01-01 00:00:00 UTC`; the same threshold used by `{NTP_STATE}`. Combine with `{WIFI_WAIT}` when WiFi is not guaranteed to be up before the script runs.

`{CREDSTORE_WAIT}` — blocks execution until the on-device credential store is unlocked by the user, or execution is aborted (ESC / BtnA). Use before any `{CREDSTORE …}` or `{TOTP …}` tokens in unattended scripts that run at boot before the user has had a chance to unlock the store.

```
{WIFI_WAIT}{TOTP mysite}{ENTER}
{WIFI_WAIT}{NTP_WAIT}{DATE %Y-%m-%d %H:%M:%S}{ENTER}
{CREDSTORE_WAIT}{CREDSTORE username corp}{TAB}{CREDSTORE corp}{ENTER}
{CREDSTORE_WAIT}{WIFI_WAIT}{NTP_WAIT}{TOTP mysite}{ENTER}
```

---

## Device Management — BLE+USB

`{DEVICE_REBOOT}` — immediately reboots the KProx device via `ESP.restart()`. Any pending HID output is flushed first. Useful at the end of a provisioning script that changes settings which take effect only after a restart (e.g. WiFi credentials, USB identity, BLE toggle).

```
{DEVICE_SETTINGS set wifi.ssid MyNetwork}{DEVICE_SETTINGS set wifi.password s3cr3t}{DEVICE_REBOOT}
```

---

`{DEVICE_SETTINGS get <label>}` — types the current value of the named setting as HID keyboard output. Label matching is case-insensitive. Sensitive settings (`wifi.password`, `api_key`) return `"Ah Ah Ah, You Didn't Say The Magic Word."` instead of their actual value.

`{DEVICE_SETTINGS set <label> <value>}` — updates the named setting in memory and persists it to NVS immediately. Invalid values (wrong type, out-of-range) are silently ignored. Settings that require a reboot to take effect (WiFi credentials, USB identity, BLE enable) are noted below.

`{DEVICE_SETTINGS_REPORT}` — types a complete human-readable summary of every setting and its current value via HID, one `label: value` line per setting followed by a CRLF. Sensitive settings are redacted. Useful for auditing a device over an HID session or logging to a file via `{SD_WRITE}`.

### Settings labels

| Label | Type | Description | Reboot? |
|-------|------|-------------|---------|
| `wifi.enabled` | bool (0/1) | WiFi enabled at boot | Yes |
| `wifi.ssid` | string | WiFi network name | Yes |
| `wifi.password` | string | WiFi password | Yes |
| `bt.enabled` | bool (0/1) | Bluetooth enabled at boot | Yes |
| `bt.keyboard` | bool (0/1) | BLE keyboard report enabled | Yes |
| `bt.mouse` | bool (0/1) | BLE mouse report enabled | Yes |
| `bt.intl_keyboard` | bool (0/1) | BLE extended keyboard report enabled | Yes |
| `usb.enabled` | bool (0/1) | USB HID enabled at boot (Cardputer) | Yes |
| `usb.keyboard` | bool (0/1) | USB keyboard sub-device enabled | Yes |
| `usb.mouse` | bool (0/1) | USB mouse sub-device enabled | Yes |
| `usb.intl_keyboard` | bool (0/1) | USB extended keyboard report enabled | Yes |
| `usb.manufacturer` | string | USB manufacturer string | Yes |
| `usb.product` | string | USB product string | Yes |
| `api_key` | string | REST API authentication key | No |
| `hostname` | string | mDNS/network hostname | Yes |
| `keymap` | string | Active keyboard layout (e.g. `en`, `de`, `dvorak`) | No |
| `led.enabled` | bool (0/1) | Status LED enabled | No |
| `led.r` | int 0–255 | LED red component | No |
| `led.g` | int 0–255 | LED green component | No |
| `led.b` | int 0–255 | LED blue component | No |
| `utc_offset` | int (seconds) | UTC timezone offset in seconds | No |
| `sink.max_size` | int (bytes) | Maximum sink buffer size; 0 = unlimited | No |
| `timing.key_press` | int (ms) | Key press hold duration | No |
| `timing.key_release` | int (ms) | Key release delay | No |
| `timing.between_keys` | int (ms) | Delay between key strokes | No |
| `timing.between_send` | int (ms) | Delay between `sendText` calls | No |
| `timing.special_key` | int (ms) | Delay after special keys | No |
| `timing.token` | int (ms) | Delay between token evaluations | No |
| `display.brightness` | int 16–255 | Display backlight brightness (Cardputer) | No |
| `display.timeout_ms` | int 5000–60000 | Screen timeout in milliseconds (Cardputer) | No |
| `cs.auto_lock_secs` | int (seconds) | Credential store auto-lock timeout; 0 = disabled | No |
| `cs.auto_wipe_attempts` | int | Failed unlock attempts before wipe; 0 = disabled | No |
| `cs.storage` | `nvs` / `sd` | Credential store backend | Yes |
| `boot_reg.enabled` | bool (0/1) | Boot register fire enabled | No |
| `boot_reg.index` | int (0-based) | Register index to fire on boot | No |
| `boot_reg.limit` | int | Max times to fire; 0 = every boot | No |
| `default_app` | int (1-based) | Cardputer app to launch on boot | No |
| `mtls.enabled` | bool (0/1) | mTLS enabled (read-only — manage via web UI) | — |

Bool settings accept `1`/`0` or `true`/`false`. `usb.*` settings are only present on builds with USB HID support.

**New settings added to the firmware are automatically included** in `{DEVICE_SETTINGS_REPORT}` and accessible via `get`/`set` — no token changes are required.

```
{DEVICE_SETTINGS get wifi.ssid}
{DEVICE_SETTINGS set led.enabled 0}
{DEVICE_SETTINGS set timing.between_keys 10}
{DEVICE_SETTINGS_REPORT}
{SD_WRITE "/audit.txt" {DEVICE_SETTINGS_REPORT}}
{SET cur_map {DEVICE_SETTINGS get keymap}}{cur_map}{ENTER}
```

---

## QR Code — Cardputer only

`{QR text}` — renders a QR code for `text` on the Cardputer display and blocks until any key or BtnG0 is pressed, then restores the screen. The text is token-evaluated before rendering. QR version is auto-selected (3–10, ECC Low) to fit the content. Silently no-ops on non-Cardputer builds.

```
{QR http://{KPROX_IP}}
{QR {CREDSTORE password github}}
{SET otp {TOTP mysite}}{QR {otp}}
```

---

## TOTP — BLE+USB

`{TOTP name}` — resolves to the current 6-digit RFC 6238 TOTP code for the named account stored in TOTProx. Resolves to an empty string if the account does not exist or if NTP time is not yet synced. The `name` is case-insensitive.

Requires WiFi + NTP sync to have occurred before use. The code is computed at the moment the token is evaluated, so it is valid for the remainder of the current 30-second window.

```
{CREDSTORE username}{TAB}{CREDSTORE password}{TAB}{TOTP github}{ENTER}
{TOTP work-vpn}
{SET code {TOTP myaccount}}{code}{ENTER}
```

Add accounts via the **TOTProx** app (see [CARDPUTER_APPS.md](CARDPUTER_APPS.md)) or the web interface (TOTProx tab). Account secrets are encrypted on-device using the credential store key (`kprox_totp` NVS namespace).

**The credential store must be unlocked** to view accounts, add accounts, delete accounts, or evaluate `{TOTP name}` tokens. When the store is locked, `{TOTP name}` resolves to an empty string.

---

## Register Control — —

`{SET_ACTIVE_REGISTER arg}` — sets the active register by name/description or 1-based index. The `arg` is matched case-insensitively against register names; if `arg` is a plain integer it is treated as a 1-based index. If no matching register is found the token is a no-op.

`{REGISTER_COUNT}` — resolves to the number of registers currently loaded.

`{REGISTER_NAME n}` — resolves to the name of register n (1-based). Returns an empty string if the register is unnamed or n is out of range.

```
Registers: {REGISTER_COUNT}
{REGISTER_NAME 1}
{FOR i 1 {REGISTER_COUNT}}{REGISTER_NAME {i}}{ENTER}{ENDFOR}
```

`{PLAY_REGISTER arg}` — immediately executes the token string stored in the matched register. Uses the same name/index matching as `SET_ACTIVE_REGISTER`. No-op if no match.

`{EXEC arg}` — like `PLAY_REGISTER` but passes the **current variable scope** into the register, making it behave as a sub-routine. Variables set inside the register persist in the caller's scope after it returns.

```
{SET_ACTIVE_REGISTER Endless Mouse Square}
{SET_ACTIVE_REGISTER 1}
{PLAY_REGISTER Endless Mouse Square}
{PLAY_REGISTER 1}
{EXEC login_helper}
{SET user admin}{EXEC fill_login_form}
```

---

## SD Card File Access — —

Requires an SD card inserted. All paths are absolute from the SD root (e.g. `/scripts/login.kps`). Parent directories are created automatically for write operations.

| Token | Description |
|-------|-------------|
| `{SD_LS}` | Newline-delimited filenames in the SD root; dirs suffixed with `/` |
| `{SD_LS path}` | Same, for the named directory |
| `{SD_READ path}` | Resolves to the full text content of the file |
| `{SD_WRITE path content}` | Create or overwrite a file with `content` (no output) |
| `{SD_APPEND path content}` | Append `content` to a file, creating it if absent (no output) |
| `{SD_EXEC path}` | Execute a `.kps` script file from the SD card |

Paths can be quoted or bare. Content can be a quoted string or a token expression.

```
{SD_LS}
{SD_LS /scripts}
{SET files {SD_LS /logs}}
{SD_READ /config/hostname.txt}
{SET ip {SD_READ /config/ip.txt}}{ip}{ENTER}
{SD_WRITE "/logs/access.log" "Login attempt from {KPROX_IP}"}
{SD_APPEND /logs/access.log {KPROX_IP} logged in{ENTER}}
{SD_EXEC /scripts/auto_login.kps}
```

---

## KProx Script (.kps) — —

KProx Script is a line-oriented programming language interpreted directly on the device. Scripts are stored on the SD card as `.kps` files and executed with `{SD_EXEC path}` or via the token `{SD_EXEC}`. Every `{TOKEN}` from the token reference works inside KPS string expressions.

### Language overview

```kps
# Comments start with #

# Variables
set name "Alice"
set count 5
set greeting "Hello ${name}"

# Echo sends the evaluated string to HID (keyboard output)
echo "Hello ${name}{ENTER}"
echo {CREDSTORE password github}

# type is an alias for echo
type "${greeting}{TAB}"

# Sleep (milliseconds)
sleep 500

# Single key by name
key ENTER
key TAB
key F5

# Key chord
chord CTRL+C
chord GUI+R

# Run a raw token string through the full token parser
run "{CHORD CTRL+ALT+T}{SLEEP 800}htop{ENTER}"

# If / elif / else / endif
if ${count} == 5
    echo "five"
elif ${count} > 5
    echo "big"
else
    echo "small"
endif

# Counter loop: for var start step end
for i 1 1 10
    echo "${i} "
endfor

# Decrement loop
for i 10 -1 1
    echo "${i}{ENTER}"
endfor

# While loop
while ${count} > 0
    set count {math ${count} - 1}
endwhile

# Infinite loop (break with ESC / BtnA, or use break)
loop
    echo "."
    sleep 1000
endloop

# Timed loop (ms)
loop 10000
    chord CTRL+ALT+DELETE
    sleep 500
endloop

# Break out of the innermost loop
break

# Return from the current script (does not propagate to caller)
return

# Include and execute another KPS file (shares variable scope)
include "/scripts/helper.kps"

# SD file operations
sd_write "/logs/run.log" "Started at ${count}"
sd_append "/logs/run.log" "Done"
set cfg {sd_read "/config/profile.txt"}

# WiFi connect
wifi_connect "MySSID" "MyPassword"

# Register operations
play_register "Endless Mouse"
play_register 2
set_active_register 1

# Halt / resume
halt
resume

# Inline token expressions — all {TOKEN} tokens work in any string context
set pass {CREDSTORE password github}
set code {TOTP github}
set ip   {KPROX_IP}
echo "${pass}{TAB}${code}{ENTER}"
```

### Expressions and variable substitution

Variables are referenced as `${varname}` inside any string argument. The entire `{TOKEN}` system (CREDSTORE, TOTP, MATH, RAND, SD_READ, etc.) is available inside quoted strings and bare word arguments:

```kps
set n {math ${n} + 1}
set rand {RAND 1000 9999}
set secret {CREDSTORE password mysite}
echo "Code: {TOTP mysite}{ENTER}"
```

### Condition operators

`==`  `!=`  `<`  `>`  `<=`  `>=` — string or numeric comparison (numeric when both sides parse as numbers).

---

## Keymap — —

`{KEYMAP id}` — switch keyboard layout for the remainder of execution. `en` is always available.

```
{KEYMAP de}Hallo Welt{ENTER}
```

---

## System Control — —

| Token | Description |
|-------|-------------|
| `{HALT}` | Stop all execution |
| `{RESUME}` | Resume halted execution |
| `{RELEASEALL}` | Release all held keys (BLE+USB) |
| `{BLUETOOTH_ENABLE}` | Enable BLE HID (persisted) |
| `{BLUETOOTH_DISABLE}` | Disable BLE HID (persisted) |
| `{USB_ENABLE}` | Enable USB HID (persisted) |
| `{USB_DISABLE}` | Disable USB HID (persisted) |
| `{WIFI ssid password}` | Connect to WiFi |
| `{SINKPROX}` | Flush and execute the SinkProx buffer |

---

## HID Output Routing — —

These tokens selectively enable or disable HID output channels **for the duration of the current token string only**. The original state is automatically restored when the string finishes executing. Use them to target output at a specific transport without permanently changing device settings.

`value` is any of: `true` / `false` / `1` / `0` / `enabled` / `disabled` / `on` / `off` (case-insensitive).

| Token | Description |
|-------|-------------|
| `{BLUETOOTH_HID value}` | Enable or disable **all** BLE output (keyboard + mouse) |
| `{BLUETOOTH_KEYBOARD value}` | Enable or disable BLE keyboard output only |
| `{BLUETOOTH_MOUSE value}` | Enable or disable BLE mouse output only |
| `{USB_HID value}` | Enable or disable **all** USB output (keyboard + mouse) |
| `{USB_KEYBOARD value}` | Enable or disable USB keyboard output only |
| `{USB_MOUSE value}` | Enable or disable USB mouse output only |

Setting `BLUETOOTH_HID false` is equivalent to setting both `BLUETOOTH_KEYBOARD false` and `BLUETOOTH_MOUSE false`.

```
{BLUETOOTH_HID false}Hello{ENTER}
{USB_HID false}{LOOP}{MOVEMOUSE {RAND -50 50} {RAND -50 50}}{SLEEP 500}{ENDLOOP}
{USB_KEYBOARD false}{BLUETOOTH_KEYBOARD true}secret{ENTER}
{BLUETOOTH_MOUSE false}{SETMOUSE 400 300}{MOUSECLICK}
```

---

## Escape Sequences

Inside plain text (outside `{}`):

`\n` newline · `\t` tab · `\r` CR · `\\` backslash · `\{` literal `{` · `\}` literal `}`

---

## Examples

```
{CHORD GUI+R}{SLEEP 500}notepad{ENTER}
{CHORD MOD+L}
{PLAYPAUSE}
{VOLUP}{SLEEP 100}{VOLUP}{SLEEP 100}{VOLUP}
{CALC}{SLEEP 800}123+456={ENTER}
{SYSTEMSLEEP}
{ENTER 2000}
{SHIFT press}{F10}{SHIFT release}
{RELEASEALL}
{CREDSTORE corp_user}{TAB}{CREDSTORE corp_pass}{ENTER}
{LOOP}{MOVEMOUSE {RAND -50 50} {RAND -50 50}}{SLEEP {RAND 1000 3000}}{ENDLOOP}
{CHORD ALT+SYSRQ+R}{SLEEP 2000}{CHORD ALT+SYSRQ+E}{SLEEP 2000}{CHORD ALT+SYSRQ+I}{SLEEP 2000}{CHORD ALT+SYSRQ+S}{SLEEP 2000}{CHORD ALT+SYSRQ+U}{SLEEP 2000}{CHORD ALT+SYSRQ+B}
```

---

## HID Keyboard / Keypad Usage Codes

These are the raw HID Usage ID values (Usage Page 0x07, Keyboard/Keypad). Use them with `{HID 0xNN}` or `{HID modifier 0xNN}`. The **Arduino Encoding** column shows the value to use when calling `hidPress()` directly: `0x88 + usage` for all non-modifier keys (values ≥ 0x88 are decoded as `value - 0x88`).

| Key | Usage (hex) | Usage (dec) | Arduino Enc | Notes |
|-----|-------------|-------------|-------------|-------|
| A | 0x04 | 4 | 0x8C | |
| B | 0x05 | 5 | 0x8D | |
| C | 0x06 | 6 | 0x8E | |
| D | 0x07 | 7 | 0x8F | |
| E | 0x08 | 8 | 0x90 | |
| F | 0x09 | 9 | 0x91 | |
| G | 0x0A | 10 | 0x92 | |
| H | 0x0B | 11 | 0x93 | |
| I | 0x0C | 12 | 0x94 | |
| J | 0x0D | 13 | 0x95 | |
| K | 0x0E | 14 | 0x96 | |
| L | 0x0F | 15 | 0x97 | |
| M | 0x10 | 16 | 0x98 | |
| N | 0x11 | 17 | 0x99 | |
| O | 0x12 | 18 | 0x9A | |
| P | 0x13 | 19 | 0x9B | |
| Q | 0x14 | 20 | 0x9C | |
| R | 0x15 | 21 | 0x9D | |
| S | 0x16 | 22 | 0x9E | |
| T | 0x17 | 23 | 0x9F | |
| U | 0x18 | 24 | 0xA0 | |
| V | 0x19 | 25 | 0xA1 | |
| W | 0x1A | 26 | 0xA2 | |
| X | 0x1B | 27 | 0xA3 | |
| Y | 0x1C | 28 | 0xA4 | |
| Z | 0x1D | 29 | 0xA5 | |
| 1 / ! | 0x1E | 30 | 0xA6 | |
| 2 / @ | 0x1F | 31 | 0xA7 | |
| 3 / # | 0x20 | 32 | 0xA8 | |
| 4 / $ | 0x21 | 33 | 0xA9 | |
| 5 / % | 0x22 | 34 | 0xAA | |
| 6 / ^ | 0x23 | 35 | 0xAB | |
| 7 / & | 0x24 | 36 | 0xAC | |
| 8 / * | 0x25 | 37 | 0xAD | |
| 9 / ( | 0x26 | 38 | 0xAE | |
| 0 / ) | 0x27 | 39 | 0xAF | |
| Enter | 0x28 | 40 | 0xB0 | `{ENTER}` |
| Escape | 0x29 | 41 | 0xB1 | `{ESC}` |
| Backspace | 0x2A | 42 | 0xB2 | `{BACKSPACE}` |
| Tab | 0x2B | 43 | 0xB3 | `{TAB}` |
| Space | 0x2C | 44 | 0xB4 | `{SPACE}` |
| - / _ | 0x2D | 45 | 0xB5 | |
| = / + | 0x2E | 46 | 0xB6 | |
| [ / { | 0x2F | 47 | 0xB7 | |
| ] / } | 0x30 | 48 | 0xB8 | |
| \ / | | 0x31 | 49 | 0xB9 | |
| ; / : | 0x33 | 51 | 0xBB | |
| ' / " | 0x34 | 52 | 0xBC | |
| ` / ~ | 0x35 | 53 | 0xBD | |
| , / < | 0x36 | 54 | 0xBE | |
| . / > | 0x37 | 55 | 0xBF | |
| / / ? | 0x38 | 56 | 0xC0 | |
| Caps Lock | 0x39 | 57 | 0xC1 | `{CAPSLOCK}` |
| F1 | 0x3A | 58 | 0xC2 | `{F1}` |
| F2 | 0x3B | 59 | 0xC3 | `{F2}` |
| F3 | 0x3C | 60 | 0xC4 | `{F3}` |
| F4 | 0x3D | 61 | 0xC5 | `{F4}` |
| F5 | 0x3E | 62 | 0xC6 | `{F5}` |
| F6 | 0x3F | 63 | 0xC7 | `{F6}` |
| F7 | 0x40 | 64 | 0xC8 | `{F7}` |
| F8 | 0x41 | 65 | 0xC9 | `{F8}` |
| F9 | 0x42 | 66 | 0xCA | `{F9}` |
| F10 | 0x43 | 67 | 0xCB | `{F10}` |
| F11 | 0x44 | 68 | 0xCC | `{F11}` |
| F12 | 0x45 | 69 | 0xCD | `{F12}` |
| Print Screen | 0x46 | 70 | 0xCE | `{PRINTSCREEN}` |
| Scroll Lock | 0x47 | 71 | 0xCF | `{SCROLLLOCK}` |
| Pause | 0x48 | 72 | 0xD0 | `{PAUSE}` |
| Insert | 0x49 | 73 | 0xD1 | `{INSERT}` |
| Home | 0x4A | 74 | 0xD2 | `{HOME}` |
| Page Up | 0x4B | 75 | 0xD3 | `{PAGEUP}` |
| Delete | 0x4C | 76 | 0xD4 | `{DELETE}` |
| End | 0x4D | 77 | 0xD5 | `{END}` |
| Page Down | 0x4E | 78 | 0xD6 | `{PAGEDOWN}` |
| Right Arrow | 0x4F | 79 | 0xD7 | `{RIGHT}` |
| Left Arrow | 0x50 | 80 | 0xD8 | `{LEFT}` |
| Down Arrow | 0x51 | 81 | 0xD9 | `{DOWN}` |
| Up Arrow | 0x52 | 82 | 0xDA | `{UP}` |
| Num Lock | 0x53 | 83 | 0xDB | `{NUMLOCK}` |
| KP / | 0x54 | 84 | 0xDC | `{KPDIVIDE}` |
| KP * | 0x55 | 85 | 0xDD | `{KPMULTIPLY}` |
| KP - | 0x56 | 86 | 0xDE | `{KPMINUS}` |
| KP + | 0x57 | 87 | 0xDF | `{KPPLUS}` |
| KP Enter | 0x58 | 88 | 0xE0 | `{KPENTER}` |
| KP 1 | 0x59 | 89 | 0xE1 | `{KP1}` |
| KP 2 | 0x5A | 90 | 0xE2 | `{KP2}` |
| KP 3 | 0x5B | 91 | 0xE3 | `{KP3}` |
| KP 4 | 0x5C | 92 | 0xE4 | `{KP4}` |
| KP 5 | 0x5D | 93 | 0xE5 | `{KP5}` |
| KP 6 | 0x5E | 94 | 0xE6 | `{KP6}` |
| KP 7 | 0x5F | 95 | 0xE7 | `{KP7}` |
| KP 8 | 0x60 | 96 | 0xE8 | `{KP8}` |
| KP 9 | 0x61 | 97 | 0xE9 | `{KP9}` |
| KP 0 | 0x62 | 98 | 0xEA | `{KP0}` |
| KP . | 0x63 | 99 | 0xEB | `{KPDOT}` |
| Application | 0x65 | 101 | 0xED | `{APPLICATION}` |
| F13 | 0x68 | 104 | 0xF0 | `{F13}` |
| F14 | 0x69 | 105 | 0xF1 | `{F14}` |
| F15 | 0x6A | 106 | 0xF2 | `{F15}` |
| F16 | 0x6B | 107 | 0xF3 | `{F16}` |
| F17 | 0x6C | 108 | 0xF4 | `{F17}` |
| F18 | 0x6D | 109 | 0xF5 | `{F18}` |
| F19 | 0x6E | 110 | 0xF6 | `{F19}` |
| F20 | 0x6F | 111 | 0xF7 | `{F20}` |
| F21 | 0x70 | 112 | 0xF8 | `{F21}` |
| F22 | 0x71 | 113 | 0xF9 | `{F22}` |
| F23 | 0x72 | 114 | 0xFA | `{F23}` |
| F24 | 0x73 | 115 | 0xFB | `{F24}` |

**Modifier byte values** (for `{HID modifier ...}` and the `mod` field in keymaps):

| Modifier | Bit | Hex | Dec |
|----------|-----|-----|-----|
| Left Ctrl | 0 | 0x01 | 1 |
| Left Shift | 1 | 0x02 | 2 |
| Left Alt | 2 | 0x04 | 4 |
| Left GUI | 3 | 0x08 | 8 |
| Right Ctrl | 4 | 0x10 | 16 |
| Right Shift | 5 | 0x20 | 32 |
| Right Alt / AltGr | 6 | 0x40 | 64 |
| Right GUI | 7 | 0x80 | 128 |

Modifiers combine additively: Ctrl+Shift = `0x01 + 0x02 = 0x03`.

---

## Consumer Control Usage Codes

Used internally by media key tokens. For reference when using `{HID}` directly (these are Consumer page 0x0C, not keyboard page).

| Usage | Hex | Token |
|-------|-----|-------|
| Scan Next Track | 0xB5 | `{NEXTTRACK}` |
| Scan Previous Track | 0xB6 | `{PREVTRACK}` |
| Stop | 0xB7 | `{STOPTRACK}` |
| Play/Pause | 0xCD | `{PLAYPAUSE}` |
| Mute | 0xE2 | `{MUTE}` / `{MICMUTE}` |
| Volume Increment | 0xE9 | `{VOLUMEUP}` |
| Volume Decrement | 0xEA | `{VOLUMEDOWN}` |
| WWW Home | 0x0223 | `{WWWHOME}` |
| My Computer | 0x0194 | `{MYCOMPUTER}` |
| Calculator | 0x0192 | `{CALCULATOR}` |
| WWW Favourites | 0x022A | `{BOOKMARKS}` |
| WWW Search | 0x0221 | `{WWWSEARCH}` |
| WWW Stop | 0x0226 | `{WWWSTOP}` |
| WWW Back | 0x0224 | `{WWWBACK}` |
| Media Select | 0x0183 | `{MEDIASEL}` |
| Mail | 0x018A | `{EMAIL}` |
| Display Brightness++ | 0x006F | `{BRIGHTNESSUP}` |
| Display Brightness-- | 0x0070 | `{BRIGHTNESSDOWN}` |
| Keyboard Illum Toggle | 0x0077 | `{KBDILLUMTOGGLE}` |
| Keyboard Illum-- | 0x0078 | `{KBDILLUMDOWN}` |
| Keyboard Illum++ | 0x0079 | `{KBDILLUMUP}` |
| Eject | 0x00B8 | `{EJECTCD}` |
| AL Terminal Lock | 0x019E | `{SCREENLOCK}` |

---

## System Control Usage Codes

Usage Page 0x01 (Generic Desktop), System Control collection 0x80.

| Usage | Hex | Token |
|-------|-----|-------|
| System Power Down | 0x81 | `{SYSTEMPOWER}` |
| System Sleep | 0x82 | `{SYSTEMSLEEP}` |
| System Wake Up | 0x83 | `{SYSTEMWAKE}` |
