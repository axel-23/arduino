#ifndef GIF_H
#define GIF_H

#include <AnimatedGIF.h>
#include <LittleFS.h>
#include <Adafruit_ST7789.h>

extern AnimatedGIF gif;
extern Adafruit_ST7789 tft;
extern bool gifActivo;
extern String gifSeleccionado;

void gifSetup();
void play_gif(const char *filename);

#endif
