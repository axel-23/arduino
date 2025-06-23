#ifndef JPG_DECODER_H
#define JPG_DECODER_H

#include <TJpg_Decoder.h>
#include <LittleFS.h>
#include <Adafruit_ST7789.h>

extern Adafruit_ST7789 tft;

void jpgSetup();
void show_jpeg(const char *filename, int xpos, int ypos);

#endif
