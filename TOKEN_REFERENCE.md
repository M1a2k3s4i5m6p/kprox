# KProx Token String Reference

Token strings are KProx's domain-specific language for scripting keyboard and mouse actions. They are stored in registers or sent directly via the web interface or API.

A token string is plain text where special commands are enclosed in curly braces `{...}`. Text outside braces is typed character by character.

```
Hello {ENTER}
Username: admin{TAB}Password: {CREDSTORE admin_pass}{ENTER}
```

---

## Text and Character Output

| Token | Description |
|-------|-------------|
| `{ENTER}` | Press Enter / Return |
| `{TAB}` | Press Tab |
| `{SPACE}` | Press Space bar |
| `{ESC}` | Press Escape |
| `{BACKSPACE}` | Press Backspace |
| `{DELETE}` | Press Delete |
| `{INSERT}` | Press Insert |

**Escape sequences** inside token strings:

| Sequence | Meaning |
|----------|---------|
| `\n` | Newline |
| `\t` | Tab character |
| `\\` | Literal backslash |
| `\{` | Literal `{` |
| `\}` | Literal `}` |

---

## Navigation Keys

`{UP}` `{DOWN}` `{LEFT}` `{RIGHT}` `{HOME}` `{END}` `{PAGEUP}` `{PAGEDOWN}`

---

## Function and System Keys

`{F1}` – `{F12}` · `{PRINTSCREEN}` / `{SYSRQ}` · `{SCROLLLOCK}` · `{PAUSE}` · `{CAPSLOCK}` · `{NUMLOCK}` · `{APPLICATION}`

---

## Keypad

`{KP0}`–`{KP9}` · `{KPENTER}` · `{KPPLUS}` · `{KPMINUS}` · `{KPMULTIPLY}` · `{KPDIVIDE}` · `{KPDOT}`

---

## Key Chords

`{CHORD modifier+key}` — press modifiers and a key simultaneously.

**Modifiers:** `CTRL` `ALT` `SHIFT` `GUI` (Windows/Cmd)

```
{CHORD CTRL+C}             Copy
{CHORD CTRL+ALT+DELETE}    Ctrl+Alt+Del
{CHORD GUI+R}              Run dialog (Windows)
{CHORD CTRL+SHIFT+ESC}     Task Manager
{CHORD ALT+SYSRQ+B}        Magic SysRq B (Linux)
```

---

## Raw HID Keycodes

`{HID keycode}` — raw HID code (hex).  
`{HID modifier keycode}` — modifier byte + key.  
`{HID mod key1 key2 ...}` — up to 6 simultaneous keys.

Media keys: `{HID 0xE9}` Vol Up · `{HID 0xEA}` Vol Down · `{HID 0xB5}` Next Track

---

## Mouse Control

| Token | Description |
|-------|-------------|
| `{SETMOUSE x y}` | Absolute mouse position |
| `{MOVEMOUSE dx dy}` | Relative movement |
| `{MOUSECLICK}` | Left click |
| `{MOUSECLICK button}` | Click: 1=left 2=right 3=middle |
| `{MOUSEDOUBLECLICK}` | Double left click |
| `{MOUSEPRESS button}` | Press and hold |
| `{MOUSERELEASE button}` | Release held button |

---

## Timing

| Token | Description |
|-------|-------------|
| `{SLEEP ms}` | Pause for milliseconds |
| `{SLEEP {expr}}` | Dynamic delay |
| `{SCHEDULE HH:MM}` | Wait until wall-clock time (NTP) |
| `{SCHEDULE HH:MM:SS}` | Wait until exact time |

---

## Loops

| Token | Description |
|-------|-------------|
| `{LOOP}` | Infinite loop (until halted) |
| `{LOOP ms}` | Timed loop |
| `{LOOP var start inc end}` | Counter loop |
| `{ENDLOOP}` | End loop block |
| `{FOR var start inc end}` | Counted loop with named variable |
| `{ENDFOR}` | End FOR block |
| `{WHILE left op right}` | Loop while condition is true |
| `{ENDWHILE}` | End WHILE block |
| `{BREAK}` | Unconditionally exit the current loop |
| `{BREAK var value}` | Exit loop when `var == value` |

```
{LOOP i 1 1 5}Line {i}{ENTER}{ENDLOOP}
{LOOP i 10 -1 1}{i}...{SLEEP 1000}{ENDLOOP}Launch!{ENTER}
{LOOP}jiggle{MOVEMOUSE {RAND -20 20} {RAND -20 20}}{SLEEP 500}{ENDLOOP}
```

---

## Variables

Variables store string or numeric values and are referenced anywhere using `{varname}`. Loop counter variables are scoped to their loop and erased on exit. `{SET}` variables persist for the remainder of the current execution.

### `{SET varname expr}`

```
{SET total 0}
{SET msg Hello World}
{SET n {RAND 1 100}}
{SET i {MATH {i} + 1}}
{SET result {MATH {a} * {b} + {c}}}
```

```
{SET name Alice}Hello, {name}!{ENTER}
```
Types: `Hello, Alice!`

---

## Conditionals

### Syntax

```
{IF left op right}
    ...true body...
{ENDIF}
```

```
{IF left op right}
    ...true body...
{ELSE}
    ...false body...
{ENDIF}
```

### Operators

| Operator | Meaning |
|----------|---------|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

Both sides are fully evaluated before comparison. Numeric comparison is used when both sides parse as numbers; otherwise string comparison.

---

## Math

`{MATH expression}` — evaluates an arithmetic expression and outputs the result.

**Operators:** `+` `-` `*` `/` `%` (modulo)  
**Functions:** `sin(x)` `cos(x)` `tan(x)` `sqrt(x)` `abs(x)` `floor(x)` `ceil(x)` `round(x)`  
**Constants:** `PI` `E`

```
{MATH 5 + 3}            → 8
{MATH 10 % 3}           → 1
{MATH PI * 2}           → 6.28
{MATH sqrt(144)}        → 12
{MATH {i} * {j}}        → product of vars i and j
{MATH floor({x} / 10)}  → integer division
```

---

## Random Numbers

`{RAND min max}` — random integer in [min, max] inclusive.

```
{RAND 1 6}           → dice roll
{RAND 65 90}         → random uppercase letter code
{MOVEMOUSE {RAND -50 50} {RAND -50 50}}
{SLEEP {RAND 500 3000}}
```

---

## ASCII Output

`{ASCII value}` — type the character with that ASCII code (decimal or `0x` hex).

```
{ASCII 65}               → A
{ASCII {RAND 65 90}}     → random uppercase letter
{ASCII 0x41}             → A (hex)
```

---

## Keymap

`{KEYMAP id}` — switch to a named keyboard layout for the remainder of the token string execution. The layout must exist on the device (built-in `en` is always available; custom layouts are uploaded via the web interface or API).

```
{KEYMAP de}Hallo Welt{ENTER}
{KEYMAP en}
```

---

## Credential Store

`{CREDSTORE label}` — looks up a named credential in the on-device encrypted store and substitutes its decrypted plaintext value inline. If the store is locked the token resolves to an empty string, producing no output.

```
admin{TAB}{CREDSTORE admin_password}{ENTER}
{CREDSTORE wifi_pass}
```

Credentials are managed via the web interface **Credential Store** tab or the `/api/credstore` API endpoint. The store must be unlocked before playback starts; the token does not trigger an unlock prompt.

> **Security note:** Once substituted, the credential value travels through the normal HID output path as plaintext keystrokes. The credential is never written to flash in plaintext; it is decrypted in RAM only at the moment of substitution. Locking the store clears the key from RAM immediately.

---

## System Control

| Token | Description |
|-------|-------------|
| `{HALT}` | Stop all execution |
| `{RESUME}` | Resume halted execution |
| `{BLUETOOTH_ENABLE}` | Enable BLE HID |
| `{BLUETOOTH_DISABLE}` | Disable BLE HID |
| `{USB_ENABLE}` | Enable USB HID |
| `{USB_DISABLE}` | Disable USB HID |
| `{WIFI ssid password}` | Connect to WiFi network |

---

## Complex Examples

### Credential-based login
```
{CREDSTORE corp_username}{TAB}{CREDSTORE corp_password}{ENTER}
```
Requires the store to be unlocked before playback.

### FizzBuzz (1–20)
```
{LOOP i 1 1 20}{SET fb {i}}{IF {MATH {i} % 15} == 0}{SET fb FizzBuzz}{ELSE}{IF {MATH {i} % 3} == 0}{SET fb Fizz}{ELSE}{IF {MATH {i} % 5} == 0}{SET fb Buzz}{ENDIF}{ENDIF}{ENDIF}{fb}{ENTER}{ENDLOOP}
```

### Accumulate a running sum
```
{SET total 0}{LOOP i 1 1 100}{SET total {MATH {total} + {i}}}{ENDLOOP}Sum 1-100 = {total}{ENTER}
```
Types: `Sum 1-100 = 5050`

### Mouse jiggler (infinite, random)
```
{LOOP}{MOVEMOUSE {RAND -50 50} {RAND -50 50}}{SLEEP {RAND 1000 3000}}{ENDLOOP}
```

### Unlock Android
```
{MOUSEMOVE 10 0}{ENTER}{SLEEP 100}mysecurepassword{SLEEP 300}{ENTER}
```

### Unlock Windows
```
{LEFT}{SLEEP 1000}mysecurepassword{ENTER}
```

### Linux Magic SysRq REISUB (safe emergency reboot)
```
{CHORD ALT+SYSRQ+R}{SLEEP 2000}{CHORD ALT+SYSRQ+E}{SLEEP 2000}{CHORD ALT+SYSRQ+I}{SLEEP 2000}{CHORD ALT+SYSRQ+S}{SLEEP 2000}{CHORD ALT+SYSRQ+U}{SLEEP 2000}{CHORD ALT+SYSRQ+B}
```

### Aligned multiplication table
```
 x |{LOOP j 0 1 9}  {j}{ENDLOOP}{ENTER}---+{LOOP j 0 1 9}---{ENDLOOP}{ENTER}{LOOP i 0 1 9} {i} |{LOOP j 0 1 9}{SET p {MATH {i} * {j}}}{IF {p} < 10}  {p}{ELSE} {p}{ENDIF}{ENDLOOP}{ENTER}{ENDLOOP}
```

### Countdown with conditional message
```
{LOOP i 10 -1 1}{IF {i} > 3}T-{i}...{ELSE}{IF {i} > 0}FINAL {i}!{ELSE}LIFTOFF!{ENDIF}{ENDIF}{ENTER}{SLEEP 1000}{ENDLOOP}
```

### Spiral mouse pattern
```
{LOOP i 0 2 720}{SETMOUSE {MATH 960 + {i}/5 * cos({MATH {i} * PI / 180})} {MATH 540 + {i}/5 * sin({MATH {i} * PI / 180})}}{SLEEP 3}{ENDLOOP}
```

---

## Scoping Rules

- **Loop counter variables** (`{LOOP var ...}`) are inserted into scope for the loop body and **erased** when the loop exits.
- **SET variables** persist for the lifetime of the current token string execution and are visible inside loop and IF bodies.
- Variables SET inside a loop body are visible in subsequent iterations and after the loop exits.
- Nested loops each manage their own counter variable independently.

---

## Syntax Notes

- Tokens are **case-insensitive**: `{enter}`, `{ENTER}`, `{Enter}` all work.
- Nested tokens are supported: `{IF {MATH {i} % 2} == 0}even{ELSE}odd{ENDIF}`
- Text outside `{...}` is typed verbatim including spaces, punctuation, and newlines.
- `{IF}` and `{LOOP}` blocks can be nested to arbitrary depth.
- The DSL interpreter is single-threaded; long scripts block the web server during execution.
- `{CREDSTORE label}` resolves silently to empty string when the store is locked — it never errors or prompts.
