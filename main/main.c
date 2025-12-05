/* Archivo: main.c
 * Descripción: Sistema de monitoreo de temperatura y humedad con ESP32.
 *              Aplicación completa que integra sensor DHT11, pantalla OLED
 * SSD1306, servidor web HTTP, comunicación WebSocket, MQTT y Bot de Telegram
 * para visualización, monitoreo y control remoto en tiempo real.
 *
 * Funcionalidades principales:
 *  - Lectura de temperatura y humedad del sensor DHT11
 *  - Visualización local en pantalla OLED SSD1306 (I2C)
 *  - Control de relé basado en umbral de temperatura con indicador LED
 *  - Servidor web HTTP con archivos estáticos desde SPIFFS
 *  - Comunicación WebSocket para actualización en tiempo real
 *  - Publicación de datos a servidor MQTT
 *  - Integración con Bot de Telegram para alertas y comandos (/status, /relay)
 *  - Configuración WiFi mediante archivo de configuración en SPIFFS
 *  - Interfaz web moderna y responsive
 *  - Registro y visualización de valores Mínimos y Máximos
 *  - Manejo de eventos de conexión/desconexión WiFi
 *
 * Autor: migbertweb
 * Fecha: 04/12/2025
 * Repositorio: https://github.com/migbertweb/DHT11_Oled_Info
 * Licencia: MIT License
 *
 * Estructura del código:
 *  - Inicialización: NVS, SPIFFS, WiFi, servidor HTTP, MQTT, Telegram
 *  - Handlers HTTP: Página principal, CSS, JavaScript, WebSocket
 *  - Tarea DHT11: Lectura periódica del sensor y actualización de displays
 *  - WebSocket: Envío de datos en tiempo real a clientes conectados
 *  - MQTT: Publicación de datos a broker MQTT
 *  - Telegram: Envío de alertas y manejo de comandos
 *  - Control de relé: Activa/desactiva salida e indicador LED según temperatura
 *
 * Funciones principales:
 *  - dht11_task: Tarea para lectura periódica de sensores
 *  - telegram_bot_task: Tarea para manejar actualizaciones de Telegram
 *  - mqtt_event_handler: Manejo de eventos MQTT
 *  - event_handler: Manejo de eventos WiFi e IP
 *  - mount_spiffs: Montaje del sistema de archivos SPIFFS
 *  - read_wifi_config: Lectura de credenciales WiFi
 *  - wifi_init_sta: Inicialización de conexión WiFi
 *  - start_webserver: Configuración e inicio del servidor web
 *  - send_ws_message: Envío de mensajes a clientes WebSocket
 *  - init_relay: Inicialización de pines de control (relé y LED)
 *  - blink_led_task: Tarea para parpadeo de LED indicador
 *  - display_centered_text: Utilidad para mostrar texto centrado en OLED
 *
 * Nota: Este proyecto usa Licencia MIT. Se recomienda (no obliga) mantener
 * derivados como código libre, especialmente para fines educativos.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dht.h"
#include "ssd1306.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "esp_tls.h"
#include "esp_timer.h"

// Variable global para almacenar la dirección IP
char ip_address[16] = "Conectando...";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// Configuración MQTT
#define BROKER_URI "mqtt://37.27.243.58:1883"
#define MQTT_TOPIC "sensores/dht11"
#define MQTT_USER "piro"         // REEMPLAZAR con tu usuario
#define MQTT_PASSWORD "gpiro2178" // REEMPLAZAR con tu contraseña

// Telegram Configuration
#define TELEGRAM_TOKEN "8283534449:AAHSVCJ_69nlvs82i0pJQMxTunJfTy_mxv4"
#define TELEGRAM_CHAT_ID "10165249"
#define TELEGRAM_API_URL "https://api.telegram.org/bot"

static int64_t last_alert_time = 0;
#define ALERT_COOLDOWN_MS 60000 // 1 minute cooldown

// Variable global para el cliente MQTT
static esp_mqtt_client_handle_t mqtt_client;

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static httpd_handle_t server = NULL;
// static int hd_fd = -1; // WebSocket file descriptor

// Variables para seguimiento de Min/Max
static float min_temp = 100.0;
static float max_temp = -100.0;
static float min_hum = 100.0;
static float max_hum = 0.0;
static float current_temp = 0.0;
static float current_hum = 0.0;

// Estructura para credenciales WiFi
typedef struct {
  char ssid[32];
  char password[64];
} app_wifi_config_t;

app_wifi_config_t wifi_creds;

#define DHT_GPIO 4 // Pin del sensor DHT11
#define tag "SSD1306"
static const char *TAG = "DHT11_ALERTA";

/**
 * @brief Manejador de eventos de WiFi e IP
 *
 * Maneja varios eventos relacionados con WiFi e IP como conexión, desconexión
 * y asignación de dirección IP.
 *
 * @param arg Datos de usuario (no utilizado)
 * @param event_base Base del evento (WIFI_EVENT o IP_EVENT)
 * @param event_id ID específico del evento
 * @param event_data Datos específicos del evento
 */
/**
 * @brief Maneja los eventos del cliente MQTT
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;

  switch (event->event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "Conectado al servidor MQTT");
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGW(TAG, "Desconectado del servidor MQTT");
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGD(TAG, "Mensaje publicado en MQTT");
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGE(TAG, "Error en MQTT");
    break;
  default:
    break;
  }
}

/**
 * @brief Inicializa el cliente MQTT
 */
static void mqtt_app_start(void) {
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker.address.uri = BROKER_URI,
      .credentials.username = MQTT_USER,
      .credentials.authentication.password = MQTT_PASSWORD,
  };

  mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
  esp_mqtt_client_start(mqtt_client);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 10) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    // Formatear la dirección IP
    snprintf(ip_address, sizeof(ip_address), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "got ip: %s", ip_address);
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

/**
 * @brief Monta el sistema de archivos SPIFFS
 *
 * Inicializa y monta la partición SPIFFS (Sistema de Archivos en Memoria Flash
 * SPI). El sistema de archivos se formateará si falla el montaje.
 *
 * @return esp_err_t
 *         - ESP_OK si tiene éxito
 *         - ESP_FAIL si falla el montaje o formateo
 *         - ESP_ERR_NOT_FOUND si no se encuentra la partición
 *         - Otros códigos de error de las funciones esp_spiffs_*
 *
 * @related_header
 * - esp_spiffs.h
 * - esp_vfs.h
 */
esp_err_t mount_spiffs(void) {
  ESP_LOGI(TAG, "Initializing SPIFFS");

  esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs",
                                .partition_label = "storage",
                                .max_files = 5,
                                .format_if_mount_failed = true};

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
    ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
  }
  return ESP_OK;
}

/**
 * @brief Lee la configuración WiFi desde SPIFFS
 *
 * Lee el SSID y contraseña del archivo /spiffs/config.txt.
 * La primera línea debe contener el SSID y la segunda la contraseña.
 *
 * @return esp_err_t
 *         - ESP_OK si tiene éxito
 *         - ESP_FAIL si no se puede abrir o leer el archivo
 *
 * @note El archivo de configuración debe tener el formato:
 *       SSID\n
 *       CONTRASEÑA\n
 *
 * @related_header
 * - stdio.h (para operaciones de archivo)
 * - string.h (para operaciones de cadenas)
 */
esp_err_t read_wifi_config(void) {
  FILE *f = fopen("/spiffs/config.txt", "r");
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

/**
 * @brief Inicializa WiFi en modo estación
 *
 * Configura e inicia la interfaz WiFi en modo estación usando credenciales
 * cargadas desde el archivo de configuración. Configura manejadores de eventos
 * para el estado de conexión y asignación de dirección IP.
 *
 * @note Esta función se bloquea hasta que se establece la conexión o falla
 *
 * @related_header
 * - esp_wifi.h
 * - esp_event.h
 * - esp_netif.h
 */
void wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta =
          {
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };

  strncpy((char *)wifi_config.sta.ssid, wifi_creds.ssid,
          sizeof(wifi_config.sta.ssid));
  strncpy((char *)wifi_config.sta.password, wifi_creds.password,
          sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

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

// Función para configurar las cabeceras CORS (no usada actualmente, pero puede
// ser útil) static esp_err_t set_cors_headers(httpd_req_t *req) {
//     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
//     httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST,
//     OPTIONS"); httpd_resp_set_hdr(req, "Access-Control-Allow-Headers",
//     "Content-Type"); return ESP_OK;
// }

static esp_err_t ws_handler(httpd_req_t *req) {
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
      ESP_LOGI(TAG,
               "WebSocket handshake completado, cliente conectado (fd: %d)",
               httpd_req_to_sockfd(req));
    } else {
      ESP_LOGD(TAG, "Frame vacío recibido");
    }
  }

  return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req) {
  FILE *f = fopen("/spiffs/index.html", "r");
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

static esp_err_t style_handler(httpd_req_t *req) {
  FILE *f = fopen("/spiffs/style.css", "r");
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

static esp_err_t js_handler(httpd_req_t *req) {
  FILE *f = fopen("/spiffs/main.js", "r");
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

/**
 * @brief Inicia el servidor HTTP con soporte WebSocket
 *
 * Inicializa y configura el servidor HTTP con los siguientes endpoints:
 * - GET / : Sirve la página HTML principal
 * - GET /style.css : Sirve la hoja de estilos CSS
 * - GET /script.js : Sirve el archivo JavaScript
 * - GET /ws : Endpoint WebSocket para actualizaciones en tiempo real
 *
 * @return httpd_handle_t Manejador del servidor HTTP iniciado
 *
 * @related_header
 * - esp_http_server.h
 * - http_server.h
 */
static httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;
  config.lru_purge_enable =
      true; // Importante para limpiar conexiones inactivas

  ESP_LOGI(TAG, "Iniciando servidor web en el puerto: %d", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Configurar manejador WebSocket
    httpd_uri_t ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true // IMPORTANTE: Indicar que es un handler WebSocket
    };
    httpd_register_uri_handler(server, &ws);

    // Configurar manejador para la página principal
    httpd_uri_t index = {.uri = "/",
                         .method = HTTP_GET,
                         .handler = index_handler,
                         .user_ctx = NULL};
    httpd_register_uri_handler(server, &index);

    // Configurar manejador para el archivo CSS
    httpd_uri_t css = {.uri = "/style.css",
                       .method = HTTP_GET,
                       .handler = style_handler,
                       .user_ctx = NULL};
    httpd_register_uri_handler(server, &css);

    // Configurar manejador para el archivo JavaScript
    httpd_uri_t js = {.uri = "/main.js",
                      .method = HTTP_GET,
                      .handler = js_handler,
                      .user_ctx = NULL};
    httpd_register_uri_handler(server, &js);

    return server;
  }

  ESP_LOGE(TAG, "Error al iniciar el servidor web");
  return NULL;
}

/**
 * @brief Envía un mensaje a todos los clientes WebSocket conectados
 *
 * @param msg Cadena terminada en nulo que contiene el mensaje a enviar
 *
 * @note El mensaje se enviará a todos los clientes WebSocket actualmente
 * conectados
 *
 * @related_header
 * - esp_http_server.h
 */
void send_ws_message(char *msg) {
  if (!server) {
    ESP_LOGW(TAG,
             "Servidor no inicializado, no se puede enviar mensaje WebSocket");
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

  httpd_ws_frame_t ws_pkt = {.payload = (uint8_t *)msg,
                             .len = strlen(msg),
                             .type = HTTPD_WS_TYPE_TEXT};

  for (int i = 0; i < fds; i++) {
    ret = httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Error enviando a cliente %d: %s", client_fds[i],
               esp_err_to_name(ret));
    } else {
      ESP_LOGD(TAG, "Mensaje enviado a cliente %d: %s", client_fds[i], msg);
    }
  }
}

// Definiciones para el control del relé
#define RELAY_GPIO 1        // Pin GPIO para el relé
#define TEMP_THRESHOLD 30.0 // Umbral de temperatura en grados Celsius
#define LED_GPIO 21         // Pin GPIO para el LED indicador

static TaskHandle_t blink_task_handle = NULL;

/**
 * @brief Tarea para parpadear el LED indicador
 * 
 * Esta tarea se activa cuando el relé está encendido para proporcionar
 * una indicación visual intermitente.
 * 
 * @param pvParameters Parámetros de la tarea (no utilizado)
 */
void blink_led_task(void *pvParameters) {
  while (1) {
    gpio_set_level(LED_GPIO, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(LED_GPIO, 0);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// Variable global para la estructura del display
SSD1306_t oled_dev;

/**
 * @brief Inicializa los pines de control (Relé y LED)
 * 
 * Configura el GPIO del relé como entrada/salida y el GPIO del LED como salida.
 * Inicializa ambos en estado bajo (apagado).
 */
static void init_relay(void) {
  gpio_reset_pin(RELAY_GPIO);
  gpio_set_direction(RELAY_GPIO, GPIO_MODE_INPUT_OUTPUT);
  gpio_set_level(RELAY_GPIO, 0); // Inicia con el relé apagado
  
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(LED_GPIO, 0); // Inicia con el LED apagado

  ESP_LOGI(TAG, "Relé inicializado en GPIO %d, LED en GPIO %d", RELAY_GPIO, LED_GPIO);
}

/**
 * @brief Centra un texto en la pantalla OLED
 *
 * @param text Texto a centrar
 * @param line Línea donde se mostrará el texto (0-7)
 * @param clear_line Si es true, limpia la línea antes de escribir
 */
void display_centered_text(const char *text, int line, bool clear_line) {
  if (clear_line) {
    // Limpiar la línea con espacios (16 caracteres)
    char empty_line[17] = "                "; // 16 espacios
    ssd1306_display_text(&oled_dev, line, empty_line, 16, false);
  }

  // Calcular la posición de inicio para centrar el texto
  int text_len = strlen(text);
  if (text_len > 16)
    text_len = 16;
  int start_pos = (16 - text_len) / 2;

  // Crear un buffer con espacios a la izquierda y el texto
  char buffer[17] = "                "; // 16 espacios + terminador nulo
  strncpy(buffer + start_pos, text, text_len);

  // Mostrar el texto centrado
  ssd1306_display_text(&oled_dev, line, buffer, 16, false);
}

/**
 * @brief Envía un mensaje a través del Bot de Telegram
 * 
 * Realiza una petición HTTPS POST a la API de Telegram para enviar un mensaje
 * al chat ID configurado.
 * 
 * @param message Mensaje de texto a enviar
 */
void send_telegram_message(const char *message) {
    char url[512];
    
    snprintf(url, sizeof(url), "%s%s/sendMessage", TELEGRAM_API_URL, TELEGRAM_TOKEN);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return;
    }

    // Create JSON payload
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "chat_id", TELEGRAM_CHAT_ID);
    cJSON_AddStringToObject(root, "text", message);
    char *post_data = cJSON_PrintUnformatted(root);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Telegram message sent: %s", message);
    } else {
        ESP_LOGE(TAG, "Failed to send Telegram message: %s", esp_err_to_name(err));
    }

    cJSON_Delete(root);
    free(post_data);
    esp_http_client_cleanup(client);
}

/**
 * @brief Maneja las actualizaciones (comandos) de Telegram
 * 
 * Consulta la API de Telegram mediante long-polling para recibir nuevos mensajes.
 * Procesa comandos como:
 * - /status: Envía el estado actual de temperatura, humedad y relé.
 * - /relay: Envía el estado actual del relé.
 */
void handle_telegram_updates(void) {
    static int32_t last_update_id = 0;
    char url[512];
    
    snprintf(url, sizeof(url), "%s%s/getUpdates?offset=%ld&timeout=0", 
             TELEGRAM_API_URL, TELEGRAM_TOKEN, (long)(last_update_id + 1));

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        int content_length = esp_http_client_fetch_headers(client);
        if (content_length > 0) {
            char *buffer = malloc(content_length + 1);
            if (buffer) {
                int read_len = esp_http_client_read_response(client, buffer, content_length);
                buffer[read_len] = 0;

                cJSON *root = cJSON_Parse(buffer);
                if (root) {
                    cJSON *result = cJSON_GetObjectItem(root, "result");
                    if (cJSON_IsArray(result)) {
                        cJSON *update = NULL;
                        cJSON_ArrayForEach(update, result) {
                            cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
                            if (update_id) {
                                last_update_id = update_id->valueint;
                            }

                            cJSON *message = cJSON_GetObjectItem(update, "message");
                            if (message) {
                                cJSON *text = cJSON_GetObjectItem(message, "text");
                                if (text && cJSON_IsString(text)) {
                                    ESP_LOGI(TAG, "Received command: %s", text->valuestring);
                                    
                                    if (strncmp(text->valuestring, "/status", 7) == 0) {
                                        char status_msg[128];
                                        snprintf(status_msg, sizeof(status_msg), 
                                                 "Status:\nTemp: %.1f°C\nHum: %.1f%%\nRelay: %s", 
                                                 current_temp, current_hum, 
                                                 gpio_get_level(RELAY_GPIO) ? "ON" : "OFF");
                                        send_telegram_message(status_msg);
                                    } else if (strncmp(text->valuestring, "/relay", 6) == 0) {
                                        char relay_msg[64];
                                        snprintf(relay_msg, sizeof(relay_msg), "Relay is %s", 
                                                 gpio_get_level(RELAY_GPIO) ? "ON" : "OFF");
                                        send_telegram_message(relay_msg);
                                    }
                                }
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
                free(buffer);
            }
        }
    }
    esp_http_client_cleanup(client);
}

/**
 * @brief Tarea principal del Bot de Telegram
 * 
 * Ejecuta un bucle infinito que consulta periódicamente las actualizaciones
 * de Telegram para procesar comandos entrantes.
 * 
 * @param pvParameters Parámetros de la tarea (no utilizado)
 */
void telegram_bot_task(void *pvParameters) {
    ESP_LOGI(TAG, "Telegram Bot Task Started");
    while (1) {
        // Check for updates
        handle_telegram_updates();
        vTaskDelay(pdMS_TO_TICKS(2000)); // Poll every 2 seconds
    }
}

/**
 * @brief Tarea para leer datos del sensor DHT11
 *
 * Lee periódicamente la temperatura y humedad del sensor DHT11,
 * actualiza la pantalla OLED y envía los datos a los clientes WebSocket
 * conectados.
 *
 * @param pvParameters Parámetros de la tarea (no utilizado)
 *
 * @related_header
 * - dht.h
 * - ssd1306.h
 * - freertos/task.h
 */
void dht11_task(void *pvParameters) {
  char lineChar[20];

  // Configurar hardware
  ESP_LOGI(TAG, "Iniciando monitor DHT11 en GPIO %d", DHT_GPIO);

  // Inicializar pantalla
  ssd1306_clear_screen(&oled_dev, false);
  ssd1306_contrast(&oled_dev, 0xff);

  // Mostrar título grande centrado
  display_centered_text("DHT11", 0, true);

  // Mostrar dirección IP centrada
  display_centered_text(ip_address, 1, true);

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

  int display_counter = 0;

  while (1) {
    int16_t temperature, humidity;
    esp_err_t result = dht_read_data(DHT_TYPE_DHT11, (gpio_num_t)DHT_GPIO,
                                     &humidity, &temperature);

    if (result == ESP_OK) {
      float temp_c = temperature / 10.0;
      float hum_p = humidity / 10.0;
      
      current_temp = temp_c;
      current_hum = hum_p;

      // Actualizar Min/Max
      if (temp_c < min_temp) min_temp = temp_c;
      if (temp_c > max_temp) max_temp = temp_c;
      if (hum_p < min_hum) min_hum = hum_p;
      if (hum_p > max_hum) max_hum = hum_p;

      // Mostrar temperatura en la consola
      ESP_LOGI(TAG, "Temperatura: %.1f°C, Humedad: %.1f%%", temp_c, hum_p);

      // Control del relé basado en la temperatura
      if (temp_c > TEMP_THRESHOLD) {
        gpio_set_level(RELAY_GPIO, 1); // Enciende el relé
        
        // Iniciar parpadeo si no está activo
        if (blink_task_handle == NULL) {
          xTaskCreate(blink_led_task, "blink_led_task", 2048, NULL, 5, &blink_task_handle);
        }

        ESP_LOGI(TAG, "Temperatura alta (%.1f°C > %.1f°C) - Relé ACTIVADO",
                 temp_c, TEMP_THRESHOLD);
                 
        // Telegram Alert
        int64_t now = esp_timer_get_time() / 1000;
        if (now - last_alert_time > ALERT_COOLDOWN_MS) {
            char alert_msg[128];
            snprintf(alert_msg, sizeof(alert_msg), "⚠️ ALERTA: Temperatura Alta!\nValor: %.1f°C\nUmbral: %.1f°C", temp_c, TEMP_THRESHOLD);
            send_telegram_message(alert_msg);
            last_alert_time = now;
        }
      } else {
        gpio_set_level(RELAY_GPIO, 0); // Apaga el relé
        
        // Detener parpadeo si está activo
        if (blink_task_handle != NULL) {
          vTaskDelete(blink_task_handle);
          blink_task_handle = NULL;
          gpio_set_level(LED_GPIO, 0); // Asegurar que el LED quede apagado
        }
      }

      // Mostrar en la pantalla OLED (Alternar cada 3 ciclos)
      display_counter++;
      if (display_counter % 3 == 0) {
          // Mostrar Min/Max
          snprintf(lineChar, sizeof(lineChar), "Min:%.0f Max:%.0f", min_temp, max_temp);
          ssd1306_display_text(&oled_dev, 5, lineChar, strlen(lineChar), false);
          
          snprintf(lineChar, sizeof(lineChar), "m:%.0f M:%.0f %%", min_hum, max_hum);
          ssd1306_display_text(&oled_dev, 6, lineChar, strlen(lineChar), false);
      } else {
          // Mostrar Actual
          snprintf(lineChar, sizeof(lineChar), "Temp.: %.1f C", temp_c);
          ssd1306_display_text(&oled_dev, 5, lineChar, strlen(lineChar), false);

          snprintf(lineChar, sizeof(lineChar), "Hum.: %.1f %%", hum_p);
          ssd1306_display_text(&oled_dev, 6, lineChar, strlen(lineChar), false);
      }

      // Publicar datos MQTT (Incluyendo Min/Max)
      char mqtt_msg[128];
      snprintf(mqtt_msg, sizeof(mqtt_msg),
               "{\"temperatura\": %.1f, \"humedad\": %.1f, \"min_temp\": %.1f, \"max_temp\": %.1f}", 
               temp_c, hum_p, min_temp, max_temp);
      esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC, mqtt_msg, 0, 1, 0);

      // Mostrar mensaje de publicación exitosa
      ESP_LOGI(TAG, "Datos publicados en MQTT: %s", mqtt_msg);

      // Mostrar mensaje en pantalla OLED
      display_centered_text("Datos enviados", 7, true);
      vTaskDelay(2000 / portTICK_PERIOD_MS);

      // Enviar por WebSocket (Incluyendo Min/Max, estado del relé y límite)
      char json_msg[200]; // Aumentado el tamaño del buffer
      int relay_state = (temp_c > TEMP_THRESHOLD) ? 1 : 0;
      snprintf(json_msg, sizeof(json_msg), 
               "{\"temp\": %.1f, \"hum\": %.1f, \"min_t\": %.1f, \"max_t\": %.1f, \"relay\": %d, \"limit\": %.1f}",
               temp_c, hum_p, min_temp, max_temp, relay_state, TEMP_THRESHOLD);
      send_ws_message(json_msg);

    } else {
      ESP_LOGE(TAG, "Error lectura: %s", esp_err_to_name(result));
      ssd1306_display_text(&oled_dev, 5, "Error lectura", 13, false);
      ssd1306_display_text(&oled_dev, 6, "Revisa conexiones", 17, false);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS); // Lectura cada 5 segundos
  }
}

/**
 * @brief Punto de entrada principal de la aplicación
 *
 * Inicializa todos los componentes del sistema incluyendo:
 * - NVS (Almacenamiento no volátil)
 * - Sistema de archivos SPIFFS
 * - Conexión WiFi
 * - Servidor web con soporte WebSocket
 * - Tarea de lectura del sensor DHT11
 *
 * @note Esta función se ejecuta como una tarea de FreeRTOS. Al retornar, la
 * tarea se elimina automáticamente.
 *
 * @related_header
 * - nvs_flash.h
 * - freertos/task.h
 * - esp_log.h
 */
void app_main(void) {
  // Inicializar NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
    ESP_LOGE(TAG, "Error leyendo config WiFi, usando valores por defecto o "
                  "deteniendo...");
    // Podríamos detenernos aquí o intentar conectar con credenciales
    // hardcodeadas si se desea
  }

  // Conectar a WiFi
  wifi_init_sta();

  // Iniciar servidor web
  server = start_webserver();

  // Iniciar cliente MQTT
  mqtt_app_start();

  // Iniciar tarea DHT11 - MOVIDO AL FINAL PARA EVITAR DUPLICADOS
  // xTaskCreate(dht11_task, "dht11_task", 4096, NULL, 5, NULL);

#if CONFIG_I2C_INTERFACE
  ESP_LOGI(tag, "INTERFACE is i2c");
  ESP_LOGI(tag, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
  ESP_LOGI(tag, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
  ESP_LOGI(tag, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);
  i2c_master_init(&oled_dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO,
                  CONFIG_RESET_GPIO);
  ssd1306_init(&oled_dev, 128, 64);

  // Inicializar relé
  init_relay();
#endif // CONFIG_I2C_INTERFACE

  // #if CONFIG_FLIP
  // 	oled_dev._flip = true;
  // 	ESP_LOGW(tag, "Flip upside down");
  // #endif

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

  // Start Telegram Bot Task
  xTaskCreate(telegram_bot_task, "telegram_task", 8192, NULL, 5, NULL);
}