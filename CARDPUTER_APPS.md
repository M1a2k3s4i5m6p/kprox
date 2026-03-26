# KProx Cardputer Apps

Navigate the launcher with arrow keys. ENTER opens an app. ESC or backtick returns to the launcher.
BtnG0 plays the active register from any app that does not override it. TAB shows the app's built-in
help page. Apps can be reordered and hidden via **Settings → App Layout**.

| App | Description |
|-----|-------------|
| **KProx** | Register playback and device status. Shows active register, IP, SSID, and credential store lock state. |
| **FuzzyProx** | Fuzzy-search across all registers by name or content. Type to filter; ENTER plays the match. |
| **RegEdit** | Full-screen register editor with token syntax support. |
| **CredStore** | Encrypted credential store (KeePass KDBX 3.1). Unlock before using `{CREDSTORE}` or `{TOTP}` tokens. |
| **Gadgets** | Browse and install community token strings from GitHub over WiFi. |
| **SinkProx** | View and execute the unauthenticated write-only sink buffer (`/api/sink`). |
| **Keyboard** | Direct keyboard HID forwarding to the paired host. All keys including `;.,/\`` are forwarded. |
| **Clock** | Current time, date, and timezone selector. Left/right arrows cycle timezones. Requires NTP. |
| **QRProx** | QR code for the web interface URL. BtnG0 types the URL to the host. |
| **SchedProx** | Time-triggered register scheduling. Tasks fire at a set `HH:MM:SS` wall-clock time. Requires NTP. |
| **TOTProx** | TOTP 2FA authenticator. BtnG0 types the live code. Requires the credential store to be unlocked. |
| **Files** | SD card file browser. ENTER opens, `D` dumps to HID, DEL deletes. |
| **KPScript** | Run KProx Script (`.kps`) files from the SD card. `R` rescans, ENTER runs. |
| **BootProx** | Fire a register on boot with an optional auto-disable limit after N boots. |
| **TimerProx** | Countdown timer with deferred register playback. No WiFi or NTP required. |
| **psCombatProx** | `ps`-command combat mini-game over HID. Requires a root terminal on the target. |
| **MediaCtrl** | Media transport controls: Prev, Stop, Play/Pause, Next, Mute, Vol −, Vol +. |
| **Settings** | All device configuration across 15 pages: WiFi, BT, USB, timing, layout, security, OTA, and more. |

## Navigation Quick Reference

| Key | Action |
|-----|--------|
| Arrow keys / `;` `.` `,` `/` | Navigate launcher or in-app selection |
| ENTER | Open selected app / confirm |
| ESC or backtick | Return to launcher |
| BtnG0 (single press) | Play active register (or app-specific action) |
| BtnG0 (double press) | Cycle to next register |
| TAB | Show app help overlay |
| FN + ENTER | Next page (multi-page apps) |
