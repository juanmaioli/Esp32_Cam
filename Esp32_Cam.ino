// ESP32 Cam v1.0.0
// Author Juan Maioli
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <time.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

// --- Variables Globales ---
const String firmware_version = "1.1.0";
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
WebServer server(3000);

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

    // --- Slide 2: Cámara (Nuevo) ---
    chunk = "<div class='carousel-slide fade'><h2>Cámara</h2><div class='emoji-container'><span class='emoji'>📷</span></div><br>";
    chunk += "<p style='text-align:center;'>Vista previa no disponible</p></div>";
    server.sendContent(chunk);

    // --- Slide 3: Config. de Cam. (Nuevo) ---
    chunk = "<div class='carousel-slide fade'><h2>Config. de Cam.</h2><div class='emoji-container'><span class='emoji'>🎛️</span></div><br>";
    chunk += "<p style='text-align:center;'>Controles no disponibles</p></div>";
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
    server.on("/time", handleTimeRequest);
    server.on("/save", HTTP_POST, handleSaveConfig);
    server.on("/config/get", handleGetConfig);
    server.on("/console/logs", handleConsoleLogs);
    server.on("/console/cmd", handleConsoleCommand);
    server.begin();

    logToConsole("WebServer puerto 3000 activo");
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
