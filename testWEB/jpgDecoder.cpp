#include "jpgDecoder.h"

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (y >= tft.height()) return 0;
  tft.drawRGBBitmap(x, y, bitmap, w, h);
  return 1;
}

void jpgSetup() {
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);
}

void show_jpeg(const char *filename, int xpos, int ypos) {
  if (!LittleFS.exists(filename)) {
    Serial.printf("Archivo JPEG %s no encontrado\n", filename);
    return;
  }

  uint16_t w, h;
  TJpgDec.getFsJpgSize(&w, &h, filename, LittleFS);
  Serial.printf("JPEG %s - ancho: %d, alto: %d\n", filename, w, h);

  TJpgDec.drawFsJpg(xpos, ypos, filename, LittleFS);
}
