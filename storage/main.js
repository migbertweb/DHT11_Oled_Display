const statusEl = document.getElementById('status');
const tempEl = document.getElementById('temp');
const humEl = document.getElementById('hum');

// Usar window.location.host para obtener el host actual
const wsProtocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';
const wsUrl = `${wsProtocol}${window.location.hostname}/ws`;
let websocket;
let reconnectAttempts = 0;
const maxReconnectAttempts = 5;

function initWebSocket() {
    console.log('Trying to open a WebSocket connection to:', wsUrl);
    websocket = new WebSocket(wsUrl);

    websocket.onopen = (event) => {
        console.log('WebSocket connection opened');
        statusEl.textContent = 'Conectado';
        statusEl.className = 'status connected';
        reconnectAttempts = 0; // Resetear el contador de reconexiones
    };

    websocket.onclose = (event) => {
        console.log('WebSocket connection closed');
        statusEl.textContent = 'Desconectado';
        statusEl.className = 'status disconnected';

        // Intentar reconectar después de un retraso
        if (reconnectAttempts < maxReconnectAttempts) {
            reconnectAttempts++;
            console.log(`Reintentando conexión (${reconnectAttempts}/${maxReconnectAttempts})...`);
            setTimeout(initWebSocket, 2000 * reconnectAttempts); // Aumentar el retraso
        } else {
            console.error('Máximo número de intentos de reconexión alcanzado');
        }
    };

    websocket.onerror = (error) => {
        console.error('WebSocket error:', error);
    };

    websocket.onmessage = (event) => {
        console.log('Message received:', event.data);
        try {
            const data = JSON.parse(event.data);
            if (data.temp !== undefined) tempEl.textContent = data.temp.toFixed(1);
            if (data.hum !== undefined) humEl.textContent = data.hum.toFixed(1);
        } catch (e) {
            console.error('Error parsing JSON:', e);
        }
    };
}

// Iniciar la conexión WebSocket cuando la página se cargue
window.addEventListener('load', initWebSocket);