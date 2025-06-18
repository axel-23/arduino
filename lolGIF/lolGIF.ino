#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <LittleFS.h>  // Usar LittleFS en lugar de SPIFFS
#include <AnimatedGIF.h>

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS 5   // Chip select control pin
#define TFT_DC 2   // Data Command control pin
#define TFT_RST 4  // Reset pin (could connect to RST pin)

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 280  // Ajustar altura para la ST7789

// Crear objeto de pantalla ST7789
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

AnimatedGIF gif;
File f;

void setup() {
  Serial.begin(115200);  // Inicializa el puerto serial para depuración

  // Inicializar la pantalla ST7789
  tft.init(240, 280);           // Especificar las dimensiones de la pantalla
  tft.fillScreen(ST77XX_BLUE);  // Fondo azul

  // Verificar si LittleFS está montado
  if (!LittleFS.begin(true)) {
    Serial.println("Error al montar LittleFS");
    return;  // Salir si no se puede montar LittleFS
  }
  Serial.println("LittleFS montado correctamente.");

  // Listar archivos en LittleFS
  listarArchivos(LittleFS, "/", 1);  // Listar los archivos del directorio raíz

  // Inicializar GIF
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // Reproducir GIF desde la memoria interna (LittleFS)
  play_gif("/allay.gif");  // Asegúrate de que el archivo GIF esté en LittleFS
}

void loop() {
  // No es necesario poner nada en el loop si solo quieres mostrar el GIF
  play_gif("/allay.gif");
  delay(50);  // Pausa de 1 segundo entre repeticiones (puedes ajustar esto)
}

void listarArchivos(fs::FS &fs, const char *path, uint8_t niveles) {
  Serial.printf("📁 Listando: %s\n", path);
  fs::File root = fs.open(path);
  if (!root || !root.isDirectory()) {
    Serial.println("❌ No se pudo abrir el directorio");
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.printf("📂  %s/\n", file.name());
      if (niveles) listarArchivos(fs, file.path(), niveles - 1);
    } else {
      Serial.printf("📄  %s\t(%d bytes)\n", file.name(), file.size());
    }
    file = root.openNextFile();
  }
}

void *GIFOpenFile(const char *fname, int32_t *pSize) {
  f = LittleFS.open(fname, "r");
  if (f) {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
}

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1;
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > DISPLAY_WIDTH)
    iWidth = DISPLAY_WIDTH - pDraw->iX;
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y;  // current line
  if (y >= DISPLAY_HEIGHT || pDraw->iX >= DISPLAY_WIDTH || iWidth < 1)
    return;
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2)  // restore to background color
  {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0;
    while (x < iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) {
          s--;
        } else {
          *d++ = usPalette[c];
          iCount++;
        }
      }
      if (iCount) {
        tft.startWrite();
        tft.setAddrWindow(pDraw->iX + x, y, iCount, 1);
        tft.writePixels(usTemp, iCount, false, false);
        tft.endWrite();
        x += iCount;
        iCount = 0;
      }
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount) {
        x += iCount;
        iCount = 0;
      }
    }
  } else {
    s = pDraw->pPixels;
    for (x = 0; x < iWidth; x++)
      usTemp[x] = usPalette[*s++];
    tft.startWrite();
    tft.setAddrWindow(pDraw->iX, y, iWidth, 1);
    tft.writePixels(usTemp, iWidth, false, false);
    tft.endWrite();
  }
}

void play_gif(const char *filename) {
  if (gif.open(filename, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    while (gif.playFrame(true, NULL)) {
    }
    gif.close();
    Serial.println("GIF terminado, reiniciando...");
  }
}
