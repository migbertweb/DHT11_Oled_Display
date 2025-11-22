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

#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static httpd_handle_t server = NULL;
//static int hd_fd = -1; // WebSocket file descriptor

// Estructura para credenciales WiFi
typedef struct {
    char ssid[32];
    char password[64];
} app_wifi_config_t;

app_wifi_config_t wifi_creds;

#define DHT_GPIO 3      // Pin del sensor DHT11
#define tag "SSD1306"
static const char *TAG = "DHT11_ALERTA";

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t mount_spiffs(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

esp_err_t read_wifi_config(void)
{
    FILE* f = fopen("/spiffs/config.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open config.txt");
        return ESP_FAIL;
    }

    char line[128];
    // Read SSID
    if (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = 0; // Strip newline
        strncpy(wifi_creds.ssid, line, sizeof(wifi_creds.ssid) - 1);
    }
    
    // Read Password
    if (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = 0; // Strip newline
        strncpy(wifi_creds.password, line, sizeof(wifi_creds.password) - 1);
    }

    fclose(f);
    ESP_LOGI(TAG, "Read config - SSID: %s", wifi_creds.ssid);
    return ESP_OK;
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, wifi_creds.ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, wifi_creds.password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", wifi_creds.ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", wifi_creds.ssid);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

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

// Función para configurar las cabeceras CORS (no usada actualmente, pero puede ser útil)
// static esp_err_t set_cors_headers(httpd_req_t *req) {
//     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
//     httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
//     httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
//     return ESP_OK;
// }

static esp_err_t ws_handler(httpd_req_t *req)
{
    // Con is_websocket = true, ESP-IDF maneja el handshake automáticamente
    // El handler se llama después del handshake para manejar frames WebSocket
    
    httpd_ws_frame_t ws_pkt = {0};
    uint8_t *buf = NULL;
    
    // Obtener la longitud del frame
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        // Si es una solicitud GET inicial (handshake), esto es normal
        if (req->method == HTTP_GET) {
            ESP_LOGI(TAG, "WebSocket handshake en proceso...");
            // El handshake se completa automáticamente con is_websocket = true
            return ESP_OK;
        }
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Si hay datos, leerlos
    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
            free(buf);
            return ret;
        }
        
        ESP_LOGI(TAG, "Mensaje recibido: %.*s", ws_pkt.len, ws_pkt.payload);
        free(buf);
    } else {
        // Frame vacío (ping/pong o handshake completado)
        if (req->method == HTTP_GET) {
            ESP_LOGI(TAG, "WebSocket handshake completado, cliente conectado (fd: %d)", httpd_req_to_sockfd(req));
        } else {
            ESP_LOGD(TAG, "Frame vacío recibido");
        }
    }
    
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    FILE* f = fopen("/spiffs/index.html", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open index.html");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t style_handler(httpd_req_t *req)
{
    FILE* f = fopen("/spiffs/style.css", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open style.css");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/css");
    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t js_handler(httpd_req_t *req)
{
    FILE* f = fopen("/spiffs/main.js", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open main.js");
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/javascript");
    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// static const httpd_uri_t ws = {
//         .uri        = "/ws",
//         .method     = HTTP_GET,
//         .handler    = ws_handler,
//         .user_ctx   = NULL
// };

// static const httpd_uri_t index_uri = {
//     .uri       = "/",
//     .method    = HTTP_GET,
//     .handler   = index_handler,
//     .user_ctx  = NULL
// };

// static const httpd_uri_t style_uri = {
//     .uri       = "/style.css",
//     .method    = HTTP_GET,
//     .handler   = style_handler,
//     .user_ctx  = NULL
// };

// static const httpd_uri_t js_uri = {
//     .uri       = "/main.js",
//     .method    = HTTP_GET,
//     .handler   = js_handler,
//     .user_ctx  = NULL
// };

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.lru_purge_enable = true;  // Importante para limpiar conexiones inactivas

    ESP_LOGI(TAG, "Iniciando servidor web en el puerto: %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Configurar manejador WebSocket
        httpd_uri_t ws = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = ws_handler,
            .user_ctx = NULL,
            .is_websocket = true  // IMPORTANTE: Indicar que es un handler WebSocket
        };
        httpd_register_uri_handler(server, &ws);

        // Configurar manejador para la página principal
        httpd_uri_t index = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = index_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index);

        // Configurar manejador para el archivo CSS
        httpd_uri_t css = {
            .uri = "/style.css",
            .method = HTTP_GET,
            .handler = style_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &css);

        // Configurar manejador para el archivo JavaScript
        httpd_uri_t js = {
            .uri = "/main.js",
            .method = HTTP_GET,
            .handler = js_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &js);

        return server;
    }

    ESP_LOGE(TAG, "Error al iniciar el servidor web");
    return NULL;
}

void send_ws_message(char *msg)
{
    if (!server) {
        ESP_LOGW(TAG, "Servidor no inicializado, no se puede enviar mensaje WebSocket");
        return;
    }
    
    size_t fds = 10;
    int client_fds[10];
    
    esp_err_t ret = httpd_get_client_list(server, &fds, client_fds);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No se pudieron obtener clientes: %s", esp_err_to_name(ret));
        return;
    }

    if (fds == 0) {
        ESP_LOGD(TAG, "No hay clientes conectados");
        return;
    }

    httpd_ws_frame_t ws_pkt = {
        .payload = (uint8_t *)msg,
        .len = strlen(msg),
        .type = HTTPD_WS_TYPE_TEXT
    };

    for (int i = 0; i < fds; i++) {
        ret = httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Error enviando a cliente %d: %s", client_fds[i], esp_err_to_name(ret));
        } else {
            ESP_LOGD(TAG, "Mensaje enviado a cliente %d: %s", client_fds[i], msg);
        }
    }
}

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

            // Enviar por WebSocket
            char json_msg[64];
            snprintf(json_msg, sizeof(json_msg), "{\"temp\": %.1f, \"hum\": %.1f}", temp_c, hum_p);
            send_ws_message(json_msg);
            
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
    // Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar SPIFFS
    if (mount_spiffs() != ESP_OK) {
        ESP_LOGE(TAG, "Error montando SPIFFS, deteniendo...");
        return;
    }

    // Leer configuración WiFi
    if (read_wifi_config() != ESP_OK) {
        ESP_LOGE(TAG, "Error leyendo config WiFi, usando valores por defecto o deteniendo...");
        // Podríamos detenernos aquí o intentar conectar con credenciales hardcodeadas si se desea
    }

    // Conectar a WiFi
    wifi_init_sta();

    // Iniciar el servidor web
    server = start_webserver();
    if (server == NULL) {
        ESP_LOGE(TAG, "Error al iniciar el servidor web");
        return;
    }

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