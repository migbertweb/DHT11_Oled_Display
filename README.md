# DHT11 Sensor with OLED Display on ESP32

Este proyecto implementa la lectura de un sensor de temperatura y humedad DHT11 y muestra los resultados en una pantalla OLED utilizando un microcontrolador ESP32. El proyecto está desarrollado utilizando el framework ESP-IDF.

## Características

- Lectura de temperatura y humedad del sensor DHT11
- Visualización en tiempo real en pantalla OLED SSD1306
- Interfaz I2C para la comunicación con la pantalla
- Fácil de configurar y personalizar
- Código documentado y modular

## Hardware Requerido

- Placa ESP32 (probado con ESP32-C3)
- Sensor DHT11
- Pantalla OLED SSD1306 (128x64 píxeles)
- Resistencias de pull-up (si son necesarias)
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

1. Clona el repositorio:
   ```bash
   git clone https://github.com/migbertweb/DHT11_Oled_Info.git
   cd DHT11_Oled_Info
   ```

2. Configura el proyecto con `menuconfig` para ajustar los pines según tu hardware:
   ```bash
   idf.py menuconfig
   ```

3. Compila y flashea el proyecto:
   ```bash
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   (Reemplaza `/dev/ttyUSB0` con el puerto serie correspondiente)

## Estructura del Proyecto

```
DHT11_Oled_Info/
├── components/     # Componentes personalizados
│   ├── dht/       # Controlador DHT11
│   └── ssd1306/   # Controlador pantalla OLED
├── main/          
│   └── main.c     # Código principal de la aplicación
├── CMakeLists.txt # Configuración del sistema de compilación
└── sdkconfig      # Configuración del proyecto
```

## Licencia

Este proyecto está bajo la Licencia MIT. Ver el archivo [LICENSE](LICENSE) para más detalles.

## Autor

Migbertweb - [GitHub](https://github.com/migbertweb)

## Agradecimientos

- A los desarrolladores de ESP-IDF
- A los contribuidores de las bibliotecas DHT y SSD1306

---

**Nota**: Este proyecto usa Licencia MIT. Se recomienda (no obliga) mantener derivados como código libre, especialmente para fines educativos.
