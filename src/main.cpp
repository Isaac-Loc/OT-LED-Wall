/*
  Rush LED Wall — scrolling banner
  Board:  Waveshare ESP32-S3-RGB-Matrix
  Panels: 64x64 HUB75 (2mm/P2, GOB), chained horizontally into one wide canvas.
          Update PANEL_CHAIN below to match how many panels are physically wired.

  Toolchain: PlatformIO (VS Code), Arduino framework, C++
  Library:   ESP32 HUB75 LED MATRIX PANEL DMA Display (mrfaptastic) — see platformio.ini
*/

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// =============================================================================
// USER SETTINGS — tweak these
// =============================================================================
static const char* MESSAGE          = "RUSH THETA TAU";
static uint8_t      BRIGHTNESS      = 160;  // 0-255. ~65% keeps well under the PSU's ceiling.
static uint16_t     SCROLL_DELAY_MS = 20;   // ms between frames. Lower = faster scroll.
static uint8_t      TEXT_SIZE       = 8;    // 1 = 6x8px chars, 2 = 12x16px, 3 = 18x24px, etc.
static int16_t      Y_NUDGE         = 4;    // fine-tune vertical position: + moves down, - moves up
static bool          SHOW_PACMAN     = true; // true = draw a Pac-Man chasing the text
static int16_t       PACMAN_RADIUS   = 25;   // pixel radius of the Pac-Man sprite
static int16_t       PACMAN_GAP      = 14;   // pixel gap kept between text's trailing edge and Pac-Man
static float          PACMAN_MOUTH_SPEED = 0.25f; // how fast the mouth chomps (higher = faster)
static int16_t         PACMAN_Y_NUDGE = -5;   // positive = move Pac-Man down, negative = move up (pixels)

// =============================================================================
// PANEL / HARDWARE CONFIG
// =============================================================================
static constexpr int PANEL_RES_X = 64;  // pixels wide of ONE panel
static constexpr int PANEL_RES_Y = 64;  // pixels tall of ONE panel
static constexpr int PANEL_CHAIN = 4;   // number of panels chained horizontally

// Waveshare ESP32-S3-RGB-Matrix fixed pin mapping (from Waveshare's own docs:
// https://docs.waveshare.com/ESP32-Peripheral-Tutorials/Display/LED-Matrix)
// The library's generic ESP32-S3 defaults match this board EXCEPT for the E line,
// which this panel needs (1/32 scan, 64x64, HUB75E). Declaring pins explicitly
// avoids that mismatch.
static const HUB75_I2S_CFG::i2s_pins PANEL_PINS = {
  4, 5, 6,       // R1, G1, B1
  7, 15, 16,     // R2, G2, B2
  18, 8, 3, 42,  // A, B, C, D
  9,             // E: required for this 1/32-scan panel
  40, 2, 41      // LAT, OE, CLK
};

// =============================================================================
// GLOBAL STATE
// =============================================================================
MatrixPanel_I2S_DMA *dma_display = nullptr;

int16_t textX       = 0;  // current horizontal scroll position
int16_t textWidth   = 0;  // measured pixel width of MESSAGE at TEXT_SIZE
int16_t textHeight  = 0;  // measured pixel height of MESSAGE at TEXT_SIZE
int16_t textYOffset = 0;  // gap between cursor and where visible pixels start (font metric)
uint8_t hue         = 0;  // drives the scrolling rainbow color cycle
uint32_t frameCount = 0;  // increments every loop, drives Pac-Man's mouth animation

// =============================================================================
// HELPERS
// =============================================================================

// Maps 0-255 to a repeating green -> red -> blue gradient.
static uint16_t colorWheel(uint8_t pos) {
  if (pos < 85) {
    return dma_display->color565(pos * 3, 255 - pos * 3, 0);
  } else if (pos < 170) {
    pos -= 85;
    return dma_display->color565(255 - pos * 3, 0, pos * 3);
  } else {
    pos -= 170;
    return dma_display->color565(0, pos * 3, 255 - pos * 3);
  }
}

// Measures MESSAGE at the current TEXT_SIZE and stores width/height/offset,
// so vertical centering accounts for the font's actual rendered bounding box
// rather than a guessed line height.
static void measureText() {
  int16_t x1, y1;
  uint16_t w, h;
  dma_display->getTextBounds(MESSAGE, 0, 0, &x1, &y1, &w, &h);
  textWidth   = static_cast<int16_t>(w);
  textHeight  = static_cast<int16_t>(h);
  textYOffset = y1;

  if (textHeight > dma_display->height()) {
    Serial.printf(
      "****** WARNING: TEXT_SIZE %d produces %dpx-tall text, "
      "but the panel is only %dpx tall. Lower TEXT_SIZE. ******\n",
      TEXT_SIZE, textHeight, dma_display->height());
  }
}

// Vertically centered cursor Y for the current text metrics + manual nudge.
static int16_t centeredTextY() {
  return ((dma_display->height() - textHeight + 1) / 2) - textYOffset + Y_NUDGE;
}

// Draws a Pac-Man sprite centered at (cx, cy) facing left (the direction it
// travels, since it's chasing text that scrolls right-to-left). The body is
// built as a fan of small triangles radiating from the center; angles inside
// the mouth wedge are simply never drawn, so there's no yellow to leak through
// at the mouth's edge (unlike drawing a full circle then erasing a wedge).
static void drawPacman(int16_t cx, int16_t cy, int16_t radius, uint32_t frame) {
  const uint16_t yellow = dma_display->color565(255, 255, 0);

  // Mouth half-angle oscillates between ~8 and ~40 degrees for a chomping look.
  float mouthDeg = 8.0f + 32.0f * fabsf(sinf(frame * PACMAN_MOUTH_SPEED));

  const float stepDeg = 6.0f; // smaller = smoother curved edge, more draw calls per frame
  float startDeg = 180.0f + mouthDeg;          // sweep starts just past one mouth edge...
  float endDeg   = 180.0f - mouthDeg + 360.0f; // ...all the way around to the other

  for (float deg = startDeg; deg < endDeg; deg += stepDeg) {
    float deg2 = (deg + stepDeg < endDeg) ? (deg + stepDeg) : endDeg;
    float rad1 = radians(deg);
    float rad2 = radians(deg2);

    int16_t x1 = cx + static_cast<int16_t>(radius * cosf(rad1));
    int16_t y1 = cy - static_cast<int16_t>(radius * sinf(rad1));
    int16_t x2 = cx + static_cast<int16_t>(radius * cosf(rad2));
    int16_t y2 = cy - static_cast<int16_t>(radius * sinf(rad2));

    dma_display->fillTriangle(cx, cy, x1, y1, x2, y2, yellow);
  }
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, PANEL_PINS);
  mxconfig.driver      = HUB75_I2S_CFG::SHIFTREG; // conventional shift-register column drivers
  mxconfig.clkphase    = false;                   // this panel needs false, or the image shifts by 1px
  mxconfig.double_buff = true;                    // smoother scrolling, no tearing

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("****** Display allocation FAILED ******");
  }

  dma_display->setBrightness8(BRIGHTNESS);
  dma_display->clearScreen();
  dma_display->setTextSize(TEXT_SIZE);
  dma_display->setTextWrap(false);
  dma_display->setTextColor(colorWheel(0));

  measureText();
  textX = dma_display->width();
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  dma_display->flipDMABuffer();
  delay(1000 / dma_display->calculated_refresh_rate);

  dma_display->clearScreen();

  hue += 2;
  dma_display->setTextColor(colorWheel(hue));
  dma_display->setCursor(textX, centeredTextY());
  dma_display->print(MESSAGE);

  if (SHOW_PACMAN) {
    // Trailing edge of the text is its rightmost pixel (textX + textWidth).
    // Keep Pac-Man that many pixels behind it, moving at the same speed.
    int16_t pacmanCX = textX + textWidth + PACMAN_GAP + PACMAN_RADIUS;
    int16_t pacmanCY = (dma_display->height() / 2) + Y_NUDGE + PACMAN_Y_NUDGE;
    drawPacman(pacmanCX, pacmanCY, PACMAN_RADIUS, frameCount);
  }
  frameCount++;

  textX--;
  if (textX < -textWidth) {
    textX = dma_display->width();
  }

  delay(SCROLL_DELAY_MS);
}