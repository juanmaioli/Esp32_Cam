# 📸 ESP32 Cam - WebServer & Gestión Remota

Este proyecto implementa un servidor web completo en un módulo ESP32 (compatible con ESP32-CAM), diseñado para ofrecer monitoreo de estado, transmisión de video en tiempo real, consola de depuración remota y actualizaciones OTA (Over-The-Air).

## 📋 Descripción General

El sistema levanta un servidor web que permite:
*   **Visualización de Estado:** Dashboard con Uptime, IP, MAC, memoria Heap y Hora NTP.
*   **Streaming de Video:** Transmisión MJPEG en tiempo real directamente en la interfaz web.
*   **Control de Cámara:** Ajuste dinámico de resolución (UXGA a QVGA), calidad de imagen y brillo.
*   **Consola Web:** Visualización de logs del sistema y ejecución de comandos (`ping`, `ip`, `restart`).
*   **Configuración Persistente:** Modificación de descripción y clave OTA guardados en `Preferences`.
*   **Actualizaciones OTA:** Carga de firmware de forma inalámbrica.

## 🛠️ Tecnologías y Librerías

*   **Lenguaje:** C++ (Arduino Framework)
*   **Hardware:** ESP32-CAM (AI Thinker)
*   **Librerías:** `esp_camera`, `WiFiManager`, `ArduinoOTA`, `WebServer`.

## 🚀 Instalación y Carga

1. Abre `Esp32_Cam.ino` en Arduino IDE.
2. Instala la librería `WiFiManager`.
3. Selecciona la placa **AI Thinker ESP32-CAM**.
4. Asegúrate de habilitar **PSRAM** en las opciones de la placa si tu módulo la tiene.
5. Compila y carga.

## 🔌 API de Control (Endpoints)

*   `GET /stream`: Inicia el flujo de video MJPEG.
*   `GET /status`: Devuelve la configuración actual en JSON.
*   `GET /control?var=X&val=Y`: Modifica parámetros (ej: `var=framesize&val=13`).

## 📝 Versiones y Cambios

*   **v1.6.0:** Integración completa de hardware de cámara, streaming MJPEG y panel de control dinámico.
*   **v1.1.1:** Cambio del puerto del servidor web al puerto estándar 80.
*   **v1.1.0:** Agregado de slides de navegación y visualización de versión.
*   **v1.0.0:** Versión inicial.

---
Desarrollado por **Juan Gabriel Maioli**