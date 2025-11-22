# DHT11 Sensor with OLED Display & WebSocket Monitor on ESP32

Este proyecto implementa un sistema completo de monitoreo de temperatura y humedad utilizando un sensor DHT11, una pantalla OLED SSD1306 y un servidor web con WebSocket para visualización en tiempo real desde cualquier dispositivo conectado a la red.

## Características

- **Lectura de Sensor**: Temperatura y humedad del sensor DHT11 con actualización cada 5 segundos.
- **Pantalla OLED**: Visualización local en tiempo real en pantalla SSD1306 (128x64, I2C).
- **Monitor Web**: Interfaz web moderna y responsive alojada en el ESP32 (SPIFFS).
- **WebSocket**: Comunicación bidireccional para actualización de datos en tiempo real en el navegador.
- **Configuración WiFi**: Credenciales leídas desde archivo `config.txt` en SPIFFS (sin hardcodear).
- **Servidor HTTP**: Servidor web completo con soporte para archivos estáticos (HTML, CSS, JS).
- **Sistema de archivos**: Uso de SPIFFS para almacenar archivos web y configuración.

## Hardware Requerido

- Placa ESP32 (probado con ESP32-C3)
- Sensor DHT11
- Pantalla OLED SSD1306 (128x64 píxeles, I2C)
- Cables de conexión

## Conexiones

### DHT11
- VCC → 3.3V
- DATA → GPIO3
- GND → GND

### Pantalla OLED SSD1306 (I2C)
- VCC → 3.3V
- GND → GND
- SCL → GPIO8 (configurable)
- SDA → GPIO10 (configurable)

## Configuración

### 1. Clonar el repositorio
```bash
git clone https://github.com/migbertweb/DHT11_Oled_Info.git
cd DHT11_Oled_Info
```

### 2. Configuración WiFi
Edita el archivo `storage/config.txt` con tus credenciales WiFi:
```text
TU_SSID
TU_PASSWORD
```
Este archivo se subirá a la partición SPIFFS del ESP32.

### 3. Configuración del Hardware
Ejecuta `idf.py menuconfig` para ajustar los pines si es necesario.

## Compilación y Flasheo

1. **Compilar el proyecto**:
   ```bash
   idf.py build
   ```

2. **Flashear (incluyendo SPIFFS)**:
   El sistema de construcción generará automáticamente la imagen de la partición `storage` con los archivos de la carpeta `storage/`.
   ```bash
   idf.py flash monitor
   ```

## Uso

1. **Configurar WiFi**: Edita `storage/config.txt` con tu SSID y contraseña WiFi.
2. **Compilar y flashear**: Ejecuta `idf.py build flash` para compilar y cargar el firmware.
3. **Monitorear**: Conecta el monitor serial (`idf.py monitor`) para ver los logs del sistema.
4. **Conectar**: El ESP32 intentará conectarse automáticamente a la red WiFi configurada.
5. **Acceder**: Una vez conectado, la dirección IP se mostrará en los logs. Ábrela en cualquier navegador.
6. **Visualizar**: La interfaz web mostrará los datos del sensor actualizándose en tiempo real mediante WebSocket.

### Características de la interfaz web

- **Estado de conexión**: Indicador visual del estado de la conexión WebSocket.
- **Actualización automática**: Los datos se actualizan cada 5 segundos sin recargar la página.
- **Diseño responsive**: Funciona correctamente en dispositivos móviles y de escritorio.
- **Reconexión automática**: Si se pierde la conexión, el cliente intenta reconectarse automáticamente.

## Estructura del Proyecto

```
DHT11_Oled_Info/
├── components/           # Componentes externos
│   ├── esp-idf-lib__dht/ # Biblioteca para sensor DHT11
│   └── ssd1306/         # Biblioteca para pantalla OLED SSD1306
├── main/                 # Código principal de la aplicación
│   └── main.c           # App principal (WiFi, WebServer, WebSocket, DHT11)
├── storage/             # Archivos Web y Configuración (SPIFFS)
│   ├── index.html       # Página principal de la interfaz web
│   ├── style.css        # Estilos CSS para la interfaz
│   ├── main.js          # JavaScript para WebSocket y actualización de datos
│   └── config.txt       # Configuración WiFi (SSID y contraseña)
├── build/               # Archivos de compilación (generado)
├── CMakeLists.txt       # Configuración de CMake
├── partitions.csv       # Tabla de particiones personalizada (incluye SPIFFS)
├── sdkconfig            # Configuración del proyecto ESP-IDF
└── README.md            # Este archivo
```

## Arquitectura del Sistema

### Componentes principales

1. **Sensor DHT11**: Lectura de temperatura y humedad cada 5 segundos.
2. **Pantalla OLED**: Muestra información del sistema y lecturas del sensor.
3. **Servidor HTTP**: Sirve archivos estáticos y maneja conexiones WebSocket.
4. **WebSocket**: Comunicación en tiempo real entre ESP32 y clientes web.
5. **SPIFFS**: Sistema de archivos para almacenar configuración y archivos web.

### Flujo de datos

```
DHT11 → ESP32 → [OLED Display] + [WebSocket] → Navegador Web
                ↓
            Logs Serial
```

## Licencia

Este proyecto está bajo la Licencia MIT. Ver el archivo [LICENSE](LICENSE) para más detalles.

## Autor

Migbertweb - [GitHub](https://github.com/migbertweb)

---

**Nota**: Este proyecto usa Licencia MIT. Se recomienda (no obliga) mantener derivados como código libre, especialmente para fines educativos.
