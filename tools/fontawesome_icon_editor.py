#!/usr/bin/env python3
"""
fontawesome_icon_editor.py — FA6 solid → 48×48 RGB565 C header tool.

Operations:
  fetch   <icon-name> [--size N] [--bg RRGGBB] [--fg RRGGBB]
      Download the FA npm package (if absent) and rasterise one icon.

  add     <icon-name> [--size N] [--bg RRGGBB] [--fg RRGGBB]
      Fetch + insert into the active header file.

  update  <icon-name> [--size N] [--bg RRGGBB] [--fg RRGGBB]
      Re-rasterise an existing entry in the active header file.

  convert <svg-file> <icon-name> [--size N] [--bg RRGGBB] [--fg RRGGBB]
      Rasterise a local SVG file and add/update it in the active header.

  list
      List icons currently in the active header file.

  save [path]
      Save the active header file (to path or the default target).

  save-as <path>
      Save as a new path (also makes it the active file for this session).

  load <path>
      Load a different header file as active for subsequent operations.

Usage examples:
  python3 fontawesome_icon_editor.py add circle-nodes
  python3 fontawesome_icon_editor.py fetch magnifying-glass --bg 000000
  python3 fontawesome_icon_editor.py convert myicon.svg my-icon
  python3 fontawesome_icon_editor.py list
  python3 fontawesome_icon_editor.py save
  python3 fontawesome_icon_editor.py save-as /tmp/my_icons.h
  python3 fontawesome_icon_editor.py load ../src/cardputer/fa_icons.h
"""

import argparse
import io
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

try:
    import cairosvg
    from PIL import Image
except ImportError:
    sys.exit("pip install cairosvg pillow")

# ── Paths ─────────────────────────────────────────────────────────────────────

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
DEFAULT_HEADER  = os.path.join(PROJECT_ROOT, "src", "cardputer", "fa_icons.h")
FA_NPM_DIR      = os.path.join(PROJECT_ROOT, "node_modules", "@fortawesome", "free-solid-svg-icons")
STATE_FILE      = os.path.join(SCRIPT_DIR, ".fa_editor_state.json")

# ── State (active file path) ──────────────────────────────────────────────────

def _load_state() -> dict:
    if os.path.exists(STATE_FILE):
        try:
            return json.load(open(STATE_FILE))
        except Exception:
            pass
    return {}


def _save_state(state: dict):
    json.dump(state, open(STATE_FILE, "w"), indent=2)


def get_active_path(state: dict) -> str:
    return state.get("active_file", DEFAULT_HEADER)


# ── FontAwesome npm helper ────────────────────────────────────────────────────

def _ensure_fa_package():
    """Install the FA6 npm package next to the project if not present."""
    if os.path.isdir(FA_NPM_DIR):
        return
    print("Installing @fortawesome/free-solid-svg-icons via npm…")
    subprocess.check_call(
        ["npm", "install", "@fortawesome/free-solid-svg-icons"],
        cwd=PROJECT_ROOT,
    )


def _camel(name: str) -> str:
    """circle-nodes → CircleNodes (for the FA export name)."""
    return "".join(w.capitalize() for w in name.split("-"))


def fa_icon_path(icon_name: str) -> str:
    """Return the SVG path string for a FA6 solid icon name."""
    _ensure_fa_package()
    export_name = "fa" + _camel(icon_name)
    script = (
        f"const icons = require('@fortawesome/free-solid-svg-icons');"
        f"const i = icons['{export_name}'];"
        f"if (!i) {{ console.error('Icon not found: {icon_name}'); process.exit(1); }}"
        f"console.log(JSON.stringify({{w:i.icon[0],h:i.icon[1],p:i.icon[4]}}))"
    )
    out = subprocess.check_output(["node", "-e", script], cwd=PROJECT_ROOT)
    data = json.loads(out)
    return data["w"], data["h"], data["p"]


# ── SVG → RGB565 array ────────────────────────────────────────────────────────

RENDER_SCALE = 4   # kept for reference but NOT used — see note below

# ICON RENDERING RULES:
# 1. Render at NATIVE 48×48 (no supersampling). cairosvg at native size produces
#    ~7-10% edge AA pixels — identical to the project's existing icons.
#    4× LANCZOS supersampling produces ~40% AA pixels → icons look blurry/soft.
# 2. Background MUST have ONE CLEARLY DOMINANT colour channel.
#    Anti-aliased edge pixels blend the icon into the bg. On the ST7789 display,
#    backgrounds with near-equal R/G/B (dark grey, dark purple, etc.) cause
#    visible colour fringing at the icon edge. Single-dominant-channel backgrounds
#    (e.g. pure green #008508, pure blue #00048b, amber #806000, teal #006080,
#    purple #5000A0) render cleanly. bg=#000000 is also bad — edges disappear.
#    Pick a dark, saturated background with one dominant channel per icon.

def svg_path_to_rgb565(path_d: str, vw: int, vh: int,
                        size: int = 48,
                        bg: str = "5000A0",
                        fg: str = "ffffff") -> list[int]:
    svg = (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {vw} {vh}">'
        f'<path d="{path_d}" fill="#{fg}"/>'
        f"</svg>"
    )
    png = cairosvg.svg2png(bytestring=svg.encode(),
                           output_width=size, output_height=size,
                           background_color=f"#{bg}")
    img = Image.open(io.BytesIO(png)).convert("RGB")
    pixels = []
    for y in range(size):
        for x in range(size):
            r, g, b = img.getpixel((x, y))
            pixels.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return pixels


def local_svg_to_rgb565(svg_file: str,
                         size: int = 48,
                         bg: str = "5000A0",
                         fg: str = "ffffff") -> list[int]:
    with open(svg_file, "rb") as f:
        raw = f.read()
    png = cairosvg.svg2png(bytestring=raw, output_width=size, output_height=size,
                           background_color=f"#{bg}")
    img = Image.open(io.BytesIO(png)).convert("RGB")
    pixels = []
    for y in range(size):
        for x in range(size):
            r, g, b = img.getpixel((x, y))
            pixels.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return pixels


# ── C array formatting ────────────────────────────────────────────────────────

def pixels_to_c_array(pixels: list[int], name: str, size: int = 48) -> str:
    hex_vals = [f"0x{p:04X}" for p in pixels]
    rows = []
    for i in range(0, len(hex_vals), 12):
        rows.append("    " + ", ".join(hex_vals[i : i + 12]) + ",")
    rows[-1] = rows[-1].rstrip(",")
    n = size * size
    lines = [
        f"// FA6 solid '{name}' icon — {size}x{size} RGB565, generated by fontawesome_icon_editor.py",
        f"static const uint16_t fa_{name.replace('-','_')}_{size}[{n}] PROGMEM = {{",
    ]
    lines += rows
    lines.append("};")
    return "\n".join(lines)


# ── Header file I/O ───────────────────────────────────────────────────────────

_ICON_RE = re.compile(
    r"// FA6 solid '([^']+)' icon.*?\nstatic const uint16_t (fa_[a-zA-Z0-9_]+)\[(\d+)\] PROGMEM = \{.*?\};",
    re.DOTALL,
)

_GUARD_OPEN  = "#pragma once\n"
_GUARD_BODY_START = "// Auto-generated FA6 solid icons"


def _read_header(path: str) -> str:
    if not os.path.exists(path):
        return f"{_GUARD_OPEN}\n{_GUARD_BODY_START} — 48x48 RGB565, PROGMEM.\n// Only icons actually used by the app launcher are compiled here.\n"
    return open(path).read()


def _write_header(path: str, content: str):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    open(path, "w").write(content)
    print(f"Written: {path}")


def list_icons(content: str) -> list[tuple[str, str]]:
    """Return [(icon_name, var_name), ...]."""
    return [(m.group(1), m.group(2)) for m in _ICON_RE.finditer(content)]


def _var_name(icon_name: str, size: int = 48) -> str:
    return f"fa_{icon_name.replace('-','_')}_{size}"


def insert_or_update(content: str, icon_name: str, array_text: str) -> str:
    """Replace existing entry or append."""
    # Try to find and replace
    pattern = re.compile(
        r"// FA6 solid '" + re.escape(icon_name) + r"' icon.*?\nstatic const uint16_t "
        + re.escape(_var_name(icon_name)) + r"\[\d+\] PROGMEM = \{.*?\};",
        re.DOTALL,
    )
    if pattern.search(content):
        new_content = pattern.sub(array_text, content)
        print(f"Updated icon '{icon_name}'.")
        return new_content
    # Append
    sep = "\n\n" if not content.endswith("\n\n") else "\n"
    print(f"Added icon '{icon_name}'.")
    return content.rstrip() + "\n\n" + array_text + "\n"


# ── Commands ──────────────────────────────────────────────────────────────────

def cmd_fetch(args, _state):
    vw, vh, path_d = fa_icon_path(args.icon_name)
    pixels = svg_path_to_rgb565(path_d, vw, vh, args.size, args.bg, args.fg)
    print(pixels_to_c_array(pixels, args.icon_name, args.size))


def cmd_add(args, state):
    vw, vh, path_d = fa_icon_path(args.icon_name)
    pixels  = svg_path_to_rgb565(path_d, vw, vh, args.size, args.bg, args.fg)
    array   = pixels_to_c_array(pixels, args.icon_name, args.size)
    path    = get_active_path(state)
    content = _read_header(path)
    content = insert_or_update(content, args.icon_name, array)
    _write_header(path, content)


def cmd_update(args, state):
    cmd_add(args, state)


def cmd_convert(args, state):
    pixels  = local_svg_to_rgb565(args.svg_file, args.size, args.bg, args.fg)
    array   = pixels_to_c_array(pixels, args.icon_name, args.size)
    path    = get_active_path(state)
    content = _read_header(path)
    content = insert_or_update(content, args.icon_name, array)
    _write_header(path, content)


def cmd_list(args, state):
    path    = get_active_path(state)
    content = _read_header(path)
    icons   = list_icons(content)
    if not icons:
        print(f"No icons found in {path}")
        return
    print(f"Icons in {path}:")
    for name, var in icons:
        print(f"  {name:30s}  → {var}")


def cmd_save(args, state):
    path = get_active_path(state)
    if hasattr(args, "path") and args.path:
        path = args.path
    content = _read_header(path)
    _write_header(path, content)


def cmd_save_as(args, state):
    src  = get_active_path(state)
    dest = args.path
    content = _read_header(src)
    _write_header(dest, content)
    state["active_file"] = dest
    _save_state(state)
    print(f"Active file is now: {dest}")


def cmd_load(args, state):
    if not os.path.exists(args.path):
        sys.exit(f"File not found: {args.path}")
    state["active_file"] = args.path
    _save_state(state)
    icons = list_icons(open(args.path).read())
    print(f"Loaded: {args.path}  ({len(icons)} icon(s))")


# ── CLI ───────────────────────────────────────────────────────────────────────

def _add_render_args(p):
    p.add_argument("--size", type=int, default=48, metavar="N",
                   help="Output size in pixels (default 48)")
    p.add_argument("--bg",   default="5000A0", metavar="RRGGBB",
                   help="Background colour hex (default 5000A0, must be non-black — see IMPORTANT note above)")
    p.add_argument("--fg",   default="ffffff",  metavar="RRGGBB",
                   help="Foreground/icon colour hex (default ffffff)")


def main():
    parser = argparse.ArgumentParser(
        description="FontAwesome → RGB565 C header editor",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_fetch = sub.add_parser("fetch", help="Rasterise an FA icon and print the C array")
    p_fetch.add_argument("icon_name")
    _add_render_args(p_fetch)

    p_add = sub.add_parser("add", help="Fetch + insert icon into header")
    p_add.add_argument("icon_name")
    _add_render_args(p_add)

    p_update = sub.add_parser("update", help="Re-rasterise existing icon in header")
    p_update.add_argument("icon_name")
    _add_render_args(p_update)

    p_conv = sub.add_parser("convert", help="Rasterise local SVG and add/update in header")
    p_conv.add_argument("svg_file")
    p_conv.add_argument("icon_name")
    _add_render_args(p_conv)

    sub.add_parser("list", help="List icons in the active header file")

    p_save = sub.add_parser("save", help="Save the active header file")
    p_save.add_argument("path", nargs="?", default=None)

    p_save_as = sub.add_parser("save-as", help="Save as a new path")
    p_save_as.add_argument("path")

    p_load = sub.add_parser("load", help="Load a different header file as active")
    p_load.add_argument("path")

    args   = parser.parse_args()
    state  = _load_state()

    dispatch = {
        "fetch":   cmd_fetch,
        "add":     cmd_add,
        "update":  cmd_update,
        "convert": cmd_convert,
        "list":    cmd_list,
        "save":    cmd_save,
        "save-as": cmd_save_as,
        "load":    cmd_load,
    }
    dispatch[args.cmd](args, state)


if __name__ == "__main__":
    main()
