// ESP32 Cam v1.0.0
// Author Juan Maioli
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <time.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"

// --- Definición de Pines Cámara (AI Thinker) ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define LED_FLASH_GPIO    4

// --- Configuración PWM LED ---
#define LED_PWM_CHANNEL   7 // Canal 7 para no chocar con la cámara (usa 0 y 1)
#define LED_PWM_FREQ      5000
#define LED_PWM_BITS      8

// --- Variables Globales ---
const String firmware_version = "1.7.0";
Preferences preferences;
SemaphoreHandle_t dataMutex;
TaskHandle_t networkTaskHandle;

String config_desc = "Cámara";
String config_ota_pass = "ArduinoOTA";

const char* hostname_prefix = "Esp32Cam-";
String serial_number;
String id_Esp;
String localIP;

// --- Optimización: Buffer Circular Estático ---
const int CONSOLE_BUF_SIZE = 4096;
char consoleBuffer[CONSOLE_BUF_SIZE];
int consoleHead = 0;
bool consoleLooped = false;

// --- Objetos Globales ---
WebServer server(80);

// --- Funciones Auxiliares ---
void getFormattedTime(char* buffer, size_t size) {
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) {
    snprintf(buffer, size, "00:00:00");
    return;
  }
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  strftime(buffer, size, "%H:%M:%S", &timeinfo);
}

String getFormattedDate() {
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) return "Fecha no sincronizada";
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

void logToConsole(const String& msg) {
  char timeBuf[16];
  getFormattedTime(timeBuf, sizeof(timeBuf));
  
  String fullMsg = String("[") + timeBuf + "] " + msg + "\n";
  const char* rawMsg = fullMsg.c_str();
  size_t len = fullMsg.length();

  Serial.print(rawMsg); 

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))) {
    for (size_t i = 0; i < len; i++) {
      consoleBuffer[consoleHead] = rawMsg[i];
      consoleHead++;
      if (consoleHead >= CONSOLE_BUF_SIZE) {
        consoleHead = 0;
        consoleLooped = true;
      }
    }
    xSemaphoreGive(dataMutex);
  }
}

// --- Configuración OTA ---
void setupOTA() {
  ArduinoOTA.setHostname(id_Esp.c_str());
  ArduinoOTA.setPassword(config_ota_pass.c_str());

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    // Nota: No usamos logToConsole aquí porque durante el flash puede ser inestable
    Serial.println("OTA: Inicio de actualización de " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA: Actualización completada.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Fallo de Autenticación");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Fallo de Inicio");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Fallo de Conexión");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Fallo de Recepción");
    else if (error == OTA_END_ERROR) Serial.println("Fallo de Finalización");
  });

  ArduinoOTA.begin();
  logToConsole("Servicio OTA iniciado (Pass: " + config_ota_pass + ")");
}

// --- Contenido Web Estático (PROGMEM) ---
const char STYLE_CSS[] PROGMEM = R"rawliteral(
:root { --bg-color: #f0f2f5; --container-bg: #ffffff; --text-primary: #1c1e21; --text-secondary: #4b4f56; --pre-bg: #f5f5f5; --hr-color: #e0e0e0; --dot-color: #bbb; --dot-active-color: #717171; --input-bg: #fff; --input-border: #ccc; --console-bg: #1e1e1e; --console-text: #00ff00; }
@media (prefers-color-scheme: dark) { :root { --bg-color: #121212; --container-bg: #1e1e1e; --text-primary: #e0e0e0; --text-secondary: #b0b3b8; --pre-bg: #2a2a2a; --hr-color: #3e4042; --dot-color: #555; --dot-active-color: #ccc; --input-bg: #333; --input-border: #555; } }
body { background-color: var(--bg-color); color: var(--text-secondary); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 1rem 0;}
.container { background-color: var(--container-bg); padding: 2rem; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); text-align: left; width: 400px; height: 80vh; position: relative; display: flex; flex-direction: column; }
h1, h2 { color: var(--text-primary); margin-bottom: 1rem; text-align: center; } p { color: var(--text-secondary); font-size: 1.1rem; margin: 0.5rem 0; } strong { color: var(--text-primary); } hr { border: 0; height: 1px; background-color: var(--hr-color); margin: 1.5rem 0; }
.carousel-container { position: relative; flex-grow: 1; overflow: hidden; } .carousel-slide { display: none; height: 100%; width: 100%; flex-basis: 100%; flex-shrink: 0; overflow-y: auto; padding-right: 15px; box-sizing: border-box; word-wrap: break-word; }
.fade { animation-name: fade; animation-duration: 0.5s; } @keyframes fade { from {opacity: .4} to {opacity: 1} }
.prev, .next { cursor: pointer; position: absolute; top: 50%; transform: translateY(-50%); width: auto; padding: 16px; color: var(--text-primary); font-weight: bold; font-size: 24px; transition: 0.3s; user-select: none; z-index: 10; } .prev { left: -50px; } .next { right: -50px; } .prev:hover, .next:hover { background-color: rgba(0,0,0,0.2); border-radius: 50%; }
.dots { text-align: center; padding-top: 20px; } .dot { cursor: pointer; height: 15px; width: 15px; margin: 0 2px; background-color: var(--dot-color); border-radius: 50%; display: inline-block; transition: background-color 0.3s ease; } .active, .dot:hover { background-color: var(--dot-active-color); }
.emoji-container { text-align: center; margin-top: 15px; margin-bottom: 15px; } .emoji { font-size: 4em; line-height: 1; display: inline-block; vertical-align: middle; }
.button { background-color: #4CAF50; color: white; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 10px 0; cursor: pointer; border-radius: 5px; border: none;} .button:hover { background-color: #45a049; } .button[disabled] { background-color: #555; color: #eee; border: 1px solid #eeeeee; cursor: not-allowed; }
.center-button { text-align: center; } 
.form-row { display: flex; gap: 10px; } .form-group { flex: 1; }
input[type=text], input[type=password] { width: 100%; padding: 12px 20px; margin: 8px 0; box-sizing: border-box; border: 1px solid var(--input-border); border-radius: 4px; background-color: var(--input-bg); color: var(--text-primary); }
#console-output { width: 90%; height: 237px; background-color: #333; color: #eee; font-family: monospace; border: 1px solid #444; border-radius: 4px; padding: 10px; resize: none; overflow-y: scroll; font-size: 1.1em; font-weight: bold; margin: 0 auto 10px auto; display: block; }
#console-input { width: calc(90% - 80px); display: inline-block; margin-left: 5%; }
#console-send { width: 70px; display: inline-block; padding: 12px 0; margin-left: 5px; }
@media (max-width: 768px) { .container { width: 95%; height: 80vh; padding: 1rem; } .prev, .next { top: auto; bottom: 5px; transform: translateY(0); } .prev { left: 10px; } .next { right: 10px; } }
)rawliteral";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;

unsigned long previousTimeUpdate = 0;
const long timeInterval = 60000;      

void loadSettings() {
  preferences.begin("app-config", true);
  config_desc = preferences.getString("desc", "Casa");
  config_ota_pass = preferences.getString("ota_pass", "ArduinoOTA");
  preferences.end();
}

String leftRepCadena(String mac) {
  String cleanMac = "";
  for (int i = 0; i < mac.length(); i++) {
    if (isAlphaNumeric(mac[i])) cleanMac += mac[i];
  }
  return (cleanMac.length() >= 4) ? cleanMac.substring(cleanMac.length() - 4) : "XXXX";
}

String getUniqueId() {
  uint64_t mac = ESP.getEfuseMac();
  return String((uint32_t)(mac >> 32), HEX) + String((uint32_t)mac, HEX);
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA; // Empezar en VGA para asegurar estabilidad
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    logToConsole("Error: Fallo al inicializar camara (0x" + String(err, HEX) + ")");
    return false;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV2640_PID) {
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  }
  
  // Drenar frames iniciales para estabilizar el sensor
  for (int i = 0; i < 10; i++) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(50);
  }

  logToConsole("Camara inicializada y estabilizada.");
  return true;
}

// --- Tarea de Segundo Plano (Core 0) ---
void networkTask(void * parameter) {
  for (;;) {
    String tempLocalIP = WiFi.localIP().toString();
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      localIP = tempLocalIP;
      xSemaphoreGive(dataMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(5000)); 
  }
}

// --- Handler de Streaming MJPEG ---
void handleStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      logToConsole("Fallo captura de frame");
      break;
    }

    String header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + String(fb->len) + "\r\n\r\n";
    server.sendContent(header);
    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");
    
    esp_camera_fb_return(fb);
    delay(1); // <--- AGREGADO: Pequeño respiro
  }
}

// --- Handlers de Cámara (Status y Control) ---
void handleStatus() {
  sensor_t * s = esp_camera_sensor_get();
  if (!s) {
    server.send(500, "text/plain", "Camara no inicializada");
    return;
  }
  
  String json = "{";
  json += "\"framesize\":" + String(s->status.framesize) + ",";
  json += "\"quality\":" + String(s->status.quality) + ",";
  json += "\"brightness\":" + String(s->status.brightness) + ",";
  json += "\"contrast\":" + String(s->status.contrast) + ",";
  json += "\"saturation\":" + String(s->status.saturation) + ",";
  json += "\"special_effect\":" + String(s->status.special_effect) + ",";
  json += "\"wb_mode\":" + String(s->status.wb_mode) + ",";
  json += "\"awb\":" + String(s->status.awb) + ",";
  json += "\"awb_gain\":" + String(s->status.awb_gain) + ",";
  json += "\"aec\":" + String(s->status.aec) + ",";
  json += "\"aec2\":" + String(s->status.aec2) + ",";
  json += "\"ae_level\":" + String(s->status.ae_level) + ",";
  json += "\"aec_value\":" + String(s->status.aec_value) + ",";
  json += "\"agc\":" + String(s->status.agc) + ",";
  json += "\"agc_gain\":" + String(s->status.agc_gain) + ",";
  json += "\"gainceiling\":" + String(s->status.gainceiling) + ",";
  json += "\"bpc\":" + String(s->status.bpc) + ",";
  json += "\"wpc\":" + String(s->status.wpc) + ",";
  json += "\"raw_gma\":" + String(s->status.raw_gma) + ",";
  json += "\"lenc\":" + String(s->status.lenc) + ",";
  json += "\"vflip\":" + String(s->status.vflip) + ",";
  json += "\"hmirror\":" + String(s->status.hmirror) + ",";
  json += "\"dcw\":" + String(s->status.dcw) + ",";
  json += "\"colorbar\":" + String(s->status.colorbar);
  json += "}";

  server.send(200, "application/json", json);
}

void handleControl() {
  if (!server.hasArg("var") || !server.hasArg("val")) {
    server.send(400, "text/plain", "Faltan parametros var/val");
    return;
  }

  String var = server.arg("var");
  int val = server.arg("val").toInt();
  sensor_t * s = esp_camera_sensor_get();
  int res = 0;

  if (var == "framesize") {
    if (s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
  } else if (var == "quality") res = s->set_quality(s, val);
  else if (var == "contrast") res = s->set_contrast(s, val);
  else if (var == "brightness") res = s->set_brightness(s, val);
  else if (var == "saturation") res = s->set_saturation(s, val);
  else if (var == "gainceiling") res = s->set_gainceiling(s, (gainceiling_t)val);
  else if (var == "colorbar") res = s->set_colorbar(s, val);
  else if (var == "awb") res = s->set_whitebal(s, val);
  else if (var == "agc") res = s->set_gain_ctrl(s, val);
  else if (var == "aec") res = s->set_exposure_ctrl(s, val);
  else if (var == "hmirror") res = s->set_hmirror(s, val);
  else if (var == "vflip") res = s->set_vflip(s, val);
  else if (var == "awb_gain") res = s->set_awb_gain(s, val);
  else if (var == "agc_gain") res = s->set_agc_gain(s, val);
  else if (var == "aec_value") res = s->set_aec_value(s, val);
  else if (var == "aec2") res = s->set_aec2(s, val);
  else if (var == "dcw") res = s->set_dcw(s, val);
  else if (var == "bpc") res = s->set_bpc(s, val);
  else if (var == "wpc") res = s->set_wpc(s, val);
  else if (var == "raw_gma") res = s->set_raw_gma(s, val);
  else if (var == "lenc") res = s->set_lenc(s, val);
  else if (var == "special_effect") res = s->set_special_effect(s, val);
  else if (var == "wb_mode") res = s->set_wb_mode(s, val);
  else if (var == "ae_level") res = s->set_ae_level(s, val);
  else if (var == "flash") {
    ledcWrite(LED_FLASH_GPIO, val); // Pin directo en v3.x
    res = 0;
  }
  else {
    res = -1;
  }

  if (res == 0) {
    server.send(200, "text/plain", "OK");
  } else {
    server.send(500, "text/plain", "Error configurando camara");
  }
}

// --- Handler de Captura a SD ---
void handleCapture() {
  if (SD_MMC.cardType() == CARD_NONE) {
    server.send(500, "text/plain", "Error: No hay tarjeta SD");
    return;
  }

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Error: Fallo captura de camara");
    return;
  }

  // Generar nombre de archivo único
  String path = "/capture_" + String(millis()) + ".jpg";
  
  fs::FS &fs = SD_MMC;
  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file) {
    server.send(500, "text/plain", "Error: Fallo al abrir archivo en SD");
    esp_camera_fb_return(fb);
    return;
  }

  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  logToConsole("Foto guardada: " + path);
  server.send(200, "text/plain", "Guardado: " + path);
}

void handleSaveConfig() {
  bool restartNeeded = false;

  if (server.hasArg("desc")) {
    String d = server.arg("desc");
    d.trim();
    if (d.length() > 0 && d.length() <= 50) {
      preferences.begin("app-config", false);
      preferences.putString("desc", d);
      preferences.end();
      config_desc = d;
      logToConsole("Configuracion: Nueva descripcion guardada.");
    }
  }

  if (server.hasArg("otapass")) {
    String p = server.arg("otapass");
    p.trim();
    if (p.length() > 0 && p.length() <= 32) {
      if (p != config_ota_pass) {
        preferences.begin("app-config", false);
        preferences.putString("ota_pass", p);
        preferences.end();
        config_ota_pass = p;
        logToConsole("Configuracion: Nueva clave OTA guardada. Se requiere reinicio.");
        restartNeeded = true;
      }
    }
  }
  
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");

  if (restartNeeded) {
    delay(2000);
    ESP.restart();
  }
}

// Handler Optimizado para leer Logs desde Buffer Circular (v2: memcpy)
void handleConsoleLogs() {
  // Usamos heap para el buffer temporal y evitar fragmentar con String +=
  char* tempLogs = new char[CONSOLE_BUF_SIZE + 1];
  
  if (!tempLogs) {
    server.send(500, "text/plain", "Error: Low Memory");
    return;
  }

  int len = 0;

  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100))) {
    if (consoleLooped) {
      // Parte 1: Desde head hasta el final
      int part1 = CONSOLE_BUF_SIZE - consoleHead;
      memcpy(tempLogs, consoleBuffer + consoleHead, part1);
      
      // Parte 2: Desde el inicio hasta head
      memcpy(tempLogs + part1, consoleBuffer, consoleHead);
      len = part1 + consoleHead;
    } else {
      // Solo desde 0 hasta head
      memcpy(tempLogs, consoleBuffer, consoleHead);
      len = consoleHead;
    }
    xSemaphoreGive(dataMutex);
  }
  
  tempLogs[len] = '\0'; // Null terminate
  server.send(200, "text/plain", tempLogs);
  delete[] tempLogs;
}

void handleConsoleCommand() {
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    logToConsole("> COMANDO: " + cmd);
    
    if (cmd == "ping") {
      logToConsole("Pong! El sistema esta activo.");
    } else if (cmd == "restart") {
      logToConsole("Reiniciando sistema en 3 segundos...");
      server.send(200, "text/plain", "OK");
      delay(3000);
      ESP.restart();
      return;
    } else if (cmd == "ip") {
      logToConsole("IP Local: " + WiFi.localIP().toString());
    } else {
      logToConsole("Comando no reconocido: " + cmd);
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Falta parametro 'cmd'");
  }
}

void handleGetConfig() {
  if (!server.hasArg("pass") || server.arg("pass") != config_ota_pass) {
    server.send(403, "text/plain", "Clave Incorrecta");
    return;
  }
  // Generamos el formulario HTML dinámicamente solo si la clave es correcta
  String html = F("<form action='/save' method='POST'><div class='form-row'><div class='form-group'><p><strong>Descripci&oacute;n:</strong><br><input type='text' name='desc' value='");
  html += config_desc;
  html += F("' maxlength='50' placeholder='Ej: Casa'></p></div><div class='form-group'><p><strong>Clave OTA:</strong><br><input type='password' name='otapass' value='");
  html += config_ota_pass;
  html += F("' maxlength='32'></p></div></div><div class='center-button'><button type='submit' class='button'>💾 Guardar Cambios</button></div></form>");
  server.send(200, "text/html", html);
}

void handleRoot() {
    String _locIP;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100))) { 
        _locIP = localIP;
        xSemaphoreGive(dataMutex);
    } else {
        _locIP = "Cargando...";
    }

    String _mac = WiFi.macAddress();
    char timeBuf[16];
    getFormattedTime(timeBuf, sizeof(timeBuf));

    unsigned long totalSeconds = millis() / 1000;
    unsigned long days = totalSeconds / 86400;
    unsigned long hours = (totalSeconds % 86400) / 3600;
    unsigned long minutes = ((totalSeconds % 86400) % 3600) / 60;
    unsigned long seconds = ((totalSeconds % 86400) % 3600) % 60;
    
    char uptimeBuf[30];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "%lud %luh %lum %lus", days, hours, minutes, seconds);

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    
    // --- Cabecera (Método dinámico F() como ESP8266) ---
    String header = F("<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>");
    header += F("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
    header += F("<link rel='icon' href='data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>📟</text></svg>'>");
    header += F("<title>Estado del Dispositivo (ESP32)</title><link rel='stylesheet' href='/style.css'>");
    header += F("</head><body><div class='container'><div class='carousel-container'>");
    server.sendContent(header);

    String chunk;
    chunk.reserve(2048); 
    
    // --- Slide 1: Estado del Dispositivo ---
    chunk = "<div class='carousel-slide fade'><h2>Estado - " + config_desc + "</h2><div class='emoji-container'><span class='emoji'>📟</span></div><br>";
    chunk += "<h3><strong>📦 Versión:</strong> " + firmware_version + "<br>";
    chunk += "<strong>📅 Fecha:</strong> " + getFormattedDate() + "<br>";
    chunk += "<strong>⌚ Hora:</strong> <span id='current-time'>" + String(timeBuf) + "</span><br>";
    chunk += "<strong>🖥️ Hostname:</strong> " + id_Esp + "<br>";
    chunk += "<strong>🏠 IP Privada:</strong> " + _locIP + "<br>";
    chunk += "<strong>🆔 MAC:</strong> " + _mac + "<br>";
    chunk += "<strong>🧠 Free Heap:</strong> " + String(ESP.getFreeHeap() / 1024.0, 2) + " KB<br>";
    chunk += "<strong>⚡ Uptime:</strong> " + String(uptimeBuf) + "</h3></div>";
    server.sendContent(chunk);

    // --- Slide 2: Cámara ---
    chunk = "<div class='carousel-slide fade'><h2>Cámara</h2><div class='emoji-container'><span class='emoji'>📷</span></div><br>";
    chunk += "<div style='text-align:center;'><img id='stream' src='' style='width:100%; border-radius:8px; display:none; background:#333;' alt='Stream'><br>";
    chunk += "<div class='center-button' style='display:flex; justify-content:center; gap:10px; flex-wrap:wrap;'>";
    chunk += "<button id='toggle-stream' class='button' onclick='toggleStream()'>▶️ Stream</button>";
    chunk += "<button class='button' style='background-color:#2196F3;' onclick='takePhoto()'>📸 Capturar</button>";
    chunk += "<button id='btn-flash' class='button' style='background-color:#FFC107; color:black;' onclick='toggleFlashQuick()'>🔦 Flash</button>";
    chunk += "</div><p id='snap-msg' style='font-size:0.8em; color:green;'></p></div></div>";
    server.sendContent(chunk);

    // --- Slide 3: Config. de Cam. ---
    chunk = "<div class='carousel-slide fade'><h2>Config. de Cam.</h2><div class='emoji-container'><span class='emoji'>🎛️</span></div><br>";
    chunk += "<div id='cam-controls' style='font-size:0.9em;'>";
    chunk += "<p><strong>Resolución:</strong><br><select id='framesize' onchange='updateCam(this)' style='width:100%; padding:8px;'>";
    chunk += "<option value='13'>UXGA (1600x1200)</option><option value='11'>HD (1280x720)</option><option value='10'>XGA (1024x768)</option><option value='9'>SVGA (800x600)</option><option value='8'>VGA (640x480)</option><option value='5'>QVGA (320x240)</option></select></p>";
    chunk += "<p><strong>Brillo Flash:</strong><br><input type='range' id='flash' min='0' max='255' value='0' onchange='updateCam(this)' style='width:100%;'></p>";
    chunk += "<p><strong>Calidad (10-63):</strong><br><input type='range' id='quality' min='10' max='63' value='12' onchange='updateCam(this)' style='width:100%;'></p>";
    chunk += "<p><strong>Brillo Cam (-2 a 2):</strong><br><input type='range' id='brightness' min='-2' max='2' value='0' onchange='updateCam(this)' style='width:100%;'></p>";
    chunk += "</div></div>";
    server.sendContent(chunk);

    // --- Slide 4: Consola Web ---
    chunk = "<div class='carousel-slide fade'><h2>Consola Web</h2><div class='emoji-container'><span class='emoji'>💻</span></div><br>";
    chunk += "<textarea id='console-output' readonly>Cargando logs...</textarea>";
    chunk += "<div class='center-button'><input type='text' id='console-input' placeholder='Escribe un comando...'><button id='console-send' class='button' onclick='sendCommand()'>Env</button></div>";
    chunk += "<p style='font-size:0.8em; text-align:center;'>Comandos: ping, ip, restart</p></div>";
    server.sendContent(chunk);

    // --- Slide 5: Configuración (Protegida) ---
    chunk = "<div class='carousel-slide fade'><h2>Configuraci&oacute;n</h2><div class='emoji-container'><span class='emoji'>⚙️</span></div><br>";
    chunk += "<div id='lock-screen' style='text-align:center;'>";
    chunk += "<p>Ingrese Clave OTA para editar:</p>";
    chunk += "<input type='password' id='unlock-pass' placeholder='Clave OTA' style='max-width:200px;'><br>";
    chunk += "<button class='button' onclick='unlockConfig()'>🔓 Desbloquear</button>";
    chunk += "<p id='unlock-msg' style='color:red;font-size:0.9em;'></p></div>";
    chunk += "<div id='config-content'></div></div>"; // Contenedor vacío para el form
    server.sendContent(chunk);

    chunk = "</div>"; // Cierra carousel-container
    chunk += "<a class='prev' onclick='changeSlide(-1)'>&#10094;</a><a class='next' onclick='changeSlide(1)'>&#10095;</a>";
    chunk += "<div class='dots'><span class='dot' onclick='currentSlide(1)'></span><span class='dot' onclick='currentSlide(2)'></span><span class='dot' onclick='currentSlide(3)'></span><span class='dot' onclick='currentSlide(4)'></span><span class='dot' onclick='currentSlide(5)'></span></div></div>";
    chunk += "<script>let slideIndex=1;showSlide(slideIndex);function changeSlide(n){showSlide(slideIndex+=n)}function currentSlide(n){showSlide(slideIndex=n)}function showSlide(n){let i;let s=document.getElementsByClassName('carousel-slide');let d=document.getElementsByClassName('dot');if(n>s.length){slideIndex=1}if(n<1){slideIndex=s.length}for(i=0;i<s.length;i++){s[i].style.display='none'}for(i=0;i<d.length;i++){d[i].className=d[i].className.replace(' active','')};s[slideIndex-1].style.display='block';d[slideIndex-1].className+=' active'}function updateTime(){fetch('/time').then(r=>r.text()).then(d=>{if(d)document.getElementById('current-time').innerText=d})}setInterval(updateTime,900000);";
    chunk += "function toggleStream(){let i=document.getElementById('stream');let b=document.getElementById('toggle-stream');if(i.style.display==='none'){i.src='/stream';i.style.display='block';b.innerText='⏹️ Stop'}else{window.stop();i.src='';i.style.display='none';b.innerText='▶️ Stream'}}";
    chunk += "function updateCam(el){fetch('/control?var='+el.id+'&val='+el.value)}";
    chunk += "let flashOn=false;function toggleFlashQuick(){flashOn=!flashOn;let v=flashOn?255:0;fetch('/control?var=flash&val='+v);document.getElementById('btn-flash').innerText=flashOn?'📴 Off':'🔦 Flash'}";
    chunk += "function takePhoto(){let m=document.getElementById('snap-msg');m.innerText='Capturando...';fetch('/capture').then(r=>r.text()).then(t=>{m.innerText=t;setTimeout(()=>m.innerText='',3000)}).catch(e=>{m.innerText='Error: '+e.message})}";
    chunk += "function updateConsole(){fetch('/console/logs').then(r=>r.text()).then(d=>{let c=document.getElementById('console-output');if(c.value!==d){c.value=d;c.scrollTop=c.scrollHeight}})}";
    chunk += "function sendCommand(){let i=document.getElementById('console-input');let v=i.value;if(!v)return;fetch('/console/cmd?cmd='+encodeURIComponent(v)).then(()=>{i.value='';updateConsole()})}";
    chunk += "function unlockConfig(){let p=document.getElementById('unlock-pass').value;let m=document.getElementById('unlock-msg');m.innerText='Verificando...';fetch('/config/get?pass='+encodeURIComponent(p)).then(r=>{if(r.status===200){return r.text()}else{throw new Error('Clave incorrecta')}}).then(h=>{document.getElementById('lock-screen').style.display='none';document.getElementById('config-content').innerHTML=h}).catch(e=>{m.innerText=e.message})}";
    chunk += "setInterval(updateConsole,2000);"; 
    chunk += "</script></body></html>";

    server.sendContent(chunk);
}

void handleTimeRequest() {
  char timeBuf[16];
  getFormattedTime(timeBuf, sizeof(timeBuf));
  server.send(200, "text/plain", timeBuf);
}

void handleCss() {
  server.send_P(200, "text/css", STYLE_CSS);
}

// --- Setup y Loop ---

void setup() {
    Serial.begin(115200);
    delay(1000);
    dataMutex = xSemaphoreCreateMutex();
    
    logToConsole("Iniciando WebServer ESP32...");

    loadSettings();

    initCamera();

    // Configurar PWM para Linterna (Nueva API v3.x)
    ledcAttach(LED_FLASH_GPIO, LED_PWM_FREQ, LED_PWM_BITS);
    ledcWrite(LED_FLASH_GPIO, 0); // Apagar al inicio

    // Inicializar SD en modo 1-bit para liberar GPIO 4 (Flash)
    if(!SD_MMC.begin("/sdcard", true)){
      logToConsole("Error: Fallo al montar SD (o no insertada)");
    } else {
      uint8_t cardType = SD_MMC.cardType();
      if(cardType == CARD_NONE){
         logToConsole("Error: No hay tarjeta SD");
      } else {
         logToConsole("SD montada correctamente (1-bit mode)");
      }
    }

    WiFi.mode(WIFI_STA); 
    delay(100); 
    serial_number = WiFi.macAddress();
    
    if (serial_number == "" || serial_number == "00:00:00:00:00:00") {
      serial_number = getUniqueId();
    }
    
    id_Esp = String(hostname_prefix) + leftRepCadena(serial_number);
    WiFi.setHostname(id_Esp.c_str());

    logToConsole("Hostname: " + id_Esp);

    WiFiManager wifiManager;
    logToConsole("Conectando WiFi...");
    if (!wifiManager.autoConnect(id_Esp.c_str())) {
      logToConsole("Fallo WiFi. Reiniciando...");
      ESP.restart();
    }
    WiFi.setSleep(false); // <--- AGREGADO: Desactivar ahorro de energia
    logToConsole("WiFi Conectado: " + WiFi.localIP().toString());
    localIP = WiFi.localIP().toString();

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    logToConsole("Sincronizando hora NTP...");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    logToConsole("Hora sincronizada.");

    // Iniciar OTA
    setupOTA();

    xTaskCreatePinnedToCore(networkTask, "NetworkTask", 4096, NULL, 1, &networkTaskHandle, 0);

    server.on("/", handleRoot);
    server.on("/style.css", handleCss);
    server.on("/stream", handleStream); // Ruta de streaming
    server.on("/status", handleStatus); // Estado de camara
    server.on("/control", handleControl); // Control de camara
    server.on("/capture", handleCapture); // Captura a SD
    server.on("/time", handleTimeRequest);
    server.on("/save", HTTP_POST, handleSaveConfig);
    server.on("/config/get", handleGetConfig);
    server.on("/console/logs", handleConsoleLogs);
    server.on("/console/cmd", handleConsoleCommand);
    server.begin();

    logToConsole("WebServer puerto 80 activo");
}

void loop() {
    // Manejar OTA
    ArduinoOTA.handle();

    unsigned long currentMillis = millis();
    if (currentMillis - previousTimeUpdate >= timeInterval) {
      previousTimeUpdate = currentMillis;
      logToConsole("💓");
    }

    server.handleClient();
    delay(2);
}
