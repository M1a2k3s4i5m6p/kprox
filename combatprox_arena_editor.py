#!/usr/bin/env python3
"""
CombatProx Arena Editor
Generates src/cardputer/combatprox_arena.h for the kprox firmware.
"""

import tkinter as tk
from tkinter import messagebox
import os
import sys

COLS      = 40
ROWS      = 20
CELL      = 20          # display cell size in pixels (editor canvas)
ARENA_W   = COLS * CELL
ARENA_H   = ROWS * CELL

CELL_PX   = 12         # firmware pixel size (480x240 virtual play area)
PLAYER_R  = 12         # display radius for player/enemy icons

COLOUR_BG      = "#1a1a2e"
COLOUR_GRID    = "#2a2a4e"
COLOUR_WALL    = "#4a7c59"
COLOUR_WALL_HL = "#6abf78"
COLOUR_PLAYER  = "#4fc3f7"
COLOUR_ENEMY   = "#ef5350"
COLOUR_EMPTY   = "#0d1117"
COLOUR_PANEL   = "#16213e"
COLOUR_BTN     = "#0f3460"
COLOUR_BTN_HL  = "#533483"
COLOUR_TEXT    = "#e0e0e0"

OUTPUT_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "src", "cardputer", "combatprox_arena.h"
)

# ---- tool modes ----
MODE_WALL   = "wall"
MODE_ERASE  = "erase"
MODE_PLAYER = "player"
MODE_ENEMY  = "enemy"


class ArenaEditor:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("CombatProx Arena Editor")
        root.configure(bg=COLOUR_PANEL)
        root.resizable(False, False)

        # State
        self.walls   = [[0]*COLS for _ in range(ROWS)]
        self.player  = [2, 9]
        self.enemies = [[37, 2], [37, 9], [37, 17]]

        self._load_defaults()

        self.mode    = tk.StringVar(value=MODE_WALL)
        self.status  = tk.StringVar(value="Ready")
        self._dragging = False

        self._build_ui()
        self._redraw()

    # ------------------------------------------------------------------ #
    #  Load defaults from the current header if it exists                  #
    # ------------------------------------------------------------------ #
    def _load_defaults(self):
        if not os.path.exists(OUTPUT_PATH):
            return
        try:
            with open(OUTPUT_PATH) as f:
                lines = f.readlines()
            row_idx = 0
            for line in lines:
                s = line.strip()
                if s.startswith("{") and "," in s and row_idx < ROWS:
                    vals = s.strip("{}").replace(" ", "").split(",")
                    for col, v in enumerate(vals[:COLS]):
                        self.walls[row_idx][col] = int(v)
                    row_idx += 1
                if "ARENA_PLAYER_X" in line:
                    px = float(line.split("=")[1].strip().rstrip(";f"))
                    self.player[0] = max(0, min(COLS-1, int(px / CELL_PX)))
                if "ARENA_PLAYER_Y" in line:
                    py = float(line.split("=")[1].strip().rstrip(";f"))
                    self.player[1] = max(0, min(ROWS-1, int(py / CELL_PX)))
                if "arenaEnemySpawns" not in line and ("{" in line) and "." in line:
                    pass
            # Parse enemy spawns
            spawn_lines = []
            in_spawns = False
            for line in lines:
                if "arenaEnemySpawns" in line:
                    in_spawns = True
                if in_spawns and "{" in line and "." in line and "arenaEnemySpawns" not in line:
                    nums = [float(x.strip().rstrip("f")) for x in
                            line.strip().strip("{},").split(",") if x.strip().rstrip("f")]
                    if len(nums) >= 2:
                        spawn_lines.append(nums)
                if in_spawns and "};" in line:
                    break
            for i, sp in enumerate(spawn_lines[:3]):
                self.enemies[i][0] = max(0, min(COLS-1, int(sp[0] / CELL_PX)))
                self.enemies[i][1] = max(0, min(ROWS-1, int(sp[1] / CELL_PX)))
        except Exception:
            pass  # silently keep defaults on parse error

    # ------------------------------------------------------------------ #
    #  UI construction                                                      #
    # ------------------------------------------------------------------ #
    def _build_ui(self):
        pad = 8

        # Canvas
        canvas_frame = tk.Frame(self.root, bg=COLOUR_PANEL, padx=pad, pady=pad)
        canvas_frame.pack(side=tk.LEFT, fill=tk.BOTH)

        self.canvas = tk.Canvas(
            canvas_frame,
            width=ARENA_W, height=ARENA_H,
            bg=COLOUR_EMPTY, highlightthickness=2,
            highlightbackground="#3a3a6a"
        )
        self.canvas.pack()
        self.canvas.bind("<Button-1>",        self._on_press)
        self.canvas.bind("<B1-Motion>",       self._on_drag)
        self.canvas.bind("<ButtonRelease-1>", self._on_release)
        self.canvas.bind("<Button-3>",        self._on_right)

        # Sidebar
        side = tk.Frame(self.root, bg=COLOUR_PANEL, padx=pad, pady=pad, width=160)
        side.pack(side=tk.RIGHT, fill=tk.Y)
        side.pack_propagate(False)

        tk.Label(side, text="TOOL", bg=COLOUR_PANEL, fg=COLOUR_TEXT,
                 font=("Helvetica", 10, "bold")).pack(pady=(8,2))

        tools = [
            ("Wall",         MODE_WALL),
            ("Erase",        MODE_ERASE),
            ("Player spawn", MODE_PLAYER),
            ("Enemy spawn",  MODE_ENEMY),
        ]
        for label, mode in tools:
            rb = tk.Radiobutton(
                side, text=label, variable=self.mode, value=mode,
                bg=COLOUR_PANEL, fg=COLOUR_TEXT, selectcolor=COLOUR_BTN,
                activebackground=COLOUR_PANEL, activeforeground=COLOUR_TEXT,
                font=("Helvetica", 9), anchor="w"
            )
            rb.pack(fill=tk.X, padx=4, pady=1)

        tk.Frame(side, bg="#3a3a6a", height=1).pack(fill=tk.X, pady=10)

        tk.Label(side, text="ENEMY SLOTS", bg=COLOUR_PANEL, fg=COLOUR_TEXT,
                 font=("Helvetica", 10, "bold")).pack(pady=(0,2))

        self.enemy_var = tk.IntVar(value=0)
        for i in range(3):
            rb = tk.Radiobutton(
                side, text=f"Enemy {i+1}", variable=self.enemy_var, value=i,
                bg=COLOUR_PANEL, fg=COLOUR_TEXT, selectcolor=COLOUR_BTN,
                activebackground=COLOUR_PANEL, activeforeground=COLOUR_TEXT,
                font=("Helvetica", 9), anchor="w"
            )
            rb.pack(fill=tk.X, padx=4, pady=1)

        tk.Frame(side, bg="#3a3a6a", height=1).pack(fill=tk.X, pady=10)

        tk.Button(
            side, text="Clear walls", command=self._clear_walls,
            bg=COLOUR_BTN, fg=COLOUR_TEXT, relief=tk.FLAT,
            activebackground=COLOUR_BTN_HL, activeforeground=COLOUR_TEXT,
            font=("Helvetica", 9)
        ).pack(fill=tk.X, padx=4, pady=2)

        tk.Button(
            side, text="Reset spawns", command=self._reset_spawns,
            bg=COLOUR_BTN, fg=COLOUR_TEXT, relief=tk.FLAT,
            activebackground=COLOUR_BTN_HL, activeforeground=COLOUR_TEXT,
            font=("Helvetica", 9)
        ).pack(fill=tk.X, padx=4, pady=2)

        tk.Frame(side, bg="#3a3a6a", height=1).pack(fill=tk.X, pady=10)

        tk.Button(
            side, text="💾  Save Header", command=self._save,
            bg="#1b5e20", fg="#a5d6a7", relief=tk.FLAT,
            activebackground="#2e7d32", activeforeground="#c8e6c9",
            font=("Helvetica", 10, "bold"), pady=6
        ).pack(fill=tk.X, padx=4, pady=2)

        tk.Label(side, textvariable=self.status, bg=COLOUR_PANEL,
                 fg="#aaaaaa", font=("Helvetica", 8),
                 wraplength=140, justify=tk.LEFT).pack(pady=(6,0), padx=4)

        # Legend
        tk.Frame(side, bg="#3a3a6a", height=1).pack(fill=tk.X, pady=8)
        legend = [
            (COLOUR_WALL,   "Wall"),
            (COLOUR_PLAYER, "Player"),
            (COLOUR_ENEMY,  "Enemy"),
        ]
        for col, lbl in legend:
            f = tk.Frame(side, bg=COLOUR_PANEL)
            f.pack(fill=tk.X, padx=4, pady=1)
            tk.Canvas(f, width=12, height=12, bg=col,
                      highlightthickness=0).pack(side=tk.LEFT, padx=(0,5))
            tk.Label(f, text=lbl, bg=COLOUR_PANEL, fg=COLOUR_TEXT,
                     font=("Helvetica", 8)).pack(side=tk.LEFT)

        # Hint
        tk.Label(
            canvas_frame,
            text="Left-drag: place  |  Right-click: erase cell  |  Select tool above",
            bg=COLOUR_PANEL, fg="#666688", font=("Helvetica", 8)
        ).pack(pady=(4,0))

    # ------------------------------------------------------------------ #
    #  Canvas interaction                                                   #
    # ------------------------------------------------------------------ #
    def _cell(self, event):
        col = event.x // CELL
        row = event.y // CELL
        if 0 <= col < COLS and 0 <= row < ROWS:
            return col, row
        return None, None

    def _on_press(self, event):
        self._dragging = True
        self._apply(event)

    def _on_drag(self, event):
        if self._dragging:
            self._apply(event)

    def _on_release(self, event):
        self._dragging = False

    def _on_right(self, event):
        col, row = self._cell(event)
        if col is None:
            return
        self.walls[row][col] = 0
        self._redraw()

    def _apply(self, event):
        col, row = self._cell(event)
        if col is None:
            return
        mode = self.mode.get()
        if mode == MODE_WALL:
            self.walls[row][col] = 1
        elif mode == MODE_ERASE:
            self.walls[row][col] = 0
        elif mode == MODE_PLAYER:
            self.player = [col, row]
        elif mode == MODE_ENEMY:
            idx = self.enemy_var.get()
            self.enemies[idx] = [col, row]
        self._redraw()

    # ------------------------------------------------------------------ #
    #  Buttons                                                              #
    # ------------------------------------------------------------------ #
    def _clear_walls(self):
        self.walls = [[0]*COLS for _ in range(ROWS)]
        self._redraw()

    def _reset_spawns(self):
        self.player  = [2, 9]
        self.enemies = [[37, 2], [37, 9], [37, 17]]
        self._redraw()

    # ------------------------------------------------------------------ #
    #  Rendering                                                            #
    # ------------------------------------------------------------------ #
    def _redraw(self):
        c = self.canvas
        c.delete("all")

        for row in range(ROWS):
            for col in range(COLS):
                x1 = col * CELL
                y1 = row * CELL
                x2 = x1 + CELL
                y2 = y1 + CELL
                fill = COLOUR_WALL if self.walls[row][col] else COLOUR_EMPTY
                c.create_rectangle(x1, y1, x2, y2, fill=fill, outline=COLOUR_GRID, width=1)

        # Grid lines
        for col in range(COLS+1):
            x = col * CELL
            c.create_line(x, 0, x, ARENA_H, fill=COLOUR_GRID, width=1)
        for row in range(ROWS+1):
            y = row * CELL
            c.create_line(0, y, ARENA_W, y, fill=COLOUR_GRID, width=1)

        # Cell coordinate labels (small)
        for row in range(ROWS):
            for col in range(COLS):
                if not self.walls[row][col]:
                    cx = col * CELL + 4
                    cy = row * CELL + 3
                    c.create_text(cx, cy, text=f"{col},{row}", anchor="nw",
                                  fill="#333355", font=("Courier", 7))

        # Enemy spawns
        enemy_colours = [COLOUR_ENEMY, "#ff8a65", "#ffb74d"]
        for i, (ec, er) in enumerate(self.enemies):
            cx = ec * CELL + CELL // 2
            cy = er * CELL + CELL // 2
            col = enemy_colours[i % len(enemy_colours)]
            c.create_oval(cx-PLAYER_R, cy-PLAYER_R, cx+PLAYER_R, cy+PLAYER_R,
                          fill=col, outline="#ffffff", width=1)
            c.create_text(cx, cy, text=f"E{i+1}", fill="#ffffff",
                          font=("Helvetica", 8, "bold"))

        # Player spawn
        px = self.player[0] * CELL + CELL // 2
        py = self.player[1] * CELL + CELL // 2
        c.create_oval(px-PLAYER_R, py-PLAYER_R, px+PLAYER_R, py+PLAYER_R,
                      fill=COLOUR_PLAYER, outline="#ffffff", width=1)
        c.create_text(px, py, text="P", fill="#ffffff",
                      font=("Helvetica", 9, "bold"))

    # ------------------------------------------------------------------ #
    #  Header generation                                                    #
    # ------------------------------------------------------------------ #
    def _save(self):
        # Validate: spawns must not be on walls
        pc, pr = self.player
        if self.walls[pr][pc]:
            messagebox.showerror("Invalid", "Player spawn is inside a wall.")
            return
        for i, (ec, er) in enumerate(self.enemies):
            if self.walls[er][ec]:
                messagebox.showerror("Invalid", f"Enemy {i+1} spawn is inside a wall.")
                return

        # Convert cell positions to firmware pixel coordinates (centre of cell)
        px = pc * CELL_PX + CELL_PX // 2
        py = pr * CELL_PX + CELL_PX // 2

        enemy_lines = ""
        for i, (ec, er) in enumerate(self.enemies):
            ex = ec * CELL_PX + CELL_PX // 2
            ey = er * CELL_PX + CELL_PX // 2
            comma = "," if i < 2 else ""
            enemy_lines += f"    {{ {ex:>5.1f}f, {ey:>5.1f}f }}{comma}\n"

        wall_rows = ""
        for row in range(ROWS):
            cells = ",".join(str(self.walls[row][col]) for col in range(COLS))
            comma = "," if row < ROWS-1 else ""
            wall_rows += f"    {{ {cells} }}{comma}\n"

        header = (
            "// Auto-generated by combatprox_arena_editor.py — do not edit by hand.\n"
            f"// Arena: {COLS} cols x {ROWS} rows, CELL={CELL_PX}px  ({COLS*CELL_PX}x{ROWS*CELL_PX} play area)\n"
            "\n"
            "#pragma once\n"
            "\n"
            f"static constexpr int ARENA_COLS        = {COLS};\n"
            f"static constexpr int ARENA_ROWS        = {ROWS};\n"
            f"static constexpr int ARENA_ENEMY_COUNT = {len(self.enemies)};\n"
            "\n"
            f"static constexpr float ARENA_PLAYER_X = {px}.f;\n"
            f"static constexpr float ARENA_PLAYER_Y = {py}.f;\n"
            "\n"
            f"static const float arenaEnemySpawns[ARENA_ENEMY_COUNT][2] = {{\n"
            f"{enemy_lines}"
            "};\n"
            "\n"
            f"const uint8_t AppCombatProx::_wallMap[ARENA_ROWS][ARENA_COLS] = {{\n"
            f"{wall_rows}"
            "};\n"
        )

        try:
            os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
            with open(OUTPUT_PATH, "w") as f:
                f.write(header)
            self.status.set(f"Saved to\n{os.path.relpath(OUTPUT_PATH)}")
        except Exception as e:
            messagebox.showerror("Save failed", str(e))
            self.status.set(f"Error: {e}")


def main():
    root = tk.Tk()
    app = ArenaEditor(root)
    root.mainloop()


if __name__ == "__main__":
    main()
