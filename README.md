# Rush LED Wall — Setup Guide

A scrolling LED banner ("RUSH THETA TAU") with a chasing Pac-Man animation, running on an ESP32-S3 driving four chained HUB75 panels.

**Hardware:** Waveshare ESP32-S3-RGB-Matrix driver board + 4× 64×64 (2mm/P2, GOB) HUB75 panels, wired in a 256×64 horizontal strip.

**Toolchain:** [PlatformIO](https://platformio.org/) (VS Code extension), Arduino framework, C++.

---

## 1. Hardware Layout

The panels are wired **4-in-a-row** (256 × 64 canvas), not a 2×2 square — this is the simplest wiring for scrolling banner text and is what the code assumes.

| Layout | Canvas size | Wiring | Code complexity |
|---|---|---|---|
| **4 in a row** (used here) | 256 × 64 | Straight chain, panel 1→2→3→4 | Simple — matches the code as-is |
| 2×2 square | 128 × 128 | Serpentine chain (zig-zag) | Needs a "virtual matrix" mapping layer — not implemented here |

---

## 2. Wiring

### 2.1 Data (HUB75 ribbon cables)

Each panel has an **IN** header and an **OUT** header.

1. ESP32-S3-RGB-Matrix board's HUB75 output → Panel 1 **IN**
2. Panel 1 **OUT** → Panel 2 **IN**
3. Panel 2 **OUT** → Panel 3 **IN**
4. Panel 3 **OUT** → Panel 4 **IN**

Arrange the 4 panels physically left-to-right in that same order (1, 2, 3, 4) so the chain order matches what's displayed. Keep ribbon runs short — under ~1.5–2m is safest for signal integrity at full chain length. If you need to go longer, use a HUB75 signal booster/re-clocker between panels.

The Waveshare driver board's pins are wired explicitly in code to match this board (see `PANEL_PINS` in `src/main.cpp`) — you don't need to change any wiring to match the code, but if you swap hardware, that's the place to update.

### 2.2 Power

This is the part people underbuild. Per Waveshare's spec for these 64×64 panels: **each panel needs its own 5V/4A (peak, full-white) supply.** For 4 panels, that's up to **16A at 5V (~80W)** in a worst-case, all-white, full-brightness scenario. Scrolling text on a black background draws far less in practice — but size your PSU for the worst case so it never sags or overheats.

- **Power the panels directly from the PSU, not through the ESP32 board.** The driver board's onboard 5V pass-through is only rated for roughly one panel's worth of current.
- Use thick wire (18AWG or better) and, ideally, a **power injection point at both ends of the panel chain** (inject 5V/GND into panel 1 *and* panel 4) — voltage droop across 4 daisy-chained panels is a common cause of dim/discolored far-end panels.
- Tie PSU ground, panel grounds, and ESP32 board ground all together (common ground).
- Check your PSU's continuous rating against the ~16A worst case. If it's undersized, lower `BRIGHTNESS` in the code (see below) — a text banner rarely needs full brightness anyway.

---

## 3. Software Setup (PlatformIO)

1. Install [VS Code](https://code.visualstudio.com/) if you don't already have it.
2. Install the **PlatformIO IDE** extension from the VS Code Extensions marketplace.
3. Clone this repo and open the folder in VS Code:
   ```bash
   git clone https://github.com/Isaac-Loc/OT-LED-Wall.git
   cd OT-LED-Wall
   code .
   ```
4. PlatformIO will automatically detect `platformio.ini` and prompt to install the project's dependencies (`ESP32 HUB75 LED MATRIX PANEL DMA Display` and `Adafruit GFX Library`) — let it install them. If it doesn't prompt automatically, click the PlatformIO icon in the sidebar → **Build** to trigger dependency resolution.
5. Plug the board in via USB-C.
6. In the PlatformIO toolbar at the bottom of VS Code, click **Upload** (→ icon). This builds and flashes `src/main.cpp` to the board.
7. Click **Monitor** (plug icon) to view serial output at 115200 baud — useful for confirming the display initialized and for any warnings (e.g. text size too tall for the panel).

No Arduino IDE installation is needed — PlatformIO handles the toolchain, board definitions, and libraries entirely through `platformio.ini`.

---

## 4. Configuration

All the settings you'd actually want to tweak are in one place at the top of `src/main.cpp`, under `USER SETTINGS`:

| Setting | What it does |
|---|---|
| `MESSAGE` | The scrolling text |
| `BRIGHTNESS` | 0–255. Defaults to 160 (~65%) to stay under typical PSU limits |
| `SCROLL_DELAY_MS` | Lower = faster scroll |
| `TEXT_SIZE` | Font scale (1 = 6×8px chars, 2 = 12×16px, etc.) |
| `Y_NUDGE` | Fine-tune vertical centering of the text |
| `SHOW_PACMAN` | Toggle the chasing Pac-Man animation on/off |
| `PACMAN_RADIUS` / `PACMAN_GAP` / `PACMAN_MOUTH_SPEED` / `PACMAN_Y_NUDGE` | Pac-Man sprite sizing, spacing, animation speed, and vertical position |

If you change panel count or panel size, update `PANEL_RES_X`, `PANEL_RES_Y`, and `PANEL_CHAIN` in the `PANEL / HARDWARE CONFIG` section further down in the same file.

---

## 5. First Boot Checklist

1. **Before connecting panel power**, upload the code and connect only USB power to the ESP32 board. The code defaults to a moderate brightness (160/255), so this is safe to check on USB power alone at low draw.
2. Connect panel 5V power. You should see the scrolling text (and Pac-Man, if enabled).
3. If the image looks **scrambled, shifted, or has wrong colors**: some GOB panels use FM6126A-family driver chips that need an extra init sequence. In `src/main.cpp`, find the `mxconfig.driver` line and try setting it to `HUB75_I2S_CFG::FM6126A` instead of `SHIFTREG`, then re-upload and check again.
4. If brightness is uneven panel-to-panel (far panel dimmer/discolored), that's a power delivery issue — add the second power injection point mentioned in §2.2.
5. Once it looks right, raise `BRIGHTNESS` gradually while watching that your PSU doesn't get hot or sag.

---

## 6. Switching to a 2×2 Layout Later

The current code assumes a 4-wide strip. Switching to 2×2 (128×128) means rewiring the physical chain in serpentine order (e.g. bottom row left-to-right, then up and top row right-to-left) and adding a `VirtualMatrixPanel_T` wrapper in code to translate x/y coordinates on the "virtual" 128×128 canvas back to the physical serpentine chain. This isn't implemented in the current code — it's a straightforward but real code change if you go this route.

---

## 7. Ideas for Later

The current sketch does a scrolling color-cycling message with a chasing Pac-Man. Easy additions if you want to keep building on this:
- Static "logo" bitmap between scroll cycles (BMP/XBM — the panel library supports this)
- Plasma/animated background behind the text
- Wi-Fi support so the message can be updated from a phone without re-flashing