/*
  Rush LED Wall — scrolling banner
  Board:  Waveshare ESP32-S3-RGB-Matrix
  Panels: 4x 64x64 HUB75 (2mm/P2, GOB), chained in a single row -> 256x64 canvas

  Toolchain: PlatformIO (VS Code), Arduino framework, C++
  Library:   ESP32 HUB75 LED MATRIX PANEL DMA Display (mrfaptastic) — see platformio.ini
*/

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// ---------------------------------------------------------------------------
// EDIT ME
// ---------------------------------------------------------------------------
static const char*  MESSAGE      = "RUSH THETA TAU";
static uint8_t       BRIGHTNESS  = 160;   // 0-255. ~65% keeps well under your 22A PSU ceiling.
static uint16_t      SCROLL_DELAY_MS = 30; // ms between frames. Lower = faster scroll.
// ---------------------------------------------------------------------------

static constexpr int PANEL_RES_X = 64;   // pixels wide of ONE panel
static constexpr int PANEL_RES_Y = 64;   // pixels tall of ONE panel
static constexpr int PANEL_CHAIN = 1;    // 4 panels chained in a single row -> 256x64 total

MatrixPanel_I2S_DMA *dma_display = nullptr;

int16_t textX = 0;
int16_t textWidth = 0;
uint8_t hue = 0;

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

void setup() {
  Serial.begin(115200);
  delay(200);

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  mxconfig.clkphase    = false;
  mxconfig.double_buff = true;   // smoother scrolling, no tearing

  // If the panels show scrambled/wrong colors on first boot, uncomment this:
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("****** Display allocation FAILED ******");
  }

  dma_display->setBrightness8(BRIGHTNESS);
  dma_display->clearScreen();

  dma_display->setTextSize(1);        // built-in 6x8 font
  dma_display->setTextWrap(false);
  dma_display->setTextColor(colorWheel(0));

  int16_t x1, y1;
  uint16_t w, h;
  dma_display->getTextBounds(MESSAGE, 0, 0, &x1, &y1, &w, &h);
  textWidth = static_cast<int16_t>(w);

  textX = dma_display->width();
}

void loop() {
  dma_display->flipDMABuffer();
  delay(1000 / dma_display->calculated_refresh_rate);

  dma_display->clearScreen();

  hue += 2;
  dma_display->setTextColor(colorWheel(hue));
  dma_display->setCursor(textX, (dma_display->height() / 2) - 4);
  dma_display->print(MESSAGE);

  textX--;
  if (textX < -textWidth) {
    textX = dma_display->width();
  }

  delay(SCROLL_DELAY_MS);
}
