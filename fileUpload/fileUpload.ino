#include <WiFiManager.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSans12pt7b.h>       // Similar a Helvetica Neue
#include <SPI.h>
#include <FS.h>         // Librería para manejar directorios y archivos (incluye Dir)
#include "html_page.h"  // Incluir el archivo HTML

#define TFT_CS   5   // Chip select control pin
#define TFT_DC   2   // Data Command control pin
#define TFT_RST  4   // Reset pin (puedes usar -1 si no está conectado)
#define TFT_SCLK 18
#define TFT_MOSI 23

#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 280

// Definir pantalla TFT
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Crear el servidor web en el puerto 80
WebServer server(80);

// Configuración del WiFiManager
WiFiManager wifiManager;

void enableCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");                             // Permite solicitudes de cualquier origen
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");           // Métodos permitidos
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");  // Encabezados permitidos
}

void configModeCallback(WiFiManager* myWiFiManager) {
  Serial.println("Entró en modo de configuración");
  Serial.println(WiFi.softAPIP());  // Muestra la IP del AP (Portal de configuración)

  // Muestra en la pantalla TFT
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setCursor(30, 20);
  tft.print("Modo AP: ESP32_AP");
  tft.setCursor(30, 50);
  tft.print("IP: ");
  tft.print(WiFi.softAPIP());

  // Imprimir el SSID que debemos usar para conectar al portal de configuración
  Serial.print("Creado portal de configuración AP ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
  // Inicializar el puerto serie
  Serial.begin(115200);

  // Inicialización de la pantalla TFT
  // -1 para MISO si no se usa
  tft.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);    // Especifica resolución
  tft.fillScreen(ST77XX_BLACK);

  tft.setFont(&FreeSans12pt7b);  
  // Mostrar mensaje inicial en la pantalla TFT
  tft.setCursor(30, 30);
  tft.print("Conectando...");

  // Si es necesario, puedes restablecer la configuración guardada
  // wifiManager.resetSettings();

  // Configurar el callback para cuando el ESP entre en modo AP (modo configuración)
  wifiManager.setAPCallback(configModeCallback);

  // Intentar conectarse a Wi-Fi. Si no puede, entra en modo AP
  if (!wifiManager.autoConnect("ESP32_AP")) {
    Serial.println("No se pudo conectar y se agotó el tiempo de espera");
    // Si no se conecta, reinicia el ESP32
    ESP.restart();
    delay(1000);
  }

  // Si se conecta correctamente a Wi-Fi, muestra el mensaje en la pantalla TFT
  Serial.println("¡Conectado a Wi-Fi, yey :)");
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(30, 20);
  tft.print("¡Conectado!");
  tft.setCursor(30, 50);
  tft.print("IP: ");
  tft.print(WiFi.localIP());

  // Iniciar LittleFS para almacenamiento de archivos
  if (!LittleFS.begin()) {
    tft.setCursor(10, 100);
    tft.print("Error al montar LittleFS");
    return;
  }

  // Endpoint para obtener la lista de archivos subidos
  server.on("/files", HTTP_GET, []() {
    enableCORS();  // Habilitar CORS
    handleListFiles();
  });

  // Endpoint para obtener el estado de almacenamiento
  server.on("/storage", HTTP_GET, []() {
    enableCORS();  // Habilitar CORS
    handleStorageStatus();
  });

  // Página de inicio con los archivos subidos
  server.on("/", HTTP_GET, []() {
    enableCORS();                             // Habilitar CORS
    server.send(200, "text/html", htmlPage);  // Página de subida de archivos
  });

  // Configurar la ruta para manejar la subida de imágenes
  server.on(
    "/upload", HTTP_POST, []() {
      // Enviar encabezados de CORS y otros necesarios
      enableCORS();
      //server.send(201, "application/json", createJsonResponse("success", "Archivo subido con éxito"));
    },
    handleFileUpload);

  // En caso de que sea una solicitud OPTIONS (preflight)
  server.on("/upload", HTTP_OPTIONS, []() {
    enableCORS();
    server.send(200);
  });

  // Endpoint para servir una imagen
  server.on("/view", HTTP_GET, []() {
    enableCORS();  // Habilitar CORS
    handleFileView();
  });

  // Ruta para manejar la solicitud POST de eliminación de archivos
  server.on("/delete", HTTP_POST, []() {
      // Habilitar CORS para esta respuesta
      enableCORS();
      handleFileDelete(server.arg("fileDelete"));
    });

    // En caso de que sea una solicitud OPTIONS (preflight)
  server.on("/delete", HTTP_OPTIONS, []() {
    enableCORS();
    server.send(200);
  });

  // Iniciar el servidor web
  server.begin();
}

void loop() {
  // Manejar peticiones HTTP
  server.handleClient();
}

// Función para generar una respuesta JSON
String createJsonResponse(String status, String message) {
  String jsonResponse = "{\"status\": \"" + status + "\", \"message\": \"" + message + "\"}";
  return jsonResponse;
}

// Función para obtener la lista de archivos subidos en formato JSON
void handleListFiles() {
  String json = "[";
  File root = LittleFS.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      json += "{\"name\": \"" + String(file.name()) + "\", \"size\": " + String(file.size()) + "},";
      file.close();
      file = root.openNextFile();  // Obtener el siguiente archivo
    }
    root.close();
    if (json.endsWith(",")) {
      json.remove(json.length() - 1);  // Eliminar la última coma
    }
  }
  json += "]";
  server.send(200, "application/json", json);  // Enviar la lista de archivos en formato JSON
}

// Función para obtener el estado del almacenamiento en formato JSON
void handleStorageStatus() {
  long totalSpace = LittleFS.totalBytes();
  long usedSpace = LittleFS.usedBytes();
  long freeSpace = totalSpace - usedSpace;

  // Calcula el porcentaje de uso
  int percentageUsed = (usedSpace * 100) / totalSpace;

  // Crea el JSON sin formatear los tamaños
  String json = "{\"total\": " + String(totalSpace) + ", \"used\": " + String(usedSpace) + ", \"free\": " + String(freeSpace) + ", \"percentage\": " + String(percentageUsed) + "}";

  // Enviar el estado del almacenamiento
  server.send(200, "application/json", json);
}

// Función para manejar la subida de archivos
void handleFileUpload() {
  HTTPUpload& upload = server.upload();  // Obtener el archivo subido

  // Manejar los diferentes estados de la subida
  if (upload.status == UPLOAD_FILE_START) {
    // Nombre del archivo
    String fileUpload = "/" + upload.filename;
    Serial.println("Subiendo archivo: " + fileUpload);

    // Crear archivo en el sistema de archivos LittleFS
    File file = LittleFS.open(fileUpload, "w");
    if (!file) {
      Serial.println("Error al crear el archivo");
      server.send(500, "application/json", createJsonResponse("error", "Error al crear el archivo"));
      return;
    }
    file.close();  // Cerrar archivo después de la creación

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Escribir los datos del archivo
    File file = LittleFS.open("/" + upload.filename, "a");
    if (file) {
      file.write(upload.buf, upload.currentSize);  // Escribir los datos recibidos
      file.close();                                // Asegurarse de cerrar el archivo después de cada escritura
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    // Finalizar el archivo después de recibir todos los datos
    Serial.println("Archivo subido con éxito: " + upload.filename);
    server.send(201, "application/json", createJsonResponse("success", "Archivo subido con éxito"));
  }
}

// Función para servir una imagen desde LittleFS
void handleFileView() {
  String filename = server.arg("file");
  String extension = filename.substring(filename.lastIndexOf(".") + 1);
  String contentType;

  // Determinar el tipo de contenido de acuerdo a la extensión del archivo
  if (extension == "jpg" || extension == "jpeg") {
    contentType = "image/jpeg";
  } else if (extension == "png") {
    contentType = "image/png";
  } else if (extension == "gif") {
    contentType = "image/gif";
  } else if (extension == "bmp") {
    contentType = "image/bmp";
  } else {
    contentType = "application/octet-stream";  // Para otros tipos de archivos desconocidos
  }

  File file = LittleFS.open("/" + filename, "r");  // Abrir el archivo de imagen
  if (!file) {
    // Si el archivo no existe
    server.send(404, "application/json", createJsonResponse("error", "Archivo no encontrado."));
    return;
  }
  server.streamFile(file, contentType);  // Servir el archivo de imagen al cliente
  file.close();
}

// Función para eliminar un archivo
void handleFileDelete(String deleteFile) {
  // Obtener el nombre del archivo desde la solicitud
  Serial.println(deleteFile);

  if (deleteFile.length() == 0) {
    Serial.println("Nombre de archivo no especificado");
    // Si no se especifica el archivo, enviar un error 400
    server.send(404, "application/json", createJsonResponse("error", "Nombre de archivo no especificado " + deleteFile));
    return;
  }

  // Intentar eliminar el archivo
  if (LittleFS.remove("/" + deleteFile)) {
    Serial.println("Archivo eliminado: " + deleteFile);
    server.send(200, "application/json", createJsonResponse("success", "Archivo eliminado con éxito " + deleteFile));
  } else {
    // Si ocurre un error al eliminar el archivo, enviar un error 500
    Serial.println("Error al eliminar el archivo: " + deleteFile);
    server.send(500, "application/json", createJsonResponse("error", "Error al eliminar el archivo " + deleteFile));
  }
}