# 📸 ESP32 Cam - WebServer & Gestión Remota

Este proyecto implementa un servidor web completo en un módulo ESP32 (compatible con ESP32-CAM), diseñado para ofrecer monitoreo de estado, transmisión de video en tiempo real, captura de fotos con descarga directa y actualizaciones OTA.

## 📋 Descripción General

El sistema permite:
*   **Visualización de Estado:** Dashboard con Uptime, IP, MAC, memoria Heap y Hora NTP.
*   **Streaming de Video:** Transmisión MJPEG optimizada para estabilidad (WiFi Sleep OFF).
*   **Control de Cámara:** Ajuste dinámico de resolución, calidad y brillo.
*   **Linterna (Flash):** Control de intensidad del LED integrado (GPIO 4) mediante PWM (API v3.x).
*   **Captura con Descarga:** Botón para tomar fotos y descargarlas instantáneamente al dispositivo cliente (sin necesidad de SD).
*   **Consola Web:** Logs en tiempo real y ejecución de comandos remotos.
*   **Actualizaciones OTA:** Carga de firmware inalámbrica protegida por contraseña.

## 🛠️ Tecnologías y Librerías

*   **Lenguaje:** C++ (Arduino Framework)
*   **Hardware:** ESP32-CAM (AI Thinker)
*   **Librerías:** `esp_camera`, `WiFiManager`, `ArduinoOTA`, `WebServer`.

## 🚀 Instalación y Carga

1. Abre `Esp32_Cam.ino` en Arduino IDE.
2. Selecciona la placa **AI Thinker ESP32-CAM**.
3. **IMPORTANTE:** En el menú de la placa, habilita **PSRAM: Enabled**.
4. Compila y carga.

## 🔌 API de Control

*   `GET /stream`: Video en tiempo real.
*   `GET /capture`: Captura un frame y lo descarga automáticamente al navegador.
*   `GET /control?var=flash&val=[0-255]`: Controla la linterna.
*   `GET /status`: Obtiene el estado actual de la cámara en JSON.

## 📝 Versiones y Cambios

*   **v1.9.0:** Implementación de Captura con Descarga Directa (se remueve dependencia de SD para mayor estabilidad).
*   **v1.8.0:** Intento de integración de Galería Web (removido por inestabilidad de hardware SD).
*   **v1.7.0:** Implementación de Linterna (PWM) y mejoras de estabilidad del stream.
*   **v1.6.0:** Integración inicial de streaming y controles de cámara.
*   **v1.1.1:** Cambio a puerto estándar 80.
*   **v1.0.0:** Versión inicial.

---
Desarrollado por **Juan Gabriel Maioli**
