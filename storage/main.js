/**
 * Archivo: main.js
 * Descripción: Cliente WebSocket para recibir y mostrar datos del sensor DHT11 en tiempo real.
 *              Maneja la conexión, reconexión automática y actualización de la interfaz.
 * 
 * Funcionalidades:
 *  - Conexión WebSocket automática al cargar la página
 *  - Reconexión automática con backoff exponencial
 *  - Actualización de temperatura y humedad en tiempo real
 *  - Indicador visual del estado de conexión
 * 
 * Autor: migbertweb
 * Fecha: 21/11/2025
 * Proyecto: DHT11_Oled_Info
 */

// Referencias a los elementos DOM
const statusEl = document.getElementById('status');
const tempEl = document.getElementById('temp');
const humEl = document.getElementById('hum');
const relayStatusEl = document.getElementById('relay-status');
const relayCardEl = document.getElementById('relay-card');
const limitValEl = document.getElementById('limit-val');

// Configuración de la URL WebSocket
// Detecta automáticamente el protocolo (ws:// o wss://) según la página
const wsProtocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
const wsUrl = `${wsProtocol}${window.location.hostname}/ws`;

// Variables de estado de la conexión
let websocket;
let reconnectAttempts = 0;
const maxReconnectAttempts = 5;

/**
 * Inicializa y configura la conexión WebSocket
 * Maneja todos los eventos de la conexión: open, close, error, message
 */
function initWebSocket() {
    console.log('Intentando abrir conexión WebSocket a:', wsUrl);
    websocket = new WebSocket(wsUrl);

    /**
     * Evento: Conexión establecida exitosamente
     * Actualiza el indicador de estado y resetea el contador de reconexiones
     */
    websocket.onopen = (event) => {
        console.log('Conexión WebSocket abierta');
        statusEl.textContent = 'Conectado';
        statusEl.className = 'status connected';
        reconnectAttempts = 0; // Resetear el contador de reconexiones
    };

    /**
     * Evento: Conexión cerrada
     * Actualiza el indicador y programa una reconexión automática
     * Usa backoff exponencial para evitar saturar el servidor
     */
    websocket.onclose = (event) => {
        console.log('Conexión WebSocket cerrada');
        statusEl.textContent = 'Desconectado';
        statusEl.className = 'status disconnected';

        // Intentar reconectar después de un retraso (backoff exponencial)
        if (reconnectAttempts < maxReconnectAttempts) {
            reconnectAttempts++;
            const delay = 2000 * reconnectAttempts; // 2s, 4s, 6s, 8s, 10s
            console.log(`Reintentando conexión (${reconnectAttempts}/${maxReconnectAttempts}) en ${delay}ms...`);
            setTimeout(initWebSocket, delay);
        } else {
            console.error('Máximo número de intentos de reconexión alcanzado');
        }
    };

    /**
     * Evento: Error en la conexión WebSocket
     * Registra el error en la consola para depuración
     */
    websocket.onerror = (error) => {
        console.error('Error en WebSocket:', error);
    };

    /**
     * Evento: Mensaje recibido del servidor
     * Parsea el JSON y actualiza los valores de temperatura y humedad en la interfaz
     * Formato esperado: {"temp": 25.5, "hum": 65.0}
     */
    websocket.onmessage = (event) => {
        console.log('Mensaje recibido:', event.data);
        try {
            // Parsear el JSON recibido
            const data = JSON.parse(event.data);
            
            // Actualizar temperatura si está presente
            if (data.temp !== undefined) {
                tempEl.textContent = data.temp.toFixed(1);
            }
            
            // Actualizar humedad si está presente
            if (data.hum !== undefined) {
                humEl.textContent = data.hum.toFixed(1);
            }

            // Actualizar límite si está presente
            if (data.limit !== undefined) {
                limitValEl.textContent = data.limit.toFixed(1);
            }

            // Actualizar estado del relé
            if (data.relay !== undefined) {
                if (data.relay === 1) {
                    relayStatusEl.textContent = "ON";
                    relayCardEl.classList.add("active");
                } else {
                    relayStatusEl.textContent = "OFF";
                    relayCardEl.classList.remove("active");
                }
            }
        } catch (e) {
            console.error('Error al parsear JSON:', e);
        }
    };
}

// Iniciar la conexión WebSocket cuando la página se cargue completamente
window.addEventListener('load', initWebSocket);