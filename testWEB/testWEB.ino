#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSans12pt7b.h>
#include <FS.h>
#include "html_page.h"
#include "gifDecoder.h"
#include "jpgDecoder.h"
#include "pngDecoder.h"

#define TFT_CS 5
#define TFT_DC 2
#define TFT_RST 4
#define TFT_SCLK 18
#define TFT_MOSI 23
#define TFT_BLK 21
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 280

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
AsyncWebServer server(80);
WiFiManager wifiManager;

String gifSeleccionado = "";
bool gifActivo = false;

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entró en modo de configuración");
  Serial.println(WiFi.softAPIP());

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(30, 20);
  tft.print("Modo AP");
  tft.setCursor(30, 50);
  tft.print("IP: ");
  tft.print(WiFi.softAPIP());
  Serial.print("Portal de configuración: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void enableCORS(AsyncWebServerResponse *response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

struct UploadState {
  File file;
  bool failed;
};

void setup() {
  Serial.begin(1150);
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);  // Enciende luz de fondo
  // Inicializar pantalla TFT
  tft.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(30, 30);
  tft.print("Conectando...");

  // Configurar WiFi con WiFiManager
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect("ESP32_AP", "12345678")) {
    Serial.println("Fallo al conectar, reiniciando...");
    ESP.restart();
    delay(1000);
  }

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(20, 40);
  tft.print("¡Conectado!");
  tft.setCursor(20, 70);
  tft.print("IP: ");
  tft.print(WiFi.localIP());

  // Inicializar LittleFS
  if (!LittleFS.begin()) {
    tft.setCursor(10, 100);
    tft.print("Error al montar LittleFS");
    return;
  }

  gifSetup();
  jpgSetup();
  //pngSetup();

  // Endpoints del servidor
  setupServerRoutes();

  server.begin();
}

void setupServerRoutes() {
  // Página principal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", htmlPage);
    enableCORS(response);
    request->send(response);
  });

  // Listado de archivos
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = listFilesAsJson();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    enableCORS(response);
    request->send(response);
  });

  // Estado de almacenamiento
  server.on("/storage", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = getStorageStatus();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    enableCORS(response);
    request->send(response);
  });

  // Subida de archivo
  server.on(
    "/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", createJsonResponse("success", "Archivo subido"));
      enableCORS(response);
      request->send(response);
    },
    handleFileUploadAsync);

  server.on(
    "/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
      UploadState *state = reinterpret_cast<UploadState *>(request->_tempObject);

      if (state && state->failed) {
        delete state;
        request->_tempObject = nullptr;
        AsyncWebServerResponse *response = request->beginResponse(500, "application/json",
                                                                  createJsonResponse("error", "Falló la subida del archivo"));
        enableCORS(response);
        request->send(response);
        return;
      }

      delete state;
      request->_tempObject = nullptr;

      AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
                                                                createJsonResponse("success", "Archivo subido correctamente"));
      enableCORS(response);
      request->send(response);
    },
    handleFileUploadAsync);


  // Ver archivo
  server.on("/view", HTTP_GET, [](AsyncWebServerRequest *request) {
    String file = request->getParam("file")->value();
    handleImage(file, request);
  });

  // Mostrar imagen o GIF
  server.on("/show", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("image", true)) {
      AsyncWebServerResponse *response = request->beginResponse(400, "application/json", createJsonResponse("error", "Falta parámetro"));
      enableCORS(response);
      request->send(response);
      return;
    }

    String file = "/" + request->getParam("image", true)->value();
    if (!LittleFS.exists(file)) {
      AsyncWebServerResponse *response = request->beginResponse(404, "application/json", createJsonResponse("error", "Archivo no encontrado"));
      enableCORS(response);
      request->send(response);
      return;
    }

    tft.fillScreen(ST77XX_BLACK);

    String ext = file.substring(file.lastIndexOf('.') + 1);
    ext.toLowerCase();

    if (ext == "gif") {
      gifSeleccionado = file;
      gifActivo = true;
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", createJsonResponse("success", "Mostrando GIF"));
      enableCORS(response);
      request->send(response);
    } else if (ext == "jpg" || ext == "jpeg") {
      gifActivo = false;
      show_jpeg(file.c_str(), 0, 0);
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", createJsonResponse("success", "Mostrando JPEG"));
      enableCORS(response);
      request->send(response);
    } else {
      AsyncWebServerResponse *response = request->beginResponse(415, "application/json", createJsonResponse("error", "Formato no soportado"));
      enableCORS(response);
      request->send(response);
    }
  });

  // Eliminar archivo
  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("fileDelete", true)) {
      AsyncWebServerResponse *response = request->beginResponse(400, "application/json", createJsonResponse("error", "Falta nombre de archivo"));
      enableCORS(response);
      request->send(response);
      return;
    }

    String file = "/" + request->getParam("fileDelete", true)->value();
    if (LittleFS.remove(file)) {
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json", createJsonResponse("success", "Archivo eliminado"));
      enableCORS(response);
      request->send(response);
    } else {
      AsyncWebServerResponse *response = request->beginResponse(500, "application/json", createJsonResponse("error", "Error al eliminar archivo"));
      enableCORS(response);
      request->send(response);
    }
  });

  server.on("/delete", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200);
    enableCORS(response);
    request->send(response);
  });
}

void handleFileUploadAsync(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  UploadState *state;

  if (index == 0) {
    // Inicio de subida
    if (LittleFS.exists("/" + filename)) {
      LittleFS.remove("/" + filename);
    }

    state = new UploadState;
    state->file = LittleFS.open("/" + filename, "w");
    state->failed = !state->file;

    if (state->failed) {
      Serial.println("❌ Error: No se pudo crear el archivo.");
    } else {
      Serial.println("✅ Archivo abierto para escritura: " + filename);
    }

    request->_tempObject = state;
  } else {
    state = reinterpret_cast<UploadState *>(request->_tempObject);
  }

  // Escribir datos
  if (!state->failed && len > 0) {
    size_t written = state->file.write(data, len);
    if (written != len) {
      Serial.println("❌ Error al escribir datos");
      state->failed = true;
    }
  }

  // Finalización
  if (final) {
    if (state->file) state->file.close();
    Serial.println("📁 Subida finalizada: " + filename);
  }
}



String createJsonResponse(String status, String message) {
  return "{\"status\": \"" + status + "\", \"message\": \"" + message + "\"}";
}

String listFilesAsJson() {
  String json = "[";
  File root = LittleFS.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      json += "{\"name\": \"" + String(file.name()) + "\", \"size\": " + String(file.size()) + "},";
      file = root.openNextFile();
    }
    root.close();
    if (json.endsWith(",")) json.remove(json.length() - 1);
  }
  json += "]";
  return json;
}

String getStorageStatus() {
  long total = LittleFS.totalBytes();
  long used = LittleFS.usedBytes();
  long free = total - used;
  int percent = (used * 100) / total;
  return "{\"total\": " + String(total) + ", \"used\": " + String(used) + ", \"free\": " + String(free) + ", \"percentage\": " + String(percent) + "}";
}

void handleImage(String filename, AsyncWebServerRequest *request) {
  String ext = filename.substring(filename.lastIndexOf('.') + 1);
  ext.toLowerCase();
  String contentType;

  if (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
  else if (ext == "png") contentType = "image/png";
  else if (ext == "gif") contentType = "image/gif";
  else if (ext == "bmp") contentType = "image/bmp";
  else contentType = "application/octet-stream";

  File file = LittleFS.open("/" + filename, "r");
  if (!file) {
    request->send(404, "application/json", createJsonResponse("error", "Archivo no encontrado"));
    return;
  }

  AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/" + filename, contentType);
  enableCORS(response);
  request->send(response);
}

void loop() {
  if (gifActivo && gifSeleccionado != "") {
    play_gif(gifSeleccionado.c_str());
    delay(50);
  }
}
