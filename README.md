# DHT11 Sensor with OLED Display & WebSocket Monitor on ESP32

Este proyecto implementa un monitor de temperatura y humedad utilizando un sensor DHT11, una pantalla OLED SSD1306 y un servidor web con WebSocket para visualización en tiempo real.

## Características

- **Lectura de Sensor**: Temperatura y humedad del sensor DHT11.
- **Pantalla OLED**: Visualización local en tiempo real.
- **Monitor Web**: Interfaz web moderna alojada en el ESP32 (SPIFFS).
- **WebSocket**: Actualización de datos en tiempo real en el navegador.
- **Configuración WiFi**: Credenciales leídas desde archivo `config.txt` en SPIFFS.

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

1. Al arrancar, el ESP32 intentará conectarse a la red WiFi configurada.
2. La dirección IP asignada se mostrará en el log del monitor serial (`idf.py monitor`).
3. Abre esa dirección IP en un navegador web (PC o Móvil).
4. Verás los datos del sensor actualizándose en tiempo real.

## Estructura del Proyecto

```
DHT11_Oled_Info/
├── components/     # Componentes (dht, ssd1306)
├── main/          
│   └── main.c      # Código principal (App, WiFi, WebServer)
├── storage/        # Archivos Web y Configuración (SPIFFS)
│   ├── index.html
│   ├── style.css
│   ├── main.js
│   └── config.txt
├── CMakeLists.txt
└── partitions.csv  # Tabla de particiones personalizada
```

## Licencia

Este proyecto está bajo la Licencia MIT. Ver el archivo [LICENSE](LICENSE) para más detalles.

## Autor

Migbertweb - [GitHub](https://github.com/migbertweb)

---

**Nota**: Este proyecto usa Licencia MIT. Se recomienda (no obliga) mantener derivados como código libre, especialmente para fines educativos.
