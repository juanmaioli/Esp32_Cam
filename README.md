# 📸 ESP32 Cam - WebServer & Gestión Remota

Este proyecto implementa un servidor web completo en un módulo ESP32 (compatible con ESP32-CAM), diseñado para ofrecer monitoreo de estado, consola de depuración remota y actualizaciones OTA (Over-The-Air).

## 📋 Descripción General

El sistema levanta un punto de acceso y servidor web que permite:
*   **Visualización de Estado:** Dashboard con información en tiempo real (Uptime, IP, MAC, Heap, Hora NTP).
*   **Cámara y Configuración:** Slides dedicadas para la visualización y control (en desarrollo).
*   **Consola Web:** Visualización de logs del sistema y envío de comandos básicos (`ping`, `ip`, `restart`).
*   **Configuración Persistente:** Modificación de parámetros guardados en memoria no volátil (Preferences).
*   **Actualizaciones OTA:** Carga de nuevo firmware de forma inalámbrica.

## 🛠️ Tecnologías y Librerías

*   **Lenguaje:** C++ (Arduino Framework)
*   **Placa:** ESP32 / ESP32-CAM
*   **Librerías Clave:**
    *   `WiFiManager`
    *   `ArduinoOTA`
    *   `WebServer`

## 🚀 Instalación y Carga

1. Abre `Esp32_Cam.ino` en Arduino IDE.
2. Instala la librería `WiFiManager`.
3. Selecciona la placa `AI Thinker ESP32-CAM`.
4. Compila y carga.

## 📝 Versiones y Cambios

*   **v1.1.0:** Se agregaron slides de Cámara y Configuración de Cámara. Se integró la visualización de la versión del firmware en el dashboard.
*   **v1.0.0:** Versión inicial con WebServer, Consola y OTA.

---
Desarrollado por **Juan Gabriel Maioli**
