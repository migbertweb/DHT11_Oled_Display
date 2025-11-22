/**
 * Archivo: main.c
 * Descripción: Aplicación de ejemplo para el control de pantallas OLED SSD1306 con ESP32.
 *              Este archivo demuestra la inicialización y uso básico de la biblioteca SSD1306
 *              con soporte para interfaces I2C y SPI.
 * Autor: migbertweb
 * Fecha: 21/11/2025
 * Repositorio: https://github.com/migbertweb/DHT11_Oled_Info
 * Licencia: MIT License
 * 
 * Uso: Este archivo contiene el punto de entrada de la aplicación y demuestra cómo:
 *      1. Inicializar la pantalla SSD1306 mediante I2C o SPI
 *      2. Mostrar texto y gráficos básicos
 *      3. Utilizar las funciones de la biblioteca SSD1306
 *
 * Nota: Este proyecto usa Licencia MIT. Se recomienda (no obliga) mantener 
 * derivados como código libre, especialmente para fines educativos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <dht.h>
#include "ssd1306.h"
//#include "font8x8_basic.h"

/*
 You have to set this config value with menuconfig
 CONFIG_INTERFACE

 for i2c
 CONFIG_MODEL
 CONFIG_SDA_GPIO
 CONFIG_SCL_GPIO
 CONFIG_RESET_GPIO

 for SPI
 CONFIG_CS_GPIO
 CONFIG_DC_GPIO
 CONFIG_RESET_GPIO
*/

#define DHT_GPIO 3      // Pin del sensor DHT11
#define tag "SSD1306"
static const char *TAG = "DHT11_ALERTA";

// Variable global para la estructura del display
SSD1306_t oled_dev;

void dht11_task(void *pvParameters)
{
    char lineChar[20];
    
    // Configurar hardware
    ESP_LOGI(TAG, "Iniciando monitor DHT11 en GPIO %d", DHT_GPIO);
    
    // Inicializar pantalla
  ssd1306_clear_screen(&oled_dev, false);
   ssd1306_contrast(&oled_dev, 0xff);
    
    // Mostrar título grande
    ssd1306_display_text(&oled_dev, 0, "DHT11", 5, false);
    
    // Mostrar encabezado fijo
    char header_line[20];
    snprintf(header_line, sizeof(header_line), "Placa: ESP32-C3");
    ssd1306_display_text(&oled_dev, 2, header_line, strlen(header_line), false);
    
    snprintf(header_line, sizeof(header_line), "Sensor GPIO: %d", DHT_GPIO);
    ssd1306_display_text(&oled_dev, 3, header_line, strlen(header_line), false);
	// Línea separadora
    ssd1306_display_text(&oled_dev, 4, "----------------", 16, false);
    // Línea separadora
    ssd1306_display_text(&oled_dev, 7, "----------------", 16, false);
    
    while (1) {
        int16_t temperature, humidity;
        esp_err_t result = dht_read_data(DHT_TYPE_DHT11, (gpio_num_t)DHT_GPIO, &humidity, &temperature);
        
        if (result == ESP_OK) {
            float temp_c = temperature / 10.0;
            float hum_p = humidity / 10.0;
            
            // Mostrar lectura en consola
            ESP_LOGI(TAG, "Temperatura: %.1f°C, Humedad: %.1f%%", temp_c, hum_p);
            
            // Mostrar en pantalla OLED
            snprintf(lineChar, sizeof(lineChar), "Temp.: %.1f C", temp_c);
            ssd1306_display_text(&oled_dev, 5, lineChar, strlen(lineChar), false);
            
            snprintf(lineChar, sizeof(lineChar), "Hum.: %.1f %%", hum_p);
            ssd1306_display_text(&oled_dev, 6, lineChar, strlen(lineChar), false);
            
        } else {
            ESP_LOGE(TAG, "Error lectura: %s", esp_err_to_name(result));
            ssd1306_display_text(&oled_dev, 5, "Error lectura", 13, false);
            ssd1306_display_text(&oled_dev, 6, "Revisa conexiones", 17, false);
        }
        
        vTaskDelay(5000 / portTICK_PERIOD_MS); // Lectura cada 5 segundos
    }
}


void app_main(void)
{

#if CONFIG_I2C_INTERFACE
	ESP_LOGI(tag, "INTERFACE is i2c");
	ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d",CONFIG_SDA_GPIO);
	ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d",CONFIG_SCL_GPIO);
	ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d",CONFIG_RESET_GPIO);
	i2c_master_init(&oled_dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_I2C_INTERFACE

#if CONFIG_FLIP
	oled_dev._flip = true;
	ESP_LOGW(tag, "Flip upside down");
#endif

#if CONFIG_SSD1306_128x64
	ESP_LOGI(tag, "Panel is 128x64");
	ssd1306_init(&oled_dev, 128, 64);
#endif // CONFIG_SSD1306_128x64

	// Mostrar mensaje de inicio
	ssd1306_clear_screen(&oled_dev, false);
	ssd1306_contrast(&oled_dev, 0xff);
	ssd1306_display_text_x3(&oled_dev, 0, "DHT11", 5, false);
	ssd1306_display_text(&oled_dev, 4, "Iniciando...", 12, false);
	vTaskDelay(3000 / portTICK_PERIOD_MS);
	ssd1306_clear_screen(&oled_dev, false);

	printf("=== MONITOR DHT11 ===\n");
	printf("Placa: ESP32-C3 SuperMini\n");
	printf("Sensor DHT11 en GPIO: %d\n", DHT_GPIO);
	printf("------------------------------------\n");
	
	// Crear tarea principal
	xTaskCreate(dht11_task, "dht11_task", 4096, NULL, 5, NULL);
	
	ESP_LOGI(TAG, "Sistema iniciado - Esperando lecturas...");
}