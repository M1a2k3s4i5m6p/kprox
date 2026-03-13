# KProx Token String Reference

Token strings are KProx's domain-specific language for scripting keyboard and mouse actions. They are stored in registers or sent directly via the web interface or API.

A token string is plain text where special commands are enclosed in curly braces `{...}`. Text outside braces is typed character by character.

```
Hello {ENTER}
Username: admin{TAB}Password: secret{ENTER}
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
| `{BREAK var value}` | Exit loop when `var == value` |

```
{LOOP i 1 1 5}Line {i}{ENTER}{ENDLOOP}
{LOOP i 10 -1 1}{i}...{SLEEP 1000}{ENDLOOP}Launch!{ENTER}
{LOOP}jiggle{MOVEMOUSE {RAND -20 20} {RAND -20 20}}{SLEEP 500}{ENDLOOP}
```

---

## Variables

Variables store string or numeric values. They are referenced anywhere in a token string using `{varname}`. Loop counter variables are automatically scoped to their loop and erased when the loop exits. Variables set with `{SET}` persist for the remainder of the current token string execution.

### `{SET varname expr}`

Evaluates `expr` (which may contain other tokens, math, rand, etc.) and stores the result.

```
{SET total 0}
{SET msg Hello World}
{SET n {RAND 1 100}}
{SET i {MATH {i} + 1}}
{SET result {MATH {a} * {b} + {c}}}
```

The stored value is output anywhere you write `{varname}`:

```
{SET name Alice}Hello, {name}!{ENTER}
```
Types: `Hello, Alice!`

```
{SET x 7}{SET y 3}Result: {MATH {x} * {y}}{ENTER}
```
Types: `Result: 21`

---

## Conditionals

Conditionals branch execution based on a comparison. Bodies may contain any token string content including nested loops and other conditionals.

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

Both sides of the condition are fully evaluated (vars, MATH, RAND, etc.) before comparison. If both sides look numeric, numeric comparison is used; otherwise string comparison.

### Simple examples

```
{SET x 7}{IF {x} == 7}Lucky seven!{ENDIF}{ENTER}
```

```
{SET n {RAND 1 10}}{IF {n} > 5}High{ELSE}Low{ENDIF}{ENTER}
```

```
{LOOP i 1 1 10}{IF {i} == 5}FIVE{ELSE}{i}{ENDIF}{ENTER}{ENDLOOP}
```
Output:
```
1
2
3
4
FIVE
6
7
8
9
10
```

### Nested conditionals

```
{LOOP i 1 1 5}
{IF {i} == 1}one{ELSE}{IF {i} == 2}two{ELSE}{IF {i} == 3}three{ELSE}many{ENDIF}{ENDIF}{ENDIF}{ENTER}
{ENDLOOP}
```
Output: `one` `two` `three` `many` `many`

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

### Aligned multiplication table for a single factor
Uses IF to pad single vs double-digit results to 3 chars each:

```
 x |{LOOP j 0 1 9}  {j}{ENDLOOP}{ENTER}---+{LOOP j 0 1 9}---{ENDLOOP}{ENTER}{LOOP i 0 1 9} {i} |{LOOP j 0 1 9}{SET p {MATH {i} * {j}}}{IF {p} < 10}  {p}{ELSE} {p}{ENDIF}{ENDLOOP}{ENTER}{ENDLOOP}
```

Output:
```
 x |  0  1  2  3  4  5  6  7  8  9
---+-------------------------------
 0 |  0  0  0  0  0  0  0  0  0  0
 1 |  0  1  2  3  4  5  6  7  8  9
 2 |  0  2  4  6  8 10 12 14 16 18
 3 |  0  3  6  9 12 15 18 21 24 27
 4 |  0  4  8 12 16 20 24 28 32 36
 5 |  0  5 10 15 20 25 30 35 40 45
 6 |  0  6 12 18 24 30 36 42 48 54
 7 |  0  7 14 21 28 35 42 49 56 63
 8 |  0  8 16 24 32 40 48 56 64 72
 9 |  0  9 18 27 36 45 54 63 72 81
```

### Accumulate a running sum with SET
```
{SET total 0}{LOOP i 1 1 10}{SET total {MATH {total} + {i}}}{ENDLOOP}Sum 1-10 = {total}{ENTER}
```
Types: `Sum 1-10 = 55`

### FizzBuzz (1–20)
```
{LOOP i 1 1 20}{SET fb {i}}{IF {MATH {i} % 15} == 0}{SET fb FizzBuzz}{ELSE}{IF {MATH {i} % 3} == 0}{SET fb Fizz}{ELSE}{IF {MATH {i} % 5} == 0}{SET fb Buzz}{ENDIF}{ENDIF}{ENDIF}{fb}{ENTER}{ENDLOOP}
```
Output: `1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz 16 17 Fizz 19 Buzz`

### Countdown with conditional message
```
{LOOP i 10 -1 1}{IF {i} > 3}T-{i}...{ELSE}{IF {i} > 0}FINAL {i}!{ELSE}LIFTOFF!{ENDIF}{ENDIF}{ENTER}{SLEEP 1000}{ENDLOOP}
```

### Spiral mouse pattern
```
{LOOP i 0 2 720}{SETMOUSE {MATH 960 + {i}/5 * cos({MATH {i} * PI / 180})} {MATH 540 + {i}/5 * sin({MATH {i} * PI / 180})}}{SLEEP 3}{ENDLOOP}
```

### Random walk with boundary check
```
{SET x 500}{SET y 400}{LOOP}{SET x {MATH {x} + {RAND -20 20}}}{SET y {MATH {y} + {RAND -20 20}}}{IF {x} < 100}{SET x 100}{ENDIF}{IF {x} > 1820}{SET x 1820}{ENDIF}{IF {y} < 100}{SET y 100}{ENDIF}{IF {y} > 980}{SET y 980}{ENDIF}{SETMOUSE {x} {y}}{SLEEP 50}{ENDLOOP}
```
Moves the mouse in a bounded random walk within a safe screen region.

### Type a padded number table (right-aligned 3-digit numbers)
```
{LOOP i 1 1 20}{IF {i} < 10}  {i}{ELSE}{IF {i} < 100} {i}{ELSE}{i}{ENDIF}{ENDIF}{ENTER}{ENDLOOP}
```
Output:
```
  1
  2
...
  9
 10
 11
...
 20
```

### Conditional mouse click based on loop position
```
{LOOP i 1 1 5}{SETMOUSE {MATH 200 + {i} * 100} 400}{IF {i} == 3}{MOUSEDOUBLECLICK}{ELSE}{MOUSECLICK}{ENDIF}{SLEEP 300}{ENDLOOP}
```
Clicks 5 positions along a row, double-clicking the middle one.

### Accumulate max value
```
{SET max 0}{LOOP i 1 1 10}{SET n {RAND 1 100}}{n} {IF {n} > {max}}{SET max {n}}(new max!){ENDIF}{ENTER}{ENDLOOP}Max was: {max}{ENTER}
```

### Magic SysRq REISUB (Linux safe reboot)
```
{CHORD ALT+SYSRQ+R}{SLEEP 2000}{CHORD ALT+SYSRQ+E}{SLEEP 2000}{CHORD ALT+SYSRQ+I}{SLEEP 2000}{CHORD ALT+SYSRQ+S}{SLEEP 2000}{CHORD ALT+SYSRQ+U}{SLEEP 2000}{CHORD ALT+SYSRQ+B}
```

---

## Scoping Rules

- **Loop counter variables** (`{LOOP var ...}`) are inserted into scope for the loop body and **erased** when the loop exits.
- **SET variables** persist for the lifetime of the current token string execution. They are visible inside loop and IF bodies.
- Variables SET inside a loop body are visible in subsequent iterations and after the loop exits (by reference semantics).
- Nested loops each manage their own counter variable independently.

```
{SET total 0}
{LOOP i 1 1 5}
    {SET total {MATH {total} + {i}}}
{ENDLOOP}
Total: {total}{ENTER}
```
`total` is set before the loop, updated inside it, and readable after it. `i` is gone after the loop.

---

## Syntax Notes

- Tokens are **case-insensitive**: `{enter}`, `{ENTER}`, `{Enter}` all work.
- Nested tokens are supported: `{IF {MATH {i} % 2} == 0}even{ELSE}odd{ENDIF}`
- Text outside `{...}` is typed verbatim including spaces, punctuation, and newlines.
- `{IF}` and `{LOOP}` blocks can be nested to arbitrary depth.
- Conditions support any expression on either side: `{IF {MATH {i} % 2} == 0}`, `{IF {RAND 1 10} > 5}`.
- The DSL interpreter is single-threaded; long scripts block the web server during execution.
