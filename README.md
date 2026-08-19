# Rush LED Wall — Setup Guide
**Hardware:** Waveshare ESP32-S3-RGB-Matrix driver board + 4x 64x64 (2mm/P2, GOB) HUB75 panels

---

## 1. Layout decision

You haven't picked an arrangement yet, so here's the trade-off:

| Layout | Canvas size | Wiring | Code complexity |
|---|---|---|---|
| **4 in a row** (recommended for text) | 256 × 64 | Straight chain, panel 1→2→3→4 | Simple — no special config |
| 2×2 square | 128 × 128 | Serpentine chain (zig-zag) | Needs a "virtual matrix" mapping layer |

For scrolling banner text like "RUSH THETA TAU," a **wide 256×64 strip reads better and is much easier to wire correctly on the first try**, so the guide and code below default to that. A note on switching to 2×2 is at the bottom.

---

## 2. Wiring

### 2.1 Data (HUB75 ribbon cables)
Each panel has an **IN** header and an **OUT** header.

1. Connect the ESP32-S3-RGB-Matrix board's HUB75 output → Panel 1 **IN**
2. Panel 1 **OUT** → Panel 2 **IN**
3. Panel 2 **OUT** → Panel 3 **IN**
4. Panel 3 **OUT** → Panel 4 **IN**

Physically arrange the 4 panels left-to-right in that same order (1, 2, 3, 4) so the chain order matches what you see. Keep ribbon runs short — under ~1.5–2m is safest for signal integrity at full chain length; if you must go longer, a HUB75 signal booster/re-clocker between panels helps.

The Waveshare driver board uses its **default ESP32-S3 HUB75 pin mapping out of the box** — you don't need to wire or define custom GPIOs.

### 2.2 Power
This is the part people underbuild. Waveshare's own spec for their 64×64 panels: **each panel needs its own 5V/4A (peak, full-white) supply.** For 4 panels, that's up to **16A at 5V (~80W)** in a worst case, all-white, full-brightness scenario. Realistically, scrolling text on a black background draws far less — but size your PSU for the worst case so it never sags or overheats.

- **Power the panels directly from the PSU, not through the ESP32 board.** The driver board's onboard 5V pass-through is only rated for roughly one panel's worth of current.
- Use thick wire (18AWG or better) and, ideally, a **power injection point at both ends of the panel chain** (inject 5V/GND into panel 1 *and* panel 4) — voltage droop across 4 daisy-chained panels is a common cause of dim/discolored far-end panels.
- Common ground: PSU ground, panel grounds, and ESP32 board ground must all be tied together.
- Check your PSU's continuous rating against the ~16A worst case. If it's undersized, just cap `BRIGHTNESS` lower in the code (see below) — a text banner rarely needs full brightness anyway.

---

## 3. Arduino IDE setup

1. Install **Arduino IDE** (2.x).
2. In Boards Manager, install **"esp32 by Espressif Systems"** — Waveshare's docs specify version **3.3.7**.
3. Select board: an ESP32-S3 variant (e.g. "ESP32S3 Dev Module") matching the ESP32-S3-RGB-Matrix. Make sure PSRAM is enabled ("OPI PSRAM") in the board menu — the driver board has 16MB PSRAM and the display buffer benefits from it.
4. Install the display library. Either:
   - Library Manager → search **"ESP32 HUB75 LED MATRIX PANEL DMA Display"** (by mrfaptastic), **or**
   - Use the copy bundled inside Waveshare's example repo: https://github.com/waveshareteam/ESP32-S3-RGB-Matrix (each example folder under `example/arduino_v3.3.7/` already includes the library source alongside the `.ino`).
5. Open `rush_led_wall.ino` (attached), plug in the board via USB-C, select the right COM port, and hit Upload.

---

## 4. First boot checklist

1. **Before connecting panel power**, upload the code with panels at low brightness (the provided code defaults to 100/255) and connect only USB power to the ESP32 board.
2. Connect panel 5V power. You should see scrolling text.
3. If the image looks **scrambled, shifted, or has wrong colors**: open the `.ino` and uncomment the line:
   ```cpp
   // mxconfig.driver = HUB75_I2S_CFG::FM6126A;
   ```
   Some GOB panels use FM6126A-family driver chips that need an extra init sequence. Re-upload and check again.
4. If brightness is uneven panel-to-panel (far panel dimmer/discolored): that's a power delivery issue — add the second power injection point mentioned in §2.2.
5. Once it looks right, raise `BRIGHTNESS` gradually while watching your PSU doesn't get hot or sag.

---

## 5. Switching to a 2×2 layout later

If you decide to go 2×2 (128×128) instead of a 4-wide strip, the physical chain is wired in a serpentine order (1→2 left-to-right on the bottom row, then up and 3→4 right-to-left on the top row, or similar), and the code needs a `VirtualMatrixPanel_T` wrapper to translate x/y coordinates on the "virtual" 128×128 canvas back to the physical serpentine chain. Say the word and I'll write that version — it's a straightforward swap once you've confirmed the row/column wiring.

---

## 6. Content ideas for later
The current sketch just does a scrolling color-cycling message. Easy additions once this is solid:
- Static "logo" bitmap between scroll cycles (BMP/XBM, panel supports this — see Waveshare's `06_BitmapIcons` example)
- Plasma/animated background behind the text (Waveshare's `02_PatternPlasma` example)
- Wi-Fi so you can update the message from your phone without re-flashing